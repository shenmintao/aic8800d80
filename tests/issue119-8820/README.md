# Product 0x8820 loader checks

Run `python3 tests/issue119-8820/test_loader.py` with a C compiler available.
The test extracts the actual system configuration, normal-mode upload branch,
and busy-connect guard and runs them against mocked USB requests. It checks
that both product `0x8820` and regular D80 devices upload the complete
adid/patch/extended-patch/patch-table/patch-config layer before starting FMAC.
Alternating and reprobed devices receive the extra syscfg writes only when
their current product readback is `0x8820`. The checks also retain the MCU1
cache fix, propagate read/cache/syscfg write errors, and reject a new connect
without overwriting an operation already in progress.

The [hardware follow-up in PR #119](https://github.com/shenmintao/aic8800d80/pull/119#issuecomment-6468608942)
corrects the initial FMAC-only hypothesis from #118. On one UGREEN
`a69c:5723`, chip revision 7, MCU1, product `0x8820` unit running Ubuntu 26.04
and kernel `7.0.0-14-generic`, the two syscfg writes alone resolve the
`MM_START` timeout. Skipping the patch layer causes Bluetooth HCI Reset
(`0x0c03`) to time out; restoring all patch uploads, including patch config,
preserves Wi-Fi and restores controller initialization and LE discovery.
The firmware remains the branch's matched V3 set, including its 327,037-byte
FMAC.

The contributor tested the restored-patch variant, rather than this exact
follow-up commit. The cold-start sequence was replayed through USB
re-enumeration; physical unplug/replug, reboot of that variant, Bluetooth
pairing/audio, and other units remain unverified. These mocked checks do not
establish hardware compatibility or support for every device in issue #79.

