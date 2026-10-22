/**
 ******************************************************************************
 *
 * rwnx_cmds.c
 *
 * Handles queueing (push to IPC, ack/cfm from IPC) of commands issued to
 * LMAC FW
 *
 * Copyright (C) RivieraWaves 2014-2019
 *
 ******************************************************************************
 */

//#define CREATE_TRACE_POINTS
#include <linux/list.h>

#include "rwnx_cmds.h"
#include "rwnx_defs.h"
#include "rwnx_strs.h"
#include "rwnx_events.h"
#include "aicwf_txrxif.h"
#include "rwnx_wakelock.h"
#include "rwnx_msg_tx.h"
#ifdef AICWF_SDIO_SUPPORT
#include "aicwf_sdio.h"
#else
#include "aicwf_usb.h"
#endif
/**
 *
 */
extern int aicwf_sdio_writeb(struct aic_sdio_dev *sdiodev, uint regaddr, u8 val);

void rwnx_cmd_free(struct rwnx_cmd *cmd);

static void cmd_dump(const struct rwnx_cmd *cmd)
{
    printk(KERN_CRIT "tkn[%d]  flags:%04x  result:%3d  cmd:%4d-%-24s - reqcfm(%4d-%-s)\n",
           cmd->tkn, cmd->flags, cmd->result, cmd->id, RWNX_ID2STR(cmd->id),
           cmd->reqid, cmd->reqid != (lmac_msg_id_t)-1 ? RWNX_ID2STR(cmd->reqid) : "none");
}

#ifdef AICWF_USB_SUPPORT
/* Kept per USB port (bus and path) rather than per device: each reset brings a new usb_device. */
struct aicwf_port {
    int busnum;
    char devpath[16];
    unsigned long last_reset;
    bool reboot_on_probe;
};
static struct aicwf_port aicwf_ports[8];
static DEFINE_SPINLOCK(aicwf_ports_lock);

/* The port's entry, claimed if it has none; NULL when every entry is taken. */
static struct aicwf_port *aicwf_port_of(struct usb_device *udev)
{
    struct aicwf_port *unused = NULL;
    int i;

    lockdep_assert_held(&aicwf_ports_lock);
    for (i = 0; i < ARRAY_SIZE(aicwf_ports); i++) {
        struct aicwf_port *port = &aicwf_ports[i];

        if (port->busnum == udev->bus->busnum && !strcmp(port->devpath, udev->devpath))
            return port;
        if (!unused && !port->busnum)
            unused = port;
    }
    if (unused) {
        unused->busnum = udev->bus->busnum;
        strscpy(unused->devpath, udev->devpath, sizeof(unused->devpath));
    }
    return unused;
}

/* True once after cmd_mgr_reset_device() reset the device at udev's port. */
bool aicwf_reboot_chip_on_probe(struct usb_device *udev)
{
    struct aicwf_port *port;
    bool reboot = false;

    spin_lock_bh(&aicwf_ports_lock);
    port = aicwf_port_of(udev);
    if (port) {
        reboot = port->reboot_on_probe;
        port->reboot_on_probe = false;
    }
    spin_unlock_bh(&aicwf_ports_lock);
    return reboot;
}
#endif

/* Read-only: lets user space tell the driver's own reset from a fault, and wait for the device to come back. */
static unsigned int recoveries;
module_param(recoveries, uint, 0444);
MODULE_PARM_DESC(recoveries, "How many device resets have been asked for on a command timeout");
static long recovered_at;
module_param(recovered_at, long, 0444);
MODULE_PARM_DESC(recovered_at, "When the last reset was asked for, in seconds since boot as in /proc/uptime (0: none)");

/* The firmware's state on the DC and DW, read by debug reads: the kernel tasks' states in RAM (the ROM's task table)
 * and the MAC's state control. Read nothing else at a fault: a debug read of an unmapped address, or of a register
 * with side effects, can hang the chip past this recovery. */
#define DC_SM_STATE		0x00118648
#define DC_SCANU_STATE		0x0011868c
#define DC_SCAN_STATE		0x00116768
#define DC_ME_STATE		0x001184c8
#define DC_MM_STATE		0x00116404
#define DC_MAC_STATE		0x40320038
#define STATE_READ_MS		500

static const struct { u32 addr; const char *name; } fw_state_regs[] = {
    { DC_SM_STATE, "SM" }, { DC_SCANU_STATE, "SCANU" }, { DC_SCAN_STATE, "SCAN" }, { DC_ME_STATE, "ME" },
    { DC_MM_STATE, "MM" }, { DC_MAC_STATE, "MAC" },
};
static bool fw_state_reading;	/* a read not answered here is dropped, never a second reset */

