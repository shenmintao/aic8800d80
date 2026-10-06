# fix(legacy-mcu1): TP-Link `2357:0147` (AIC8800DC) support + NULL guards for probe-failure & disconnect teardown

Applies cleanly on `legacy-mcu1 @ b7a476c`. This PR branch carries the three
commits on top of that base; cherry-pick or `git am` both work. See also the
companion issue for the README branch-criterion problem and cross-branch notes.

## What

1. **`d7f5e38` — TP-Link `2357:0147` device support**
   Neither branch claims this PID (TP entries are `0x014b/0x014e`; chipmatch
   only accepts `0x88dc`), so a clean tree ignores the device entirely
   (`BIND=NONE`). Adds `USB_PRODUCT_ID_TP_AIC8800DC (0x0147)`, the
   `usb_device_id` entry, and the `aicwf_usb_chipmatch` DC branch.

2. **`0eca8e3` — `aicwf_bus_deinit` / `aicwf_usb_bus_stop` NULL guards**
   `aicwf_bus_deinit()` runs twice on probe-failure paths with no NULL check and
   never clears drvdata; this matches the historical host oops at
   `RIP: aicwf_usb_bus_stop+0x11` (observed repeatedly before the fixes; that
   probe-failure path was not re-triggered in this matrix). Returns early on NULL
   drvdata, clears drvdata **after** `aicwf_bus_stop()` (its `ops->stop()`
   re-reads drvdata), and NULL-checks `bus_if` inside `aicwf_usb_bus_stop()`.
   **Cherry-pick this together with commit `1b65c0e` below**: the drvdata clearing
   introduced here is exactly what makes `rwnx_close` observe `usbdev==NULL`
   during disconnect (verified: this commit alone + unplug = oops; with `1b65c0e`
   = `BUG=0`).

3. **`1b65c0e` — `rwnx_close()` NULL guard on the disconnect path**
   The `vif_started==0` branch reads `usbdev->bus_if->state` with no
   `usbdev` check. With `aicwf_bus_deinit()` preceding `netdev close` during
   disconnect, close observes `usbdev==NULL` → oops (`CR2=0x8`, pinned via
   disassembly reloc `rwnx_send_reset`) — field symptom: **host hard-hangs on
   physical unplug**. The unpatched tree reaches the same expression through a
   stale pointer (latent UAF, currently survives by luck); this guard covers
   both, USB and SDIO variants. Complementary to #99 (merged as `b72eea9`), which
   skips the teardown *waits* when the bus is down — removing the 4 s/2 s stalls
   and the spurious `WARN_ON` — but only on `main`; `legacy-mcu1` still waits and
   warns (observed in every teardown here). #99 does not touch the
   `vif_started==0` reset-block dereference guarded above, so the two fixes do not
   overlap; cherry-picking #99 to this branch would be a natural follow-up.

## Not included (deliberately)

- **`0x40100020` cache RMW** — this branch **already performs it**:
  `system_config_8800dc()` contains the vendor, mcu1-gated
  `if (chip_mcu_id) { read 0x40100020; |= 1; write }` (verified line-by-line;
  `main`'s copy of the same function lacks it — that gap is discussed in the
  linked issue, together with the Windows-side confirmation and the canonical
  same-device fix thread `ronnyf/AIC8800-Linux-Driver#32` / `#33`, to which we defer RMW
  ownership). No new RMW is needed here, which is why our boot verification
  passes with this branch's stock cache handling.

## Verification (isolated VFIO lab, one unit, kernels 7.2.8-arch1-2 / 7.1.5-zen)

| stage | result |
|---|---|
| clean `b7a476c` | device enumerated, **`BIND=NONE`** (missing ID confirmed) |
| +`d7f5e38` | binds, `chipmatch USE AIC8800DC (TP-Link 0x0147)`, issue71 loader boots chip → `Firmware Version: …2022 gcf79227` → `wlan0` |
| +`0eca8e3`, `1b65c0e` | scan → WPA2 (`TP-LINK_2Star`) → DHCP → **IPv4/IPv6 ping 3/3, 0% loss** each |
| disconnect (`authorized=0` ≙ unplug) | **`BUG=0`**, only the pre-existing `rwnx_main.c:1529` `WARN_ON`, teardown sequence clean, shell alive |
| A/B control | same disconnect on recognition-only build: **no crash** → crash attributed to missing guard, then root-caused & fixed as above |

Device: `chip_id=7, chip_sub_id=1`, `chip_mcu_id=1` — printed natively by this
branch (`system_config_8800dc()` log), cross-checked against the `0x40500000`
read and the Windows capture;
chip self-boots `zh Aug 08 2022 … gcf79227`; firmware dir unchanged from
branch contents.

Refs: #101 · companion issue (README criterion & cross-branch notes): _link pending_
