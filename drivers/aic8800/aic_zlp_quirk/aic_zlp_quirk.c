// SPDX-License-Identifier: GPL-2.0

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/atomic.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/ptrace.h>
#include <linux/usb.h>

#define AIC_USB_VENDOR_ID	0x368b
#define AIC_USB_PRODUCT_ID	0x8d81
#define USB_BT_SUBCLASS		0x01
#define USB_BT_PROTOCOL		0x01

static atomic64_t injection_count = ATOMIC64_INIT(0);
static const char *hook_name = "none";

static int injections_get(char *buffer, const struct kernel_param *kp)
{
	(void)kp;

	return scnprintf(buffer, PAGE_SIZE, "%lld\n",
			 (long long)atomic64_read(&injection_count));
}

static const struct kernel_param_ops injections_ops = {
	.get = injections_get,
};

module_param_cb(injections, &injections_ops, NULL, 0444);
MODULE_PARM_DESC(injections, "Number of AIC ACL bulk OUT URBs modified");

static int hook_get(char *buffer, const struct kernel_param *kp)
{
	(void)kp;

	return scnprintf(buffer, PAGE_SIZE, "%s\n", hook_name);
}

static const struct kernel_param_ops hook_ops = {
	.get = hook_get,
};

module_param_cb(hook, &hook_ops, NULL, 0444);
MODULE_PARM_DESC(hook, "Active injection hook");

#if IS_ENABLED(CONFIG_KPROBES)

enum aic_zlp_hook {
	AIC_ZLP_HOOK_NONE,
	AIC_ZLP_HOOK_BTUSB_RETURN,
	AIC_ZLP_HOOK_USB_SUBMIT,
};

static enum aic_zlp_hook active_hook;

static bool is_aic_bulk_out(const struct urb *urb)
{
	const struct usb_device *udev;

	if (!urb)
		return false;

	udev = urb->dev;
	if (!udev)
		return false;

	if (le16_to_cpu(udev->descriptor.idVendor) != AIC_USB_VENDOR_ID ||
	    le16_to_cpu(udev->descriptor.idProduct) != AIC_USB_PRODUCT_ID)
		return false;

	return usb_pipetype(urb->pipe) == PIPE_BULK &&
	       usb_pipeout(urb->pipe);
}

static bool is_bluetooth_acl_endpoint(const struct urb *urb)
{
	const struct usb_endpoint_descriptor *ep;
	struct usb_host_interface *alt;
	struct usb_interface *intf;
	unsigned int endpoint;
	int i;

	if (!is_aic_bulk_out(urb))
		return false;

	intf = usb_ifnum_to_if(urb->dev, 0);
	if (!intf)
		return false;

	alt = READ_ONCE(intf->cur_altsetting);
	if (!alt ||
	    alt->desc.bInterfaceClass != USB_CLASS_WIRELESS_CONTROLLER ||
	    alt->desc.bInterfaceSubClass != USB_BT_SUBCLASS ||
	    alt->desc.bInterfaceProtocol != USB_BT_PROTOCOL)
		return false;

	endpoint = usb_pipeendpoint(urb->pipe);
	for (i = 0; i < alt->desc.bNumEndpoints; i++) {
		ep = &alt->endpoint[i].desc;
		if (usb_endpoint_is_bulk_out(ep) &&
		    usb_endpoint_num(ep) == endpoint)
			return true;
	}

	return false;
}

static void enable_zlp(struct urb *urb)
{
	long long count;

	if (urb->transfer_flags & URB_ZERO_PACKET)
		return;

	urb->transfer_flags |= URB_ZERO_PACKET;
	count = atomic64_inc_return(&injection_count);

	if (count == 1)
		pr_info("enabled ZLP on the first 368b:8d81 ACL bulk OUT URB\n");
}

static int alloc_bulk_urb_ret_handler(struct kretprobe_instance *ri,
				      struct pt_regs *regs)
{
	struct urb *urb;

	(void)ri;

	urb = (struct urb *)regs_return_value(regs);
	if (!is_aic_bulk_out(urb))
		return 0;

	enable_zlp(urb);
	return 0;
}

static struct kretprobe alloc_bulk_urb_probe = {
	.kp.symbol_name = "btusb:alloc_bulk_urb",
	.handler = alloc_bulk_urb_ret_handler,
};

static int usb_submit_urb_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct urb *urb;

	(void)p;

#if defined(CONFIG_ARM)
	urb = (struct urb *)regs->ARM_r0;
#else
	urb = (struct urb *)regs_get_kernel_argument(regs, 0);
#endif
	if (is_bluetooth_acl_endpoint(urb))
		enable_zlp(urb);

	return 0;
}

static struct kprobe usb_submit_urb_probe = {
	.symbol_name = "usb_submit_urb",
	.pre_handler = usb_submit_urb_pre_handler,
};

static int __init aic_zlp_quirk_init(void)
{
	int ret;

	ret = register_kretprobe(&alloc_bulk_urb_probe);
	if (!ret) {
		active_hook = AIC_ZLP_HOOK_BTUSB_RETURN;
		hook_name = "btusb:alloc_bulk_urb";
		pr_info("attached to system btusb for USB device 368b:8d81\n");
		return 0;
	}

	pr_warn("btusb return hook unavailable (%d), trying USB submit fallback\n",
		ret);

	ret = register_kprobe(&usb_submit_urb_probe);
	if (ret) {
		pr_err("cannot attach to usb_submit_urb: %d\n", ret);
		return ret;
	}

	active_hook = AIC_ZLP_HOOK_USB_SUBMIT;
	hook_name = "usb_submit_urb";
	pr_info("attached USB submit fallback for device 368b:8d81 interface 0\n");
	return 0;
}

static void __exit aic_zlp_quirk_exit(void)
{
	unsigned long missed = 0;

	if (active_hook == AIC_ZLP_HOOK_BTUSB_RETURN) {
		unregister_kretprobe(&alloc_bulk_urb_probe);
		missed = alloc_bulk_urb_probe.nmissed;
	} else if (active_hook == AIC_ZLP_HOOK_USB_SUBMIT) {
		unregister_kprobe(&usb_submit_urb_probe);
		missed = usb_submit_urb_probe.nmissed;
	}

	pr_info("detached %s after %lld injections (%lu missed hits)\n",
		hook_name, (long long)atomic64_read(&injection_count), missed);
}

#else

static int __init aic_zlp_quirk_init(void)
{
	pr_err("CONFIG_KPROBES is disabled in this kernel\n");
	return -EOPNOTSUPP;
}

static void __exit aic_zlp_quirk_exit(void)
{
}

#endif

module_init(aic_zlp_quirk_init);
module_exit(aic_zlp_quirk_exit);

MODULE_AUTHOR("Shen Mintao <cx330.shen@autocore.ai>");
MODULE_DESCRIPTION("AIC 8800D80 standard btusb ACL bulk TX ZLP quirk");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
MODULE_SOFTDEP("pre: btusb");
MODULE_ALIAS("usb:v368Bp8D81d*dc*dsc*dp*ic*isc*ip*in*");