static int fw_read_word(struct aic_usb_dev *usbdev, u32 addr, u32 *val)
{
    struct dbg_mem_read_cfm cfm;
    int err;

    memset(&cfm, 0, sizeof(cfm));
    err = rwnx_send_dbg_mem_read_req(usbdev->rwnx_hw, addr, &cfm);
    if (err)
        return err;
    /* With the bus down the send returns 0 and no answer: trust only an answer for this address. */
    if (cfm.memaddr != addr)
        return -EIO;
    *val = cfm.memdata;
    return 0;
}

/* At a command timeout, before the reset: the firmware's state in one line, stopping at the first read not answered
 * (the firmware's task loop has then stopped). */
static void cmd_mgr_log_fw_state(struct aic_usb_dev *usbdev, lmac_msg_id_t timed_out)
{
    char line[96];
    int len = 0, i, err = 0;
    u32 val;

    if (usbdev->chipid != PRODUCT_ID_AIC8800DC && usbdev->chipid != PRODUCT_ID_AIC8800DW)
        return;
    fw_state_reading = true;
    for (i = 0; i < ARRAY_SIZE(fw_state_regs); i++) {
        err = fw_read_word(usbdev, fw_state_regs[i].addr, &val);
        if (err)
            break;
        len += scnprintf(line + len, sizeof(line) - len, " %s %x", fw_state_regs[i].name,
                         fw_state_regs[i].addr >> 28 ? val : val & 0xff);
    }
    fw_state_reading = false;
    if (i < ARRAY_SIZE(fw_state_regs))
        len += scnprintf(line + len, sizeof(line) - len, " %s not answered (%d)", fw_state_regs[i].name, err);
    printk(KERN_CRIT "aic8800: firmware state after %s timed out:%s\n", RWNX_ID2STR(timed_out), line);
}

/* The stall watch (DC, DW), at the two points where a stalled firmware shows first: a join whose scan stays on the
 * access point's channel for JOIN_SCAN_STUCK_MS (a healthy scan leaves within its slot, ~100 ms), and a disconnect
 * unconfirmed after DISCONNECT_WATCH_MS (a healthy one comes in milliseconds). 0 off, 1 log, 2 reset at once. */
static int stall_watch = 1;
module_param(stall_watch, int, 0644);
MODULE_PARM_DESC(stall_watch, "Watch joins and disconnects for a stalled firmware (DC, DW): 0 off, 1 log, 2 reset at once");
#define JOIN_WATCH_MS		500
#define JOIN_SCAN_STUCK_MS	2000
#define JOIN_WATCH_MAX_MS	30000
#define DISCONNECT_WATCH_MS	1000
#define SCAN_WAIT_BEACON_PROBE_RSP	3

static void cmd_mgr_reset_device(struct rwnx_cmd_mgr *cmd_mgr);
static void join_watch_tick(struct work_struct *work);
static DECLARE_DELAYED_WORK(join_watch_work, join_watch_tick);
static struct aic_usb_dev *join_watch_dev;
static bool join_watch_on;
static unsigned long join_watch_since, join_scan_since;

static void join_watch_tick(struct work_struct *work)
{
    struct aic_usb_dev *usbdev = join_watch_dev;
    unsigned int ms = jiffies_to_msecs(jiffies - join_watch_since), stuck;
    u32 scan, sm;

    if (!join_watch_on || !usbdev)
        return;
    if (ms > JOIN_WATCH_MAX_MS) {
        join_watch_on = false;
        return;
    }
    /* Only with nothing else queued: a read behind a waiting command would wait with it. */
    if (usbdev->state != USB_UP_ST || usbdev->cmd_mgr.state == RWNX_CMD_MGR_STATE_CRASHED || usbdev->cmd_mgr.queue_sz)
        goto again;
    if (fw_read_word(usbdev, DC_SCAN_STATE, &scan) || fw_read_word(usbdev, DC_SM_STATE, &sm))
        goto again;
    if (!join_watch_on)
        return;		/* the connect ended meanwhile */
    if ((scan & 0xff) != SCAN_WAIT_BEACON_PROBE_RSP) {
        join_scan_since = 0;
        goto again;
    }
    if (!join_scan_since)
        join_scan_since = jiffies;
    stuck = jiffies_to_msecs(jiffies - join_scan_since);
    if (stuck < JOIN_SCAN_STUCK_MS)
        goto again;
    join_watch_on = false;
    printk(KERN_CRIT "aic8800: stall watch: a join's scan stuck on its channel for %u ms (SM %u)%s\n", stuck,
           sm & 0xff, stall_watch >= 2 ? ": resetting the device now" : "");
    if (stall_watch >= 2)
        cmd_mgr_reset_device(&usbdev->cmd_mgr);
    return;
again:
    if (join_watch_on)
        schedule_delayed_work(&join_watch_work, msecs_to_jiffies(JOIN_WATCH_MS));
}

