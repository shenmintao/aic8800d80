# Monitor TX confirmation regression

Run `python3 tests/monitor/test_tx_confirm.py`. The fixture compiles the actual
USB and SDIO descriptor-selection blocks from `rwnx_tx_push()` with mocked
transport functions. It checks raw frames, normal management frames, EAPOL,
USB-specific WAPI handling, ordinary/fixed-rate data, ring registration before
transport submission, and accounting deferred until confirmation. It does not
send frames or model firmware, USB completion, or radio behavior.

To compare another revision, pass `--source /path/to/rwnx_tx.c`.

Build the optional monitor path with prepared kernel headers:

```sh
bash tests/kernel-build/build.sh /path/to/kernel/build CC=gcc-14 CONFIG_RWNX_MON_DATA=y
```

`CONFIG_RWNX_MON_XMIT` already defaults to `y`; `CONFIG_RWNX_MON_DATA` remains
opt-in. For hardware verification, re-open the monitor interface before setting
its channel and use a second radio on the same channel to capture a short
beacon/probe-request test on equipment you control. Check both 2.4 and 5 GHz,
normal station traffic after the test, and interface teardown/recreation.

The confirmation descriptor carries a ring index instead of the fixed-rate
encoding. Radiotap-selected rates are not established by this check. Other
chips, SDIO hardware and arbitrary raw data frames also require hardware
validation.

Based on ggorbi99-dev's [issue #123](https://github.com/shenmintao/aic8800d80/issues/123).
The contributor reports over-the-air validation on a D80 MCU1 USB adapter;
that report is not a hardware test of these commits or of the `main` firmware.
