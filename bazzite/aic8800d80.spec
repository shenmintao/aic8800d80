%global commit 51b7b6e72989afe4d21f52e55b70f5a4d6b21e5b
%global shortcommit %(echo %{commit} | cut -c1-7)
%{!?kver:%global kver %(uname -r)}

Name:           aic8800d80
Version:        %{shortcommit}
Release:        1%{?dist}
Summary:        AIC8800 USB Wi-Fi, Bluetooth firmware, and ZLP quirk driver

License:        GPL-2.0-only
URL:            https://github.com/shenmintao/aic8800d80
Source0:        %{url}/archive/%{commit}/%{name}-%{commit}.tar.gz

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  kernel-devel-uname-r = %{kver}

Requires:       kernel-uname-r = %{kver}
Requires:       /usr/bin/eject
Requires:       /usr/sbin/usb_modeswitch
Requires(post): /usr/sbin/depmod
Requires(postun): /usr/sbin/depmod

%description
Out-of-tree AIC8800 USB driver with Wi-Fi support, Bluetooth firmware loading,
udev mode-switch rules, and all firmware variants shipped by the upstream
repository. Combo adapters use the standard Linux btusb transport driver. A
device-scoped companion module supplies the required ACL bulk TX ZLP behavior
for USB device 368b:8d81 without replacing the distribution btusb module.

%prep
%autosetup -n %{name}-%{commit}

%build
make -C drivers/aic8800 KVER=%{kver} KDIR=/usr/src/kernels/%{kver} clean
make -C drivers/aic8800 KVER=%{kver} KDIR=/usr/src/kernels/%{kver}

%install
rm -rf %{buildroot}

install -Dpm0644 \
  drivers/aic8800/aic8800_fdrv/aic8800_fdrv.ko \
  %{buildroot}/usr/lib/modules/%{kver}/kernel/drivers/net/wireless/aic8800/aic8800_fdrv.ko

install -Dpm0644 \
  drivers/aic8800/aic_load_fw/aic_load_fw.ko \
  %{buildroot}/usr/lib/modules/%{kver}/kernel/drivers/net/wireless/aic8800/aic_load_fw.ko

install -Dpm0644 \
  drivers/aic8800/aic_zlp_quirk/aic_zlp_quirk.ko \
  %{buildroot}/usr/lib/modules/%{kver}/kernel/drivers/bluetooth/aic8800/aic_zlp_quirk.ko

install -Dpm0644 \
  aic.rules \
  %{buildroot}/usr/lib/udev/rules.d/90-aic8800-mode-switch.rules

install -Dpm0644 \
  usb_modeswitch/1111_1111 \
  %{buildroot}/etc/usb_modeswitch.d/1111:1111

install -d -m 0755 %{buildroot}/usr/lib/firmware
cp -a fw/aic8800* %{buildroot}/usr/lib/firmware/

%post
/usr/sbin/depmod -a %{kver} || :

%postun
/usr/sbin/depmod -a %{kver} || :

%files
%dir /usr/lib/modules/%{kver}/kernel/drivers/net/wireless/aic8800
/usr/lib/modules/%{kver}/kernel/drivers/net/wireless/aic8800/aic8800_fdrv.ko
/usr/lib/modules/%{kver}/kernel/drivers/net/wireless/aic8800/aic_load_fw.ko
/usr/lib/modules/%{kver}/kernel/drivers/bluetooth/aic8800/aic_zlp_quirk.ko
/usr/lib/udev/rules.d/90-aic8800-mode-switch.rules
%config(noreplace) /etc/usb_modeswitch.d/1111:1111
/usr/lib/firmware/aic8800*

%changelog
* Sat Jul 25 2026 Shen Mintao <shenmintao@gmail.com> - 51b7b6e-1
- Package the unified installer and ZLP quirk with the legacy MCU1 profiles.
- Preserve the matched D80 and DC/DW firmware and loader combinations.

* Sat Jul 25 2026 Shen Mintao <shenmintao@gmail.com> - 13baa9b-1
- Unify Wi-Fi and standard-btusb Bluetooth support in one package.
- Add the device-scoped 368b:8d81 ACL bulk TX ZLP companion module.
- Clean up obsolete aic_btusb and issue #63 diagnostic installations.

* Fri Jul 24 2026 Shen Mintao <shenmintao@gmail.com> - 88dbc0a-1
- Use the unified Wi-Fi and Bluetooth installer/package description.
- Keep Bluetooth transport on the standard Linux btusb driver.

* Tue Jul 14 2026 Shen Mintao <shenmintao@gmail.com> - d10bc52-1
- Build the current main branch without the legacy custom Bluetooth module.
- Package all firmware variants and current mode-switch rules.
- Remove obsolete Bluetooth-specific configuration from the Wi-Fi package.