/* A connect the firmware has taken (SM_CONNECT_CFM, status 0): watched until its indication. */
void aicwf_join_watch_start(struct rwnx_hw *rwnx_hw)
{
    struct aic_usb_dev *usbdev = rwnx_hw->usbdev;

    if (!stall_watch || !usbdev || (usbdev->chipid != PRODUCT_ID_AIC8800DC && usbdev->chipid != PRODUCT_ID_AIC8800DW))
        return;
    join_watch_dev = usbdev;
    join_watch_since = jiffies;
    join_scan_since = 0;
    join_watch_on = true;
    mod_delayed_work(system_wq, &join_watch_work, msecs_to_jiffies(JOIN_WATCH_MS));
}

/* From the receive path too: never waits for the work, whose reads are answered on that path. */
void aicwf_join_watch_stop(void)
{
    join_watch_on = false;
    cancel_delayed_work(&join_watch_work);
}

/* A command's wait for its confirm; for SM_DISCONNECT_REQ with the stall watch on, a checkpoint at
 * DISCONNECT_WATCH_MS. Nonzero: the confirm came (or the wait was interrupted, as before). */
static long cmd_mgr_wait(struct rwnx_cmd *cmd, unsigned long tout)
{
    unsigned long first = msecs_to_jiffies(DISCONNECT_WATCH_MS), waited = jiffies;
    long ret;

    if (cmd->id != SM_DISCONNECT_REQ || !stall_watch || tout <= first)
        return wait_for_completion_killable_timeout(&cmd->complete, tout);
    ret = wait_for_completion_killable_timeout(&cmd->complete, first);
    if (ret)
        return ret;
    printk(KERN_CRIT "aic8800: stall watch: no SM_DISCONNECT_CFM in %d ms%s\n", DISCONNECT_WATCH_MS,
           stall_watch >= 2 ? ": resetting the device now" : "");
    if (stall_watch >= 2)
        return 0;
    ret = wait_for_completion_killable_timeout(&cmd->complete, tout - first);
    if (ret)
        printk(KERN_CRIT "aic8800: stall watch: SM_DISCONNECT_CFM after %u ms: acting at %d ms would have reset a "
               "working chip\n", jiffies_to_msecs(jiffies - waited), DISCONNECT_WATCH_MS);
    return ret;
}

#define RESET_INTERVAL	(30 * HZ)

/* The firmware stopped answering: reset the USB device, so the driver probes again and loads the firmware afresh.
 * At most once per RESET_INTERVAL on each port; the commands refused meanwhile ask again. */
static void cmd_mgr_reset_device(struct rwnx_cmd_mgr *cmd_mgr)
{
#ifdef AICWF_USB_SUPPORT
    struct aic_usb_dev *usbdev = container_of(cmd_mgr, struct aic_usb_dev, cmd_mgr);
    struct aicwf_port *port;

    if (!usbdev->intf)
        return;
    spin_lock_bh(&aicwf_ports_lock);
    port = aicwf_port_of(usbdev->udev);
    if (port && port->last_reset && time_before(jiffies, port->last_reset + RESET_INTERVAL)) {
        spin_unlock_bh(&aicwf_ports_lock);
        return;
    }
    if (port) {
        port->last_reset = jiffies;
        port->reboot_on_probe = true;
    }
    spin_unlock_bh(&aicwf_ports_lock);
    recoveries++;
    recovered_at = (long)ktime_get_boottime_seconds();
    printk(KERN_CRIT "aic8800: firmware not answering: resetting the device to load it afresh\n");
    usb_queue_reset_device(usbdev->intf);
#endif
}

/**
 *
 */
static void cmd_complete(struct rwnx_cmd_mgr *cmd_mgr, struct rwnx_cmd *cmd)
{
    //RWNX_DBG(RWNX_FN_ENTRY_STR);
    lockdep_assert_held(&cmd_mgr->lock);

    //list_del(&cmd->list);
    //cmd_mgr->queue_sz--;

    cmd->flags |= RWNX_CMD_FLAG_DONE;
    if (cmd->flags & RWNX_CMD_FLAG_NONBLOCK) {
        rwnx_cmd_free(cmd);//kfree(cmd);AIDEN
    } else {
        if (RWNX_CMD_WAIT_COMPLETE(cmd->flags)) {
            cmd->result = 0;
            complete(&cmd->complete);
        }
    }
    
    if(cmd_mgr->queue_sz == 0){
        rwnx_wakeup_unlock(g_rwnx_plat->usbdev->rwnx_hw->ws_tx);
    }
}

