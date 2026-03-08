Name:           aic8800d80
Version:        b0787d9
Release:        3%{?dist}
Summary:        This driver is for the AIC8800D80 chipset, supported by devices such as the Tenda U11 and AX913B.

License:        Unknown
URL:            https://github.com/shenmintao/aic8800d80
Source0:        https://github.com/shenmintao/aic8800d80/archive/b0787d9989dc364a04ea8f11d6d824f391f77594.zip

BuildRequires: make
BuildRequires: gcc
BuildRequires: dkms
BuildRequires: kernel-headers
BuildRequires: kernel-devel
BuildRequires: unzip

Requires:      kernel-headers
Requires:      kernel
Requires:      dkms

%description
This driver is for the AIC8800D80 chipset, supported by devices such as the Tenda U11 and AX913B. With bluetooth support.

%prep

unzip %{SOURCE0}
mv aic8800d80{-b0787d9989dc364a04ea8f11d6d824f391f77594,}

%build

cd aic8800d80/drivers/aic8800
make clean
make

%install

rm -rf %{buildroot}

mkdir -p %{buildroot}/usr/lib/modules/%{uname}/kernel/drivers/net/wireless/aic8800 \
         %{buildroot}/usr/lib/firmware \
         %{buildroot}/etc/udev/rules.d

install -d -m 0755 %{buildroot}/usr/lib/modules/%{uname}/kernel/drivers/net/wireless/aic8800
install -d -m 0755 %{buildroot}/usr/lib/firmware
install -d -m 0755 %{buildroot}/usr/lib/firmware/aic8800DC
install -d -m 0755 %{buildroot}/usr/lib/firmware/aic8800D80X2
install -d -m 0755 %{buildroot}/usr/lib/firmware/aic8800D80
install -d -m 0755 %{buildroot}/usr/lib/firmware/aic8800
install -d -m 0755 %{buildroot}/etc/udev/rules.d

install -m 0644 \
  aic8800d80/aic.rules \
  %{buildroot}/etc/udev/rules.d/90-aic8800-mode-switch.rules

install -m 0644 \
  aic8800d80/drivers/aic8800/aic8800_fdrv/aic8800_fdrv.ko \
  %{buildroot}/usr/lib/modules/%{uname}/kernel/drivers/net/wireless/aic8800/aic8800_fdrv.ko

install -m 0644 \
  aic8800d80/drivers/aic8800/aic_load_fw/aic_load_fw.ko \
  %{buildroot}/usr/lib/modules/%{uname}/kernel/drivers/net/wireless/aic8800/aic_load_fw.ko

install -m 0644 \
  aic8800d80/fw/aic8800DC/lmacfw_rf_8800dc.bin \
  %{buildroot}/usr/lib/firmware/aic8800DC/lmacfw_rf_8800dc.bin

install -m 0644 \
  aic8800d80/fw/aic8800DC/fw_patch_table_8800dc_u02h.bin \
  %{buildroot}/usr/lib/firmware/aic8800DC/fw_patch_table_8800dc_u02h.bin

install -m 0644 \
  aic8800d80/fw/aic8800DC/fw_patch_table_8800dc_u02.bin \
  %{buildroot}/usr/lib/firmware/aic8800DC/fw_patch_table_8800dc_u02.bin

install -m 0644 \
  aic8800d80/fw/aic8800DC/fw_patch_8800dc_u02h.bin \
  %{buildroot}/usr/lib/firmware/aic8800DC/fw_patch_8800dc_u02h.bin

install -m 0644 \
  aic8800d80/fw/aic8800DC/fw_patch_8800dc_u02_ext0.bin \
  %{buildroot}/usr/lib/firmware/aic8800DC/fw_patch_8800dc_u02_ext0.bin

install -m 0644 \
  aic8800d80/fw/aic8800DC/fw_patch_8800dc_u02.bin \
  %{buildroot}/usr/lib/firmware/aic8800DC/fw_patch_8800dc_u02.bin

install -m 0644 \
  aic8800d80/fw/aic8800DC/fw_adid_8800dc_u02h.bin \
  %{buildroot}/usr/lib/firmware/aic8800DC/fw_adid_8800dc_u02h.bin

install -m 0644 \
  aic8800d80/fw/aic8800DC/fw_adid_8800dc_u02.bin \
  %{buildroot}/usr/lib/firmware/aic8800DC/fw_adid_8800dc_u02.bin

