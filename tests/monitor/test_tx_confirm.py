#!/usr/bin/env python3
"""Test descriptor routing using the actual USB/SDIO rwnx_tx_push blocks."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path,
                    default=ROOT / "drivers/aic8800/aic8800_fdrv/rwnx_tx.c")
args, remaining = parser.parse_known_args()

PRELUDE = r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#define TXU_CNTRL_MGMT (1 << 3)
#define AICWFDBG(...) ((void)0)
struct sk_buff { unsigned char data[80]; };
struct vif { struct { int tx_packets, tx_bytes; } net_stats; };
struct rwnx_sw_txhdr {
    struct { struct { unsigned int flags, ethertype, status_desc_addr, staid; } host; } desc;
    int raw_frame, fixed_rate, need_cfm, headroom, frame_len;
    unsigned int rate_config;
    struct vif *rwnx_vif;
};
struct env { unsigned int txdesc_free_idx[1]; };
struct rwnx_hw {
    struct env usb_env, sdio_env;
    struct { unsigned long last_tx; } stats;
    void *usbdev, *sdiodev;
};
static int registrations, submissions;
static struct sk_buff packet;
static struct rwnx_sw_txhdr hdr;
static struct vif vif;
static struct rwnx_hw hw;
static unsigned long jiffies = 1234;
static void push(struct env *env, int queue, long host_id) {
    assert(host_id == (long)&packet && queue == 0 && submissions == 0);
    registrations++; env->txdesc_free_idx[0]++;
}
#define aicwf_usb_host_txdesc_push push
#define aicwf_sdio_host_txdesc_push push
static void aicwf_frame_tx(void *dev, struct sk_buff *skb) {
    assert(skb == &packet && dev == &hw);
    assert(registrations == hdr.need_cfm);
    submissions++;
}
static void tx(struct rwnx_hw *rwnx_hw, struct rwnx_sw_txhdr *sw_txhdr,
               struct sk_buff *skb) {
'''

CHECKS = r'''
}
static void check(unsigned char fc, int raw, int mgmt, unsigned int ether,
                  int fixed_rate, int expect_confirm) {
    memset(&hdr, 0, sizeof(hdr)); memset(&hw, 0, sizeof(hw));
    memset(&vif, 0, sizeof(vif)); memset(&packet, 0, sizeof(packet));
    registrations = submissions = 0;
    hdr.headroom = 8; packet.data[8] = fc; hdr.frame_len = 24;
    hdr.raw_frame = raw; hdr.fixed_rate = fixed_rate; hdr.rate_config = 0x123;
    hdr.desc.host.flags = mgmt ? TXU_CNTRL_MGMT : 0;
    hdr.desc.host.ethertype = ether;
    hdr.rwnx_vif = &vif;
    hw.usbdev = hw.sdiodev = &hw;
    hw.usb_env.txdesc_free_idx[0] = hw.sdio_env.txdesc_free_idx[0] = 7;
    tx(&hw, &hdr, &packet);
    assert(submissions == 1 && registrations == expect_confirm);
    assert(hdr.need_cfm == expect_confirm);
    if (expect_confirm) {
        assert(hdr.desc.host.status_desc_addr == ((1U << 31) | 7));
        /* The real confirmation callback accounts for these packets later. */
        assert(!vif.net_stats.tx_packets && !vif.net_stats.tx_bytes);
        assert(!hw.stats.last_tx);
    } else {
        assert(hdr.desc.host.status_desc_addr == (fixed_rate ? ((1U << 30) | 0x123) : 0));
        assert(vif.net_stats.tx_packets == 1 && vif.net_stats.tx_bytes == 24);
        assert(hw.stats.last_tx == jiffies);
    }
    assert(hdr.desc.host.flags == (mgmt ? TXU_CNTRL_MGMT : 0));
}
int main(void) {
    const unsigned char injected[] = {0x80, 0x40, 0xd0, 0xb0, 0xc0, 0x08};
    for (unsigned int i = 0; i < sizeof(injected); i++) {
        check(injected[i], 1, 1, 0, 1, 1);
        check(injected[i], 1, 1, 0, 0, 1);
    }
    check(0xd0, 0, 1, 0, 0, 1);
    check(0x10, 0, 1, 0, 0, 1);
    check(0x30, 0, 1, 0, 0, 1);
    check(0x80, 0, 1, 0, 0, 0);
    check(0xd0, 0, 0, 0, 0, 0);
    check(0x08, 0, 0, 0, 0, 0);
    check(0x08, 0, 0, 0, 1, 0);
    check(0x08, 0, 0, 0x8e88, 0, 1);
#ifdef AICWF_USB_SUPPORT
    check(0x08, 0, 0, 0xb488, 0, 1);
#else
    check(0x08, 0, 0, 0xb488, 0, 0);
#endif
    return 0;
}
'''


class MonitorTxTests(unittest.TestCase):
    def test_descriptor_registration_and_accounting(self):
        source = args.source.read_text()
        start = source.index("#ifdef  AICWF_SDIO_SUPPORT", source.index("void rwnx_tx_push("))
        end = source.index("\n#endif", source.index("aicwf_frame_tx((void *)(rwnx_hw->usbdev)", start))
        block = source[start:end] + "\n#endif\n"
        for bus in ("USB", "SDIO"):
            with self.subTest(bus=bus):
                with tempfile.TemporaryDirectory(prefix="aic-monitor-tx-") as temp:
                    c_file, binary = Path(temp) / "test.c", Path(temp) / "test"
                    c_file.write_text(PRELUDE + block + CHECKS)
                    subprocess.run([os.environ.get("CC", "cc"), "-std=gnu11",
                                    "-DAICWF_" + bus + "_SUPPORT", "-o", str(binary),
                                    str(c_file)], check=True)
                    subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main(argv=[__file__] + remaining)