int cmd_mgr_queue_force_defer(struct rwnx_cmd_mgr *cmd_mgr, struct rwnx_cmd *cmd)
{
    bool defer_push = false;

    RWNX_DBG(RWNX_FN_ENTRY_STR);
#ifdef CREATE_TRACE_POINTS
    trace_msg_send(cmd->id);
#endif
    spin_lock_bh(&cmd_mgr->lock);

    if (cmd_mgr->state == RWNX_CMD_MGR_STATE_CRASHED) {
        printk(KERN_CRIT"cmd queue crashed\n");
        cmd->result = -EPIPE;
        spin_unlock_bh(&cmd_mgr->lock);
        cmd_mgr_reset_device(cmd_mgr);
        return -EPIPE;
    }

    #ifndef CONFIG_RWNX_FHOST
    if (!list_empty(&cmd_mgr->cmds)) {
        if (cmd_mgr->queue_sz == cmd_mgr->max_queue_sz) {
            printk(KERN_CRIT"Too many cmds (%d) already queued\n",
                   cmd_mgr->max_queue_sz);
            cmd->result = -ENOMEM;
            spin_unlock_bh(&cmd_mgr->lock);
            return -ENOMEM;
        }
    }
    #endif

    cmd->flags |= RWNX_CMD_FLAG_WAIT_PUSH;
    defer_push = true;

    if (cmd->flags & RWNX_CMD_FLAG_REQ_CFM)
        cmd->flags |= RWNX_CMD_FLAG_WAIT_CFM;

    cmd->tkn    = cmd_mgr->next_tkn++;
    cmd->result = -EINTR;

    if (!(cmd->flags & RWNX_CMD_FLAG_NONBLOCK))
        init_completion(&cmd->complete);

    list_add_tail(&cmd->list, &cmd_mgr->cmds);
    if(cmd_mgr->queue_sz == 0){
        rwnx_wakeup_lock(g_rwnx_plat->usbdev->rwnx_hw->ws_tx);
    }
    cmd_mgr->queue_sz++;
    spin_unlock_bh(&cmd_mgr->lock);

    WAKE_CMD_WORK(cmd_mgr);
    return 0;
}

void rwnx_msg_free_(struct lmac_msg *msg);