install -m 0644 \
  aic8800d80/fw/aic8800DC/fmacfw_patch_tbl_8800dc_u02.bin \
  %{buildroot}/usr/lib/firmware/aic8800DC/fmacfw_patch_tbl_8800dc_u02.bin

install -m 0644 \
  aic8800d80/fw/aic8800DC/fmacfw_patch_tbl_8800dc_ipc_u02.bin \
  %{buildroot}/usr/lib/firmware/aic8800DC/fmacfw_patch_tbl_8800dc_ipc_u02.bin

install -m 0644 \
  aic8800d80/fw/aic8800DC/fmacfw_patch_tbl_8800dc_h_u02.bin \
  %{buildroot}/usr/lib/firmware/aic8800DC/fmacfw_patch_tbl_8800dc_h_u02.bin

install -m 0644 \
  aic8800d80/fw/aic8800DC/fmacfw_patch_8800dc_u02.bin \
  %{buildroot}/usr/lib/firmware/aic8800DC/fmacfw_patch_8800dc_u02.bin

install -m 0644 \
  aic8800d80/fw/aic8800DC/fmacfw_patch_8800dc_ipc_u02.bin \
  %{buildroot}/usr/lib/firmware/aic8800DC/fmacfw_patch_8800dc_ipc_u02.bin

install -m 0644 \
  aic8800d80/fw/aic8800DC/fmacfw_patch_8800dc_h_u02.bin \
  %{buildroot}/usr/lib/firmware/aic8800DC/fmacfw_patch_8800dc_h_u02.bin

install -m 0644 \
  aic8800d80/fw/aic8800DC/fmacfw_calib_8800dc_u02.bin \
  %{buildroot}/usr/lib/firmware/aic8800DC/fmacfw_calib_8800dc_u02.bin

install -m 0644 \
  aic8800d80/fw/aic8800DC/fmacfw_calib_8800dc_h_u02.bin \
  %{buildroot}/usr/lib/firmware/aic8800DC/fmacfw_calib_8800dc_h_u02.bin

install -m 0644 \
  aic8800d80/fw/aic8800DC/aic_userconfig_8800dw_w311.txt \
  %{buildroot}/usr/lib/firmware/aic8800DC/aic_userconfig_8800dw_w311.txt

install -m 0644 \
  aic8800d80/fw/aic8800DC/aic_userconfig_8800dw_u2.txt \
  %{buildroot}/usr/lib/firmware/aic8800DC/aic_userconfig_8800dw_u2.txt

install -m 0644 \
  aic8800d80/fw/aic8800DC/aic_userconfig_8800dw.txt \
  %{buildroot}/usr/lib/firmware/aic8800DC/aic_userconfig_8800dw.txt

install -m 0644 \
  aic8800d80/fw/aic8800DC/aic_userconfig_8800dc.txt \
  %{buildroot}/usr/lib/firmware/aic8800DC/aic_userconfig_8800dc.txt

install -m 0644 \
  aic8800d80/fw/aic8800DC/aic_powerlimit_8800dw.txt \
  %{buildroot}/usr/lib/firmware/aic8800DC/aic_powerlimit_8800dw.txt

install -m 0644 \
  aic8800d80/fw/aic8800DC/aic_powerlimit_8800dc.txt \
  %{buildroot}/usr/lib/firmware/aic8800DC/aic_powerlimit_8800dc.txt

install -m 0644 \
  aic8800d80/fw/aic8800D80X2/lmacfw_rf_8800d80x2.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80X2/lmacfw_rf_8800d80x2.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80X2/fw_patch_table_8800d80x2_u05.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80X2/fw_patch_table_8800d80x2_u05.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80X2/fw_patch_table_8800d80x2_u03.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80X2/fw_patch_table_8800d80x2_u03.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80X2/fw_patch_8800d80x2_u05.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80X2/fw_patch_8800d80x2_u05.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80X2/fw_patch_8800d80x2_u03.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80X2/fw_patch_8800d80x2_u03.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80X2/fw_adid_8800d80x2_u05.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80X2/fw_adid_8800d80x2_u05.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80X2/fw_adid_8800d80x2_u03.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80X2/fw_adid_8800d80x2_u03.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80X2/fmacfw_8800d80x2.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80X2/fmacfw_8800d80x2.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80X2/aic_userconfig_8800d80x2.txt \
  %{buildroot}/usr/lib/firmware/aic8800D80X2/aic_userconfig_8800d80x2.txt

