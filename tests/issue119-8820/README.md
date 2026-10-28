# Product 0x8820 loader checks

Run `python3 tests/issue119-8820/test_loader.py` with a C compiler available.
The test extracts the actual system configuration, normal-mode upload branch,
and busy-connect guard and runs them against mocked USB requests. It checks
that alternating/reprobing M80 and regular D80 devices preserves each loader
sequence, retains the MCU1 cache fix, propagates configuration errors, and
rejects a new connect without overwriting an operation already in progress.

These checks do not establish hardware or Bluetooth compatibility. Issue #118
reports working Wi-Fi on one UGREEN `a69c:5723`, chip revision 7, MCU1 unit
with product `0x8820` and the branch's 327,037-byte V3 FMAC. Bluetooth pairing
and audio remain unverified. Product `0x8820` is not a rule to skip patches
on every MCU1 device, and the report does not establish that every device in
issue #79 works with this firmware.