static int cmd_mgr_queue(struct rwnx_cmd_mgr *cmd_mgr, struct rwnx_cmd *cmd)
{
	int ret = 0;
#ifdef AICWF_SDIO_SUPPORT
    struct aic_sdio_dev *sdiodev = container_of(cmd_mgr, struct aic_sdio_dev, cmd_mgr);
#endif
#ifdef AICWF_USB_SUPPORT
	
    struct aic_usb_dev *usbdev = container_of(cmd_mgr, struct aic_usb_dev, cmd_mgr);
#endif
    bool defer_push = false;
    u8_l empty = 0;

    //RWNX_DBG(RWNX_FN_ENTRY_STR);
#ifdef CREATE_TRACE_POINTS
    trace_msg_send(cmd->id);
#endif
    if(cmd->e2a_msg != NULL) {
        do {
			spin_lock_bh(&cmd_mgr->lock);
			if(cmd_mgr->state == RWNX_CMD_MGR_STATE_CRASHED) {
				AICWFDBG(LOGERROR, "cmd queue crashed\n");
				cmd->result = -EPIPE;
				spin_unlock_bh(&cmd_mgr->lock);
				cmd_mgr_reset_device(cmd_mgr);
				return -EPIPE;
			}
            empty = list_empty(&cmd_mgr->cmds);
            if(!empty) {
                spin_unlock_bh(&cmd_mgr->lock);
                if(in_softirq()) {
                    printk("in_softirq:check cmdqueue empty\n");
                    mdelay(10);
                } else {
                    printk("check cmdqueue empty\n");
                    msleep(50);
                }
            }
        } while(!empty);//wait for cmd queue empty
    } else {
			spin_lock_bh(&cmd_mgr->lock);
			if (cmd_mgr->state == RWNX_CMD_MGR_STATE_CRASHED) {
				printk(KERN_CRIT"cmd queue crashed\n");
				cmd->result = -EPIPE;
				spin_unlock_bh(&cmd_mgr->lock);
				cmd_mgr_reset_device(cmd_mgr);
				return -EPIPE;
		}
	}

    #ifndef CONFIG_RWNX_FHOST
    if (!list_empty(&cmd_mgr->cmds)) {
        struct rwnx_cmd *last;

        if (cmd_mgr->queue_sz == cmd_mgr->max_queue_sz) {
            printk(KERN_CRIT"Too many cmds (%d) already queued\n",
                   cmd_mgr->max_queue_sz);
            cmd->result = -ENOMEM;
            spin_unlock_bh(&cmd_mgr->lock);
            return -ENOMEM;
        }
        last = list_entry(cmd_mgr->cmds.prev, struct rwnx_cmd, list);
        if (last->flags & (RWNX_CMD_FLAG_WAIT_ACK | RWNX_CMD_FLAG_WAIT_PUSH | RWNX_CMD_FLAG_WAIT_CFM)) {
#if 0 // queue even NONBLOCK command.
            if (cmd->flags & RWNX_CMD_FLAG_NONBLOCK) {
                printk(KERN_CRIT"cmd queue busy\n");
                cmd->result = -EBUSY;
                spin_unlock_bh(&cmd_mgr->lock);
                return -EBUSY;
            }
#endif
            cmd->flags |= RWNX_CMD_FLAG_WAIT_PUSH;
            defer_push = true;
        }
    }
    #endif

#if 0
    cmd->flags |= RWNX_CMD_FLAG_WAIT_ACK;
#endif
    if (cmd->flags & RWNX_CMD_FLAG_REQ_CFM)
        cmd->flags |= RWNX_CMD_FLAG_WAIT_CFM;

    cmd->tkn    = cmd_mgr->next_tkn++;
    cmd->result = -EINTR;

    if (!(cmd->flags & RWNX_CMD_FLAG_NONBLOCK))
        init_completion(&cmd->complete);

    list_add_tail(&cmd->list, &cmd_mgr->cmds);
    if(cmd_mgr->queue_sz == 0){
        rwnx_wakeup_lock(g_rwnx_plat->usbdev->rwnx_hw->ws_tx);
    }
    cmd_mgr->queue_sz++;

	if(cmd->a2e_msg->id == ME_TRAFFIC_IND_REQ
	#ifdef AICWF_ARP_OFFLOAD
		|| cmd->a2e_msg->id == MM_SET_ARPOFFLOAD_REQ
	#endif
	) {
		defer_push = true;
		cmd->flags |= RWNX_CMD_FLAG_WAIT_PUSH;
		//printk("defer push: tkn=%d\r\n", cmd->tkn);
	}

	//spin_unlock_bh(&cmd_mgr->lock);
    if (!defer_push) {
		spin_unlock_bh(&cmd_mgr->lock);
		AICWFDBG(LOGTRACE, "queue:id=%x, param_len=%u\n",cmd->a2e_msg->id, cmd->a2e_msg->param_len);

        #ifdef AICWF_SDIO_SUPPORT
        aicwf_set_cmd_tx((void *)(sdiodev), cmd->a2e_msg, sizeof(struct lmac_msg) + cmd->a2e_msg->param_len);
        #else
        aicwf_set_cmd_tx((void *)(usbdev), cmd->a2e_msg, sizeof(struct lmac_msg) + cmd->a2e_msg->param_len);
        #endif
        //rwnx_ipc_msg_push(rwnx_hw, cmd, RWNX_CMD_A2EMSG_LEN(cmd->a2e_msg));

		kfree(cmd->a2e_msg);
    } else {
		if(cmd_mgr->queue_sz <= 1) {
			spin_unlock_bh(&cmd_mgr->lock);
			WAKE_CMD_WORK(cmd_mgr);
		} else {
			spin_unlock_bh(&cmd_mgr->lock);
		}
		return 0;
	}

    if (!(cmd->flags & RWNX_CMD_FLAG_NONBLOCK)) {
        #ifdef CONFIG_RWNX_FHOST
        if (wait_for_completion_killable(&cmd->complete)) {
            cmd->result = -EINTR;
            spin_lock_bh(&cmd_mgr->lock);
            cmd_complete(cmd_mgr, cmd);
            spin_unlock_bh(&cmd_mgr->lock);
            /* TODO: kill the cmd at fw level */
        }
        #else
        bool state_read = fw_state_reading && cmd->id == DBG_MEM_READ_REQ;
        bool empty;
        unsigned long tout = msecs_to_jiffies(state_read ? STATE_READ_MS : RWNX_80211_CMD_TIMEOUT_MS/*AIDEN workaround* cmd_mgr->queue_sz*/);
        if (!cmd_mgr_wait(cmd, tout)) {
            if (state_read) {
                /* A state read not answered: dropped; the reset after the reads does the rest. */
                spin_lock_bh(&cmd_mgr->lock);
                list_del(&cmd->list);
                cmd_mgr->queue_sz--;
                if (!(cmd->flags & RWNX_CMD_FLAG_DONE)) {
                    cmd->result = -ETIMEDOUT;
                    cmd_complete(cmd_mgr, cmd);
                }
                spin_unlock_bh(&cmd_mgr->lock);
                return -ETIMEDOUT;
            }
            printk(KERN_CRIT"%s cmd timed-out cmd_mgr->queue_sz:%d\n", __func__,cmd_mgr->queue_sz);
            /* Off the queue, and not yet marked crashed, so that the state reads go straight out. */
            spin_lock_bh(&cmd_mgr->lock);
            list_del(&cmd->list);
            cmd_mgr->queue_sz--;
            empty = cmd_mgr->queue_sz == 0;
            spin_unlock_bh(&cmd_mgr->lock);
            if (empty)
                cmd_mgr_log_fw_state(usbdev, cmd->id);
        #ifdef AICWF_SDIO_SUPPORT
            ret = aicwf_sdio_writeb(sdiodev, SDIOWIFI_WAKEUP_REG, 2);
            if (ret < 0) {
                sdio_err("reg:%d write failed!\n", SDIOWIFI_WAKEUP_REG);
            }
        #endif

            cmd_dump(cmd);
            spin_lock_bh(&cmd_mgr->lock);
            cmd_mgr->state = RWNX_CMD_MGR_STATE_CRASHED;
            if (cmd_mgr->queue_sz == 0)
                rwnx_wakeup_unlock(usbdev->rwnx_hw->ws_tx);
            if (!(cmd->flags & RWNX_CMD_FLAG_DONE)) {
                cmd->result = -ETIMEDOUT;
                cmd_complete(cmd_mgr, cmd);
            }
			ret = -ETIMEDOUT;
            spin_unlock_bh(&cmd_mgr->lock);
            cmd_mgr_reset_device(cmd_mgr);
        }
		else{
			spin_lock_bh(&cmd_mgr->lock);
			list_del(&cmd->list);
			cmd_mgr->queue_sz--;
			if(cmd_mgr->queue_sz == 0){
                rwnx_wakeup_unlock(usbdev->rwnx_hw->ws_tx);
            }
			spin_unlock_bh(&cmd_mgr->lock);
			rwnx_cmd_free(cmd);//kfree(cmd);AIDEN
            if(!list_empty(&cmd_mgr->cmds) && usbdev->state == USB_UP_ST)
                WAKE_CMD_WORK(cmd_mgr);
		}
        #endif
    } else {
        cmd->result = 0;
    }
    return ret;
}