install -m 0644 \
  aic8800d80/fw/aic8800D80X2/aic_powerlimit_8800d80x2.txt \
  %{buildroot}/usr/lib/firmware/aic8800D80X2/aic_powerlimit_8800d80x2.txt

install -m 0644 \
  aic8800d80/fw/aic8800D80/lmacfw_rf_8800d80_u02.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80/lmacfw_rf_8800d80_u02.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80/fw_patch_table_8800d80_u04.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80/fw_patch_table_8800d80_u04.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80/fw_patch_table_8800d80_u02.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80/fw_patch_table_8800d80_u02.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80/fw_patch_8800d80_u04.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80/fw_patch_8800d80_u04.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80/fw_patch_8800d80_u02_ext0.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80/fw_patch_8800d80_u02_ext0.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80/fw_patch_8800d80_u02.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80/fw_patch_8800d80_u02.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80/fw_ble_scan_ad_filter.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80/fw_ble_scan_ad_filter.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80/fw_adid_8800d80_u02.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80/fw_adid_8800d80_u02.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80/fmacfw_8800d80_u02_ipc.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80/fmacfw_8800d80_u02_ipc.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80/fmacfw_8800d80_u02.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80/fmacfw_8800d80_u02.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80/fmacfw_8800d80_h_u02_ipc.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80/fmacfw_8800d80_h_u02_ipc.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80/fmacfw_8800d80_h_u02.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80/fmacfw_8800d80_h_u02.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80/calibmode_8800d80.bin \
  %{buildroot}/usr/lib/firmware/aic8800D80/calibmode_8800d80.bin

install -m 0644 \
  aic8800d80/fw/aic8800D80/aic_userconfig_8800d80_u11_pro.txt \
  %{buildroot}/usr/lib/firmware/aic8800D80/aic_userconfig_8800d80_u11_pro.txt

install -m 0644 \
  aic8800d80/fw/aic8800D80/aic_userconfig_8800d80_u11_cus.txt \
  %{buildroot}/usr/lib/firmware/aic8800D80/aic_userconfig_8800d80_u11_cus.txt

install -m 0644 \
  aic8800d80/fw/aic8800D80/aic_userconfig_8800d80_u11.txt \
  %{buildroot}/usr/lib/firmware/./aic8800D80/aic_userconfig_8800d80_u11.txt

install -m 0644 \
  aic8800d80/fw/aic8800D80/aic_userconfig_8800d80.txt \
  %{buildroot}/usr/lib/firmware/./aic8800D80/aic_userconfig_8800d80.txt

install -m 0644 \
  aic8800d80/fw/aic8800D80/aic_powerlimit_8800d80.txt \
  %{buildroot}/usr/lib/firmware/./aic8800D80/aic_powerlimit_8800d80.txt

install -m 0644 \
  aic8800d80/fw/aic8800/m2d_ota.bin \
  %{buildroot}/usr/lib/firmware/aic8800/m2d_ota.bin

install -m 0644 \
  aic8800d80/fw/aic8800/fw_patch_u03.bin \
  %{buildroot}/usr/lib/firmware/aic8800/fw_patch_u03.bin

install -m 0644 \
  aic8800d80/fw/aic8800/fw_patch_table_u03.bin \
  %{buildroot}/usr/lib/firmware/aic8800/fw_patch_table_u03.bin

install -m 0644 \
  aic8800d80/fw/aic8800/fw_patch_table.bin \
  %{buildroot}/usr/lib/firmware/aic8800/fw_patch_table.bin

install -m 0644 \
  aic8800d80/fw/aic8800/fw_patch_rf.bin \
  %{buildroot}/usr/lib/firmware/aic8800/fw_patch_rf.bin

install -m 0644 \
  aic8800d80/fw/aic8800/fw_patch.bin \
  %{buildroot}/usr/lib/firmware/aic8800/fw_patch.bin

install -m 0644 \
  aic8800d80/fw/aic8800/fw_ble_scan_ad_filter_ldo.bin \
  %{buildroot}/usr/lib/firmware/aic8800/fw_ble_scan_ad_filter_ldo.bin

install -m 0644 \
  aic8800d80/fw/aic8800/fw_ble_scan_ad_filter_dcdc.bin \
  %{buildroot}/usr/lib/firmware/aic8800/fw_ble_scan_ad_filter_dcdc.bin

