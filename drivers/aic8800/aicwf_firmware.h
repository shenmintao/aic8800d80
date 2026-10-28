/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Files installed below /lib/firmware by install.sh.
 *
 * Both USB modules may be loaded in an initramfs. Declare the complete matched
 * firmware set, including configuration and extension patches, so initramfs
 * generators can copy it along with either module. Keep this list in sync with
 * fw/; tests/firmware/test_firmware.py checks the installed paths.
 */
#ifndef AICWF_FIRMWARE_H
#define AICWF_FIRMWARE_H

MODULE_FIRMWARE("aic8800/aic_userconfig.txt");
MODULE_FIRMWARE("aic8800/fmacfw.bin");
MODULE_FIRMWARE("aic8800/fmacfw_m2d.bin");
MODULE_FIRMWARE("aic8800/fmacfw_no_msg_ep.bin");
MODULE_FIRMWARE("aic8800/fmacfw_no_msg_ep_rf.bin");
MODULE_FIRMWARE("aic8800/fmacfw_rf.bin");
MODULE_FIRMWARE("aic8800/fw_adid.bin");
MODULE_FIRMWARE("aic8800/fw_adid_rf.bin");
MODULE_FIRMWARE("aic8800/fw_adid_u03.bin");
MODULE_FIRMWARE("aic8800/fw_ble_scan.bin");
MODULE_FIRMWARE("aic8800/fw_ble_scan_ad_filter_dcdc.bin");
MODULE_FIRMWARE("aic8800/fw_ble_scan_ad_filter_ldo.bin");
MODULE_FIRMWARE("aic8800/fw_patch.bin");
MODULE_FIRMWARE("aic8800/fw_patch_rf.bin");
MODULE_FIRMWARE("aic8800/fw_patch_table.bin");
MODULE_FIRMWARE("aic8800/fw_patch_table_u03.bin");
MODULE_FIRMWARE("aic8800/fw_patch_u03.bin");
MODULE_FIRMWARE("aic8800/m2d_ota.bin");
MODULE_FIRMWARE("aic8800D80/aic_powerlimit_8800d80.txt");
MODULE_FIRMWARE("aic8800D80/aic_userconfig_8800d80.txt");
MODULE_FIRMWARE("aic8800D80/aic_userconfig_8800d80_u11.txt");
MODULE_FIRMWARE("aic8800D80/aic_userconfig_8800d80_u11_cus.txt");
MODULE_FIRMWARE("aic8800D80/aic_userconfig_8800d80_u11_pro.txt");
MODULE_FIRMWARE("aic8800D80/calibmode_8800d80.bin");
MODULE_FIRMWARE("aic8800D80/fmacfw_8800d80_h_u02.bin");
MODULE_FIRMWARE("aic8800D80/fmacfw_8800d80_h_u02_ipc.bin");
MODULE_FIRMWARE("aic8800D80/fmacfw_8800d80_u02.bin");
MODULE_FIRMWARE("aic8800D80/fmacfw_8800d80_u02_ipc.bin");
MODULE_FIRMWARE("aic8800D80/fw_adid_8800d80_u02.bin");
MODULE_FIRMWARE("aic8800D80/fw_ble_scan_ad_filter.bin");
MODULE_FIRMWARE("aic8800D80/fw_patch_8800d80_u02.bin");
MODULE_FIRMWARE("aic8800D80/fw_patch_8800d80_u02_ext0.bin");
MODULE_FIRMWARE("aic8800D80/fw_patch_8800d80_u04.bin");
MODULE_FIRMWARE("aic8800D80/fw_patch_table_8800d80_u02.bin");
MODULE_FIRMWARE("aic8800D80/fw_patch_table_8800d80_u04.bin");
MODULE_FIRMWARE("aic8800D80/lmacfw_rf_8800d80_u02.bin");
MODULE_FIRMWARE("aic8800D80N/aic_powerlimit_8800d80n.txt");
MODULE_FIRMWARE("aic8800D80N/aic_userconfig_8800d80n.txt");
MODULE_FIRMWARE("aic8800D80N/fmacfw_btusb_8800d80n_u02.bin");
MODULE_FIRMWARE("aic8800D80N/fmacfw_calib_8800d80n_u02.bin");
MODULE_FIRMWARE("aic8800D80N/fmacfw_cinit_8800d80n_u02.bin");
MODULE_FIRMWARE("aic8800D80N/fmacfw_patch_8800d80n_u02.bin");
MODULE_FIRMWARE("aic8800D80N/fmacfw_patch_tbl_8800d80n_u02.bin");
MODULE_FIRMWARE("aic8800D80N/fw_patch_8800d80n_rf_u02.bin");
MODULE_FIRMWARE("aic8800D80N/fw_patch_8800d80n_rf_u02_ext0.bin");
MODULE_FIRMWARE("aic8800D80N/fw_patch_8800d80n_u02.bin");
MODULE_FIRMWARE("aic8800D80N/fw_patch_8800d80n_u02_ext0.bin");
MODULE_FIRMWARE("aic8800D80N/fw_patch_table_8800d80n_rf_u02.bin");
MODULE_FIRMWARE("aic8800D80N/fw_patch_table_8800d80n_u02.bin");
MODULE_FIRMWARE("aic8800D80N/lmacfw_rf_8800d80n.bin");
MODULE_FIRMWARE("aic8800D80X2/aic_powerlimit_8800d80x2.txt");
MODULE_FIRMWARE("aic8800D80X2/aic_userconfig_8800d80x2.txt");
MODULE_FIRMWARE("aic8800D80X2/aicbt.conf");
MODULE_FIRMWARE("aic8800D80X2/fmacfw_8800d80x2.bin");
MODULE_FIRMWARE("aic8800D80X2/fw_adid_8800d80x2_u03.bin");
MODULE_FIRMWARE("aic8800D80X2/fw_adid_8800d80x2_u05.bin");
MODULE_FIRMWARE("aic8800D80X2/fw_patch_8800d80x2_u03.bin");
MODULE_FIRMWARE("aic8800D80X2/fw_patch_8800d80x2_u05.bin");
MODULE_FIRMWARE("aic8800D80X2/fw_patch_8800d80x2_u05_ext0.bin");
MODULE_FIRMWARE("aic8800D80X2/fw_patch_8800d80x2_u05_ext1.bin");
MODULE_FIRMWARE("aic8800D80X2/fw_patch_table_8800d80x2_u03.bin");
MODULE_FIRMWARE("aic8800D80X2/fw_patch_table_8800d80x2_u05.bin");
MODULE_FIRMWARE("aic8800D80X2/lmacfw_rf_8800d80x2.bin");
MODULE_FIRMWARE("aic8800DC/aic_powerlimit_8800dc.txt");
MODULE_FIRMWARE("aic8800DC/aic_powerlimit_8800dw.txt");
MODULE_FIRMWARE("aic8800DC/aic_userconfig_8800dc.txt");
MODULE_FIRMWARE("aic8800DC/aic_userconfig_8800dw.txt");
MODULE_FIRMWARE("aic8800DC/aic_userconfig_8800dw_u2.txt");
MODULE_FIRMWARE("aic8800DC/aic_userconfig_8800dw_w311.txt");
MODULE_FIRMWARE("aic8800DC/fmacfw_calib_8800dc_h_u02.bin");
MODULE_FIRMWARE("aic8800DC/fmacfw_calib_8800dc_u02.bin");
MODULE_FIRMWARE("aic8800DC/fmacfw_patch_8800dc_h_u02.bin");
MODULE_FIRMWARE("aic8800DC/fmacfw_patch_8800dc_ipc_u02.bin");
MODULE_FIRMWARE("aic8800DC/fmacfw_patch_8800dc_u02.bin");
MODULE_FIRMWARE("aic8800DC/fmacfw_patch_tbl_8800dc_h_u02.bin");
MODULE_FIRMWARE("aic8800DC/fmacfw_patch_tbl_8800dc_ipc_u02.bin");
MODULE_FIRMWARE("aic8800DC/fmacfw_patch_tbl_8800dc_u02.bin");
MODULE_FIRMWARE("aic8800DC/fw_adid_8800dc_u02.bin");
MODULE_FIRMWARE("aic8800DC/fw_adid_8800dc_u02h.bin");
MODULE_FIRMWARE("aic8800DC/fw_patch_8800dc_u02.bin");
MODULE_FIRMWARE("aic8800DC/fw_patch_8800dc_u02_ext0.bin");
MODULE_FIRMWARE("aic8800DC/fw_patch_8800dc_u02h.bin");
MODULE_FIRMWARE("aic8800DC/fw_patch_table_8800dc_u02.bin");
MODULE_FIRMWARE("aic8800DC/fw_patch_table_8800dc_u02h.bin");
MODULE_FIRMWARE("aic8800DC/lmacfw_rf_8800dc.bin");
MODULE_FIRMWARE("aic8800DLN/aic_powerlimit_8800dln.txt");
MODULE_FIRMWARE("aic8800DLN/aic_userconfig_8800dln.txt");
MODULE_FIRMWARE("aic8800DLN/fmacfw_gaintbl_8800dln.bin");
MODULE_FIRMWARE("aic8800DLN/fmacfw_initvar_8800dln.bin");

#endif /* AICWF_FIRMWARE_H */