/**
 *
 */
static int cmd_mgr_llind(struct rwnx_cmd_mgr *cmd_mgr, struct rwnx_cmd *cmd)
{
    struct rwnx_cmd *cur, *acked = NULL, *next = NULL;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    spin_lock_bh(&cmd_mgr->lock);
    list_for_each_entry(cur, &cmd_mgr->cmds, list) {
        if (!acked) {
            if (cur->tkn == cmd->tkn) {
                if (WARN_ON_ONCE(cur != cmd)) {
                    cmd_dump(cmd);
                }
                acked = cur;
                continue;
            }
        }
        if (cur->flags & RWNX_CMD_FLAG_WAIT_PUSH) {
                next = cur;
                break;
        }
    }
    if (!acked) {
        printk(KERN_CRIT "Error: acked cmd not found\n");
    } else {
        cmd->flags &= ~RWNX_CMD_FLAG_WAIT_ACK;
        if (RWNX_CMD_WAIT_COMPLETE(cmd->flags))
            cmd_complete(cmd_mgr, cmd);
    }

    if (next) {
	#if 0 //there is no ack
        struct rwnx_hw *rwnx_hw = container_of(cmd_mgr, struct rwnx_hw, cmd_mgr);
        next->flags &= ~RWNX_CMD_FLAG_WAIT_PUSH;
        rwnx_ipc_msg_push(rwnx_hw, next, RWNX_CMD_A2EMSG_LEN(next->a2e_msg));
        kfree(next->a2e_msg);
	#endif
    }
    spin_unlock(&cmd_mgr->lock);

    return 0;
}

