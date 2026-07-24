# Issue #71: complete MCU1 V3 DC/DW profile

This branch is the second test for
[issue #71](https://github.com/shenmintao/aic8800d80/issues/71). It follows the
same method as the successful issue #58 legacy MCU1 branch: use a complete,
matched legacy firmware and loader profile rather than mixing firmware
generations.

The branch keeps the current USB transport, cfg80211 integration, device IDs,
DKMS installer, and modern-kernel compatibility. For AIC8800DC/DW it restores
the complete firmware directory and matching loader from immediately before
SDK V5 commit `d66e5cb`. That loader includes the MCU1 cache fix and flash DPD
validation used by the reporter's previously working driver.

The first issue #71 test proved that the DPD result stored in flash is valid,
but the V5 main application still timed out at `DBG_START_APP_REQ`. This branch
tests the complete profile that produced the reporter's known-good firmware
hashes.

## Install

```bash
git fetch origin
git switch test/issue-71-mcu1-v3-profile
git pull --ff-only
sudo ./install.sh
sudo poweroff
```

After shutdown, unplug the TX1U Nano or remove host power for at least 10
seconds before reconnecting and booting.

## Verify

```bash
git rev-parse --short HEAD
md5sum /lib/firmware/aic8800DC/fmacfw_patch_8800dc_u02.bin
md5sum /lib/firmware/aic8800DC/fmacfw_patch_tbl_8800dc_u02.bin
lsusb
lsusb -t
iw dev
sudo journalctl -k -b --no-pager | tee issue71-v3-profile.log
sudo journalctl -k -b --no-pager | grep -iE 'issue71|chip_id|chip_mcu_id|chip_sub_id|DPD|calib|patch.*tbl|start app|firmware version|interface|wlan|timed-out|err_lmac'
```

Expected firmware values and log markers include:

```text
bfd8ea1d174242e7ec823813b6c5d849  fmacfw_patch_8800dc_u02.bin
7b5fde609392c2e6c2c5874838dda718  fmacfw_patch_tbl_8800dc_u02.bin
chip_id=7, chip_mcu_id=1, chip_sub_id=1
issue71: using complete pre-SDK-V5/V3 DC/DW firmware and loader profile
issue71: legacy V3 flash DPD result is valid
misc ram valid, skip calib process
FMACFW_PATCH_TBL_8800DC_U02_DESCRIBE_BASE = 187c00
Start app: 00150000, 5
Firmware Version: ...
New interface create wlan0
```

There should be no command timeout, `err_lmac_reqs`, or failed USB probe after
the main application starts.

If the interface is created, also test scanning, association, DHCP, gateway
and Internet reachability, at least 10 minutes of real traffic, and one USB
unplug/replug cycle. Attach `issue71-v3-profile.log` whether the result passes
or fails.

## Roll back

```bash
git switch main
git pull --ff-only
sudo ./install.sh
sudo poweroff
```

Cold-power-cycle the adapter again before booting the rollback result.