install -m 0644 \
  aic8800d80/fw/aic8800/fw_ble_scan.bin \
  %{buildroot}/usr/lib/firmware/aic8800/fw_ble_scan.bin

install -m 0644 \
  aic8800d80/fw/aic8800/fw_adid_u03.bin \
  %{buildroot}/usr/lib/firmware/aic8800/fw_adid_u03.bin

install -m 0644 \
  aic8800d80/fw/aic8800/fw_adid_rf.bin \
  %{buildroot}/usr/lib/firmware/aic8800/fw_adid_rf.bin

install -m 0644 \
  aic8800d80/fw/aic8800/fw_adid.bin \
  %{buildroot}/usr/lib/firmware/aic8800/fw_adid.bin

install -m 0644 \
  aic8800d80/fw/aic8800/fmacfw_rf.bin \
  %{buildroot}/usr/lib/firmware/aic8800/fmacfw_rf.bin

install -m 0644 \
  aic8800d80/fw/aic8800/fmacfw_no_msg_ep_rf.bin \
  %{buildroot}/usr/lib/firmware/aic8800/fmacfw_no_msg_ep_rf.bin

install -m 0644 \
  aic8800d80/fw/aic8800/fmacfw_no_msg_ep.bin \
  %{buildroot}/usr/lib/firmware/aic8800/fmacfw_no_msg_ep.bin

install -m 0644 \
  aic8800d80/fw/aic8800/fmacfw_m2d.bin \
  %{buildroot}/usr/lib/firmware/aic8800/fmacfw_m2d.bin

install -m 0644 \
  aic8800d80/fw/aic8800/fmacfw.bin \
  %{buildroot}/usr/lib/firmware/aic8800/fmacfw.bin

install -m 0644 \
  aic8800d80/fw/aic8800/aic_userconfig.txt \
  %{buildroot}/usr/lib/firmware/aic8800/aic_userconfig.txt

%clean
rm -rf %{buildroot}

%post
/usr/bin/depmod -a %{uname}