void cmd_mgr_task_process(struct work_struct *work)
{
    struct rwnx_cmd_mgr *cmd_mgr = container_of(work, struct rwnx_cmd_mgr, cmdWork);
    struct rwnx_cmd *cur, *next = NULL;
    unsigned long tout;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    while(1) {
        next = NULL;
        spin_lock_bh(&cmd_mgr->lock);

        list_for_each_entry(cur, &cmd_mgr->cmds, list) {
            if (cur->flags & RWNX_CMD_FLAG_WAIT_PUSH) { //just judge the first
                    next = cur;
            }
            break;
        }
        spin_unlock_bh(&cmd_mgr->lock);

        if(next == NULL)
            break;

        if (next) {
 	    #ifdef AICWF_SDIO_SUPPORT
            struct aic_sdio_dev *sdiodev = container_of(cmd_mgr, struct aic_sdio_dev, cmd_mgr);
	    #endif
	    #ifdef AICWF_USB_SUPPORT
    	    struct aic_usb_dev *usbdev = container_of(cmd_mgr, struct aic_usb_dev, cmd_mgr);
	    #endif
            next->flags &= ~RWNX_CMD_FLAG_WAIT_PUSH;

            //printk("cmd_process, cmd->id=%d, tkn=%d\r\n",next->reqid, next->tkn);
            //rwnx_ipc_msg_push(rwnx_hw, next, RWNX_CMD_A2EMSG_LEN(next->a2e_msg));
#ifdef AICWF_SDIO_SUPPORT
            aicwf_set_cmd_tx((void *)(sdiodev), next->a2e_msg, sizeof(struct lmac_msg) + next->a2e_msg->param_len);
#else
            aicwf_set_cmd_tx((void *)(usbdev), next->a2e_msg, sizeof(struct lmac_msg) + next->a2e_msg->param_len);
#endif
            kfree(next->a2e_msg);

            tout = msecs_to_jiffies(RWNX_80211_CMD_TIMEOUT_MS * cmd_mgr->queue_sz);
            if (!wait_for_completion_killable_timeout(&next->complete, tout)) {
                bool empty;

                printk(KERN_CRIT"%s cmd timed-out cmd_mgr->queue_sz:%d\n", __func__, cmd_mgr->queue_sz);
                cmd_dump(next);
                /* As in cmd_mgr_queue(): off the queue, the state read, then marked crashed. */
                spin_lock_bh(&cmd_mgr->lock);
                list_del(&next->list);
                cmd_mgr->queue_sz--;
                empty = cmd_mgr->queue_sz == 0;
                spin_unlock_bh(&cmd_mgr->lock);
                if (empty)
                    cmd_mgr_log_fw_state(usbdev, next->id);
                spin_lock_bh(&cmd_mgr->lock);
                cmd_mgr->state = RWNX_CMD_MGR_STATE_CRASHED;
                if (cmd_mgr->queue_sz == 0)
                    rwnx_wakeup_unlock(usbdev->rwnx_hw->ws_tx);
                spin_unlock_bh(&cmd_mgr->lock);
                rwnx_cmd_free(next);
                cmd_mgr_reset_device(cmd_mgr);
            } else {
				spin_lock_bh(&cmd_mgr->lock);
				list_del(&next->list);
				cmd_mgr->queue_sz--;
				if(cmd_mgr->queue_sz == 0){
                    rwnx_wakeup_unlock(usbdev->rwnx_hw->ws_tx);
                }
				spin_unlock_bh(&cmd_mgr->lock);
				rwnx_cmd_free(next);//kfree(next);AIDEN
			}
        }
    }

}


static int cmd_mgr_run_callback(struct rwnx_hw *rwnx_hw, struct rwnx_cmd *cmd,
                                struct rwnx_cmd_e2amsg *msg, msg_cb_fct cb)
{
    int res;

    if (! cb){
        return 0;
    }
    //RWNX_DBG(RWNX_FN_ENTRY_STR);
#ifndef CONFIG_DEBUG_ATOMIC_SLEEP
	//spin_lock_bh(&rwnx_hw->cb_lock);
#endif
    res = cb(rwnx_hw, cmd, msg);
#ifndef CONFIG_DEBUG_ATOMIC_SLEEP
	//spin_unlock_bh(&rwnx_hw->cb_lock);
#endif

    return res;
}

/**
 *

 */
static int cmd_mgr_msgind(struct rwnx_cmd_mgr *cmd_mgr, struct rwnx_cmd_e2amsg *msg,
                          msg_cb_fct cb)
{
#ifdef AICWF_SDIO_SUPPORT
    struct aic_sdio_dev *sdiodev = container_of(cmd_mgr, struct aic_sdio_dev, cmd_mgr);
    struct rwnx_hw *rwnx_hw = sdiodev->rwnx_hw;
#endif
#ifdef AICWF_USB_SUPPORT
    struct aic_usb_dev *usbdev = container_of(cmd_mgr, struct aic_usb_dev, cmd_mgr);
    struct rwnx_hw *rwnx_hw = usbdev->rwnx_hw;
#endif
    struct rwnx_cmd *cmd, *pos;
    bool found = false;

   // RWNX_DBG(RWNX_FN_ENTRY_STR);
#ifdef CREATE_TRACE_POINTS
    trace_msg_recv(msg->id);
#endif
    AICWFDBG(LOGTRACE, "%s cmd->id=%d\n", __func__, msg->id);
    spin_lock_bh(&cmd_mgr->lock);
    list_for_each_entry_safe(cmd, pos, &cmd_mgr->cmds, list) {
        if (cmd->reqid == msg->id &&
            (cmd->flags & RWNX_CMD_FLAG_WAIT_CFM)) {

            if (!cmd_mgr_run_callback(rwnx_hw, cmd, msg, cb)) {
                found = true;
                cmd->flags &= ~RWNX_CMD_FLAG_WAIT_CFM;

                if (WARN((msg->param_len > RWNX_CMD_E2AMSG_LEN_MAX),
                         "Unexpect E2A msg len %d > %d\n", msg->param_len,
                         RWNX_CMD_E2AMSG_LEN_MAX)) {
                    msg->param_len = RWNX_CMD_E2AMSG_LEN_MAX;
                }

                if (cmd->e2a_msg && msg->param_len)
                    memcpy(cmd->e2a_msg, &msg->param, msg->param_len);

                if (RWNX_CMD_WAIT_COMPLETE(cmd->flags))
                    cmd_complete(cmd_mgr, cmd);

                break;
            }
        }
    }
    spin_unlock_bh(&cmd_mgr->lock);

    if (!found)
        cmd_mgr_run_callback(rwnx_hw, NULL, msg, cb);

    return 0;
}

