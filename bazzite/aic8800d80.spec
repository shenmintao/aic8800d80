Name:           aic8800d80
Version:        453feef
Release:        1%{?dist}
Summary:        This driver is for the AIC8800D80 chipset, supported by devices such as the Tenda U11 and AX913B.

License:        Unknown
URL:            https://github.com/shenmintao/aic8800d80
Source0:        https://github.com/shenmintao/aic8800d80/archive/453feef9547eafdf2725fdd4129a716600ecd03f.zip
Source1:        https://github.com/radxa-pkg/aic8800/blob/main/src/USB/driver_fw/fw/aic8800D80/fmacfw_8800d80_u02.bin

BuildRequires: make
BuildRequires: gcc
BuildRequires: dkms
BuildRequires: kernel-headers
BuildRequires: kernel-devel
BuildRequires: unzip

Requires:      kernel-headers
Requires:      kernel
%description
This driver is for the AIC8800D80 chipset, supported by devices such as the Tenda U11 and AX913B.

%prep

unzip %{SOURCE0}
mv aic8800d80{-453feef9547eafdf2725fdd4129a716600ecd03f,}
cp %{SOURCE1} aic8800d80/fw/aic8800D80


%build

cd aic8800d80/drivers/aic8800
make clean
make

%install

pwd

rm -rf %{buildroot}

mkdir -p %{buildroot}/usr/lib/modules/%{uname}/kernel/drivers/net/wireless/aic8800 \
         %{buildroot}/usr/lib/firmware \
         %{buildroot}/etc/udev/rules.d

install -d -m 0755 %{buildroot}/usr/lib/modules/%{uname}/kernel/drivers/net/wireless/aic8800
install -d -m 0755 %{buildroot}/usr/lib/firmware
install -d -m 0755 %{buildroot}/etc/udev/rules.d

install -m 0644 aic8800d80/aic.rules %{buildroot}/etc/udev/rules.d/90-aic8800-mode-switch.rules

install -m 0644 aic8800d80/drivers/aic8800/aic8800_fdrv/aic8800_fdrv.ko %{buildroot}/usr/lib/modules/%{uname}/kernel/drivers/net/wireless/aic8800/aic8800_fdrv.
install -m 0644 aic8800d80/drivers/aic8800/aic_load_fw/aic_load_fw.ko %{buildroot}/usr/lib/modules/%{uname}/kernel/drivers/net/wireless/aic8800/aic_load_fw.ko

install -m 0644 aic8800d80/fw/aic8800D80/aic_powerlimit_8800d80.txt         %{buildroot}/usr/lib/firmware/aic_powerlimit_8800d80.txt
install -m 0644 aic8800d80/fw/aic8800D80/aic_userconfig_8800d80.txt         %{buildroot}/usr/lib/firmware/aic_userconfig_8800d80.txt
install -m 0644 aic8800d80/fw/aic8800D80/aic_userconfig_8800d80_u11_cus.txt %{buildroot}/usr/lib/firmware/aic_userconfig_8800d80_u11_cus.txt
install -m 0644 aic8800d80/fw/aic8800D80/aic_userconfig_8800d80_u11_cus.txt %{buildroot}/usr/lib/firmware/aic_userconfig_8800d80_u11_pro.txt
install -m 0644 aic8800d80/fw/aic8800D80/aic_userconfig_8800d80_u11.txt     %{buildroot}/usr/lib/firmware/aic_userconfig_8800d80_u11.txt
install -m 0644 aic8800d80/fw/aic8800D80/calibmode_8800d80.bin              %{buildroot}/usr/lib/firmware/calibmode_8800d80.bin
install -m 0644 aic8800d80/fw/aic8800D80/fmacfw_8800d80_h_u02.bin           %{buildroot}/usr/lib/firmware/fmacfw_8800d80_h_u02.bin
install -m 0644 aic8800d80/fw/aic8800D80/fmacfw_8800d80_h_u02_ipc.bin       %{buildroot}/usr/lib/firmware/fmacfw_8800d80_h_u02_ipc.bin
install -m 0644 aic8800d80/fw/aic8800D80/fmacfw_8800d80_u02.bin             %{buildroot}/usr/lib/firmware/fmacfw_8800d80_u02.bin
install -m 0644 aic8800d80/fw/aic8800D80/fmacfw_8800d80_u02_ipc.bin         %{buildroot}/usr/lib/firmware/fmacfw_8800d80_u02_ipc.bin
install -m 0644 aic8800d80/fw/aic8800D80/fw_adid_8800d80_u02.bin            %{buildroot}/usr/lib/firmware/fw_adid_8800d80_u02.bin
install -m 0644 aic8800d80/fw/aic8800D80/fw_ble_scan_ad_filter.bin          %{buildroot}/usr/lib/firmware/fw_ble_scan_ad_filter.bin
install -m 0644 aic8800d80/fw/aic8800D80/fw_patch_8800d80_u02.bin           %{buildroot}/usr/lib/firmware/fw_patch_8800d80_u02.bin
install -m 0644 aic8800d80/fw/aic8800D80/fw_patch_8800d80_u02_ext0.bin      %{buildroot}/usr/lib/firmware/fw_patch_8800d80_u02_ext0.bin
install -m 0644 aic8800d80/fw/aic8800D80/fw_patch_table_8800d80_u02.bin     %{buildroot}/usr/lib/firmware/fw_patch_table_8800d80_u02.bin
install -m 0644 aic8800d80/fw/aic8800D80/lmacfw_rf_8800d80_u02.bin          %{buildroot}/usr/lib/firmware/lmacfw_rf_8800d80_u02.bin

%clean
rm -rf %{buildroot}

%post
/usr/bin/depmod -a

%files
%defattr(-,root,root,-)
%dir /usr/lib/modules/%{uname}/kernel/drivers/net/wireless/aic8800
%dir /usr/lib/firmware
%dir /etc/udev/rules.d
/etc/udev/rules.d/90-aic8800-mode-switch.rules
/usr/lib/modules/%{uname}/kernel/drivers/net/wireless/aic8800/aic8800_fdrv.ko
/usr/lib/modules/%{uname}/kernel/drivers/net/wireless/aic8800/aic_load_fw.ko
/usr/lib/firmware/aic_powerlimit_8800d80.txt
/usr/lib/firmware/aic_userconfig_8800d80.txt
/usr/lib/firmware/aic_userconfig_8800d80_u11_cus.txt
/usr/lib/firmware/aic_userconfig_8800d80_u11_pro.txt
/usr/lib/firmware/aic_userconfig_8800d80_u11.txt
/usr/lib/firmware/calibmode_8800d80.bin
/usr/lib/firmware/fmacfw_8800d80_h_u02.bin
/usr/lib/firmware/fmacfw_8800d80_h_u02_ipc.bin
/usr/lib/firmware/fmacfw_8800d80_u02.bin
/usr/lib/firmware/fmacfw_8800d80_u02_ipc.bin
/usr/lib/firmware/fw_adid_8800d80_u02.bin
/usr/lib/firmware/fw_ble_scan_ad_filter.bin
/usr/lib/firmware/fw_patch_8800d80_u02.bin
/usr/lib/firmware/fw_patch_8800d80_u02_ext0.bin
/usr/lib/firmware/fw_patch_table_8800d80_u02.bin
/usr/lib/firmware/lmacfw_rf_8800d80_u02.bin

%license
%doc

%changelog
* Sat Mar 07 2026 Nurmukhamed Artykaly
- Created spec file.