%files
%defattr(-,root,root,-)
%dir /usr/lib/modules/%{uname}/kernel/drivers/net/wireless/aic8800
%dir /usr/lib/firmware
%dir /etc/udev/rules.d
/etc/udev/rules.d/90-aic8800-mode-switch.rules
/usr/lib/modules/%{uname}/kernel/drivers/net/wireless/aic8800/aic8800_fdrv.ko
/usr/lib/modules/%{uname}/kernel/drivers/net/wireless/aic8800/aic_load_fw.ko
/usr/lib/firmware/aic8800DC/lmacfw_rf_8800dc.bin
/usr/lib/firmware/aic8800DC/fw_patch_table_8800dc_u02h.bin
/usr/lib/firmware/aic8800DC/fw_patch_table_8800dc_u02.bin
/usr/lib/firmware/aic8800DC/fw_patch_8800dc_u02h.bin
/usr/lib/firmware/aic8800DC/fw_patch_8800dc_u02_ext0.bin
/usr/lib/firmware/aic8800DC/fw_patch_8800dc_u02.bin
/usr/lib/firmware/aic8800DC/fw_adid_8800dc_u02h.bin
/usr/lib/firmware/aic8800DC/fw_adid_8800dc_u02.bin
/usr/lib/firmware/aic8800DC/fmacfw_patch_tbl_8800dc_u02.bin
/usr/lib/firmware/aic8800DC/fmacfw_patch_tbl_8800dc_ipc_u02.bin
/usr/lib/firmware/aic8800DC/fmacfw_patch_tbl_8800dc_h_u02.bin
/usr/lib/firmware/aic8800DC/fmacfw_patch_8800dc_u02.bin
/usr/lib/firmware/aic8800DC/fmacfw_patch_8800dc_ipc_u02.bin
/usr/lib/firmware/aic8800DC/fmacfw_patch_8800dc_h_u02.bin
/usr/lib/firmware/aic8800DC/fmacfw_calib_8800dc_u02.bin
/usr/lib/firmware/aic8800DC/fmacfw_calib_8800dc_h_u02.bin
/usr/lib/firmware/aic8800DC/aic_userconfig_8800dw_w311.txt
/usr/lib/firmware/aic8800DC/aic_userconfig_8800dw_u2.txt
/usr/lib/firmware/aic8800DC/aic_userconfig_8800dw.txt
/usr/lib/firmware/aic8800DC/aic_userconfig_8800dc.txt
/usr/lib/firmware/aic8800DC/aic_powerlimit_8800dw.txt
/usr/lib/firmware/aic8800DC/aic_powerlimit_8800dc.txt
/usr/lib/firmware/aic8800D80X2/lmacfw_rf_8800d80x2.bin
/usr/lib/firmware/aic8800D80X2/fw_patch_table_8800d80x2_u05.bin
/usr/lib/firmware/aic8800D80X2/fw_patch_table_8800d80x2_u03.bin
/usr/lib/firmware/aic8800D80X2/fw_patch_8800d80x2_u05.bin
/usr/lib/firmware/aic8800D80X2/fw_patch_8800d80x2_u03.bin
/usr/lib/firmware/aic8800D80X2/fw_adid_8800d80x2_u05.bin
/usr/lib/firmware/aic8800D80X2/fw_adid_8800d80x2_u03.bin
/usr/lib/firmware/aic8800D80X2/fmacfw_8800d80x2.bin
/usr/lib/firmware/aic8800D80X2/aic_userconfig_8800d80x2.txt
/usr/lib/firmware/aic8800D80X2/aic_powerlimit_8800d80x2.txt
/usr/lib/firmware/aic8800D80/lmacfw_rf_8800d80_u02.bin
/usr/lib/firmware/aic8800D80/fw_patch_table_8800d80_u04.bin
/usr/lib/firmware/aic8800D80/fw_patch_table_8800d80_u02.bin
/usr/lib/firmware/aic8800D80/fw_patch_8800d80_u04.bin
/usr/lib/firmware/aic8800D80/fw_patch_8800d80_u02_ext0.bin
/usr/lib/firmware/aic8800D80/fw_patch_8800d80_u02.bin
/usr/lib/firmware/aic8800D80/fw_ble_scan_ad_filter.bin
/usr/lib/firmware/aic8800D80/fw_adid_8800d80_u02.bin
/usr/lib/firmware/aic8800D80/fmacfw_8800d80_u02_ipc.bin
/usr/lib/firmware/aic8800D80/fmacfw_8800d80_u02.bin
/usr/lib/firmware/aic8800D80/fmacfw_8800d80_h_u02_ipc.bin
/usr/lib/firmware/aic8800D80/fmacfw_8800d80_h_u02.bin
/usr/lib/firmware/aic8800D80/calibmode_8800d80.bin
/usr/lib/firmware/aic8800D80/aic_userconfig_8800d80_u11_pro.txt
/usr/lib/firmware/aic8800D80/aic_userconfig_8800d80_u11_cus.txt
/usr/lib/firmware/aic8800D80/aic_userconfig_8800d80_u11.txt
/usr/lib/firmware/aic8800D80/aic_userconfig_8800d80.txt
/usr/lib/firmware/aic8800D80/aic_powerlimit_8800d80.txt
/usr/lib/firmware/aic8800/m2d_ota.bin
/usr/lib/firmware/aic8800/fw_patch_u03.bin
/usr/lib/firmware/aic8800/fw_patch_table_u03.bin
/usr/lib/firmware/aic8800/fw_patch_table.bin
/usr/lib/firmware/aic8800/fw_patch_rf.bin
/usr/lib/firmware/aic8800/fw_patch.bin
/usr/lib/firmware/aic8800/fw_ble_scan_ad_filter_ldo.bin
/usr/lib/firmware/aic8800/fw_ble_scan_ad_filter_dcdc.bin
/usr/lib/firmware/aic8800/fw_ble_scan.bin
/usr/lib/firmware/aic8800/fw_adid_u03.bin
/usr/lib/firmware/aic8800/fw_adid_rf.bin
/usr/lib/firmware/aic8800/fw_adid.bin
/usr/lib/firmware/aic8800/fmacfw_rf.bin
/usr/lib/firmware/aic8800/fmacfw_no_msg_ep_rf.bin
/usr/lib/firmware/aic8800/fmacfw_no_msg_ep.bin
/usr/lib/firmware/aic8800/fmacfw_m2d.bin
/usr/lib/firmware/aic8800/fmacfw.bin
/usr/lib/firmware/aic8800/aic_userconfig.txt

%license
%doc

%changelog
* Sat Mar 07 2026 Nurmukhamed Artykaly
- Created spec file.
* Sun Mar 08 2026 Nurmukhamed Artykaly
- Switched to bluetooth branch
- Added all firmwares
- Removed bin file from RADXA repository