/**
 *
 */
static void cmd_mgr_print(struct rwnx_cmd_mgr *cmd_mgr)
{
    struct rwnx_cmd *cur;

    spin_lock_bh(&cmd_mgr->lock);
    RWNX_DBG("q_sz/max: %2d / %2d - next tkn: %d\n",
             cmd_mgr->queue_sz, cmd_mgr->max_queue_sz,
             cmd_mgr->next_tkn);
    list_for_each_entry(cur, &cmd_mgr->cmds, list) {
        cmd_dump(cur);
    }
    spin_unlock_bh(&cmd_mgr->lock);
}

static void cmd_mgr_drain(struct rwnx_cmd_mgr *cmd_mgr)
{
    struct rwnx_cmd *cur, *nxt;

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    spin_lock_bh(&cmd_mgr->lock);
    list_for_each_entry_safe(cur, nxt, &cmd_mgr->cmds, list) {
        list_del(&cur->list);
        //cmd_mgr->queue_sz--;
        if (!(cur->flags & RWNX_CMD_FLAG_NONBLOCK))
            complete(&cur->complete);
    }
    spin_unlock_bh(&cmd_mgr->lock);
    #if 0
    if(cmd_mgr->queue_sz == 0){
        rwnx_wakeup_unlock(g_rwnx_plat->usbdev->rwnx_hw->ws_tx);
    }
	#endif

}

void rwnx_cmd_mgr_init(struct rwnx_cmd_mgr *cmd_mgr)
{
    RWNX_DBG(RWNX_FN_ENTRY_STR);

    INIT_LIST_HEAD(&cmd_mgr->cmds);
	cmd_mgr->state = RWNX_CMD_MGR_STATE_INITED;
    spin_lock_init(&cmd_mgr->lock);
    cmd_mgr->max_queue_sz = RWNX_CMD_MAX_QUEUED;
    cmd_mgr->queue  = &cmd_mgr_queue;
    cmd_mgr->print  = &cmd_mgr_print;
    cmd_mgr->drain  = &cmd_mgr_drain;
    cmd_mgr->llind  = &cmd_mgr_llind;
    cmd_mgr->msgind = &cmd_mgr_msgind;

    INIT_WORK(&cmd_mgr->cmdWork, cmd_mgr_task_process);
    cmd_mgr->cmd_wq = create_singlethread_workqueue("cmd_wq");
    if (!cmd_mgr->cmd_wq) {
        txrx_err("insufficient memory to create cmd workqueue.\n");
        return;
    }
}

void rwnx_cmd_mgr_deinit(struct rwnx_cmd_mgr *cmd_mgr)
{
    join_watch_on = false;	/* no new reads; the work is waited for once the queue is drained */
    if(cmd_mgr->print && cmd_mgr->drain){
        cmd_mgr->print(cmd_mgr);
        cmd_mgr->drain(cmd_mgr);
        cancel_delayed_work_sync(&join_watch_work);
        join_watch_dev = NULL;
        cmd_mgr->print(cmd_mgr);
        flush_workqueue(cmd_mgr->cmd_wq);
        destroy_workqueue(cmd_mgr->cmd_wq);
        memset(cmd_mgr, 0, sizeof(*cmd_mgr));
    }
}


void aicwf_set_cmd_tx(void *dev, struct lmac_msg *msg, uint len)
{
    u8 *buffer = NULL;
    u16 index = 0;
#ifdef AICWF_SDIO_SUPPORT
	struct aic_sdio_dev *sdiodev = (struct aic_sdio_dev *)dev;
    struct aicwf_bus *bus = sdiodev->bus_if;
#else
	struct aic_usb_dev *usbdev = (struct aic_usb_dev *)dev;
	struct aicwf_bus *bus = NULL;
    if (!usbdev->state) {
        printk("down msg \n");
        return;
    }
	bus = usbdev->bus_if;
#endif
    buffer = bus->cmd_buf;

    memset(buffer, 0, CMD_BUF_MAX);
    buffer[0] = (len+4) & 0x00ff;
    buffer[1] = ((len+4) >> 8) &0x0f;
    buffer[2] = 0x11;
    buffer[3] = 0x0;
    index += 4;
    //there is a dummy word
    index += 4;

	//make sure little endian
    put_u16(&buffer[index], msg->id);
    index += 2;
    put_u16(&buffer[index], msg->dest_id);
    index += 2;
    put_u16(&buffer[index], msg->src_id);
    index += 2;
    put_u16(&buffer[index], msg->param_len);
    index += 2;
    memcpy(&buffer[index], (u8 *)msg->param, msg->param_len);

    aicwf_bus_txmsg(bus, buffer, len + 8);
}

