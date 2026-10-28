#!/usr/bin/env python3
"""Exercise extracted loader paths with mocked USB requests, without hardware."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def block(source, signature):
    start = source.index(signature)
    brace = source.index("{", start)
    end, depth = brace + 1, 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


loader = (ROOT / "drivers/aic8800/aic_load_fw/aic_compat_8800d80.c").read_text()
header = (ROOT / "drivers/aic8800/aic_load_fw/aic_compat_8800d80.h").read_text()
header = "\n".join(line for line in header.splitlines() if not line.startswith("#include"))
config = block(loader, "int system_config_8800d80(")
normal = block(loader, "if(testmode == FW_NORMAL_MODE)")
connect = (ROOT / "drivers/aic8800/aic8800_fdrv/rwnx_main.c").read_text()
guard = block(connect[connect.index("static int rwnx_cfg80211_connect("):],
              "if((int)atomic_read(&rwnx_vif->drv_conn_state) == (int)RWNX_DRV_STATUS_DISCONNECTING")
prelude = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
typedef uint32_t u32;
typedef uint8_t u8;
#define printk(...) ((void)0)
#define AICWFDBG(...) ((void)0)
#define CHIP_REV_U01 1
#define CHIP_REV_U02 3
#define CHIP_REV_U03 7
#define FW_NORMAL_MODE 0
#define HOST_START_APP_AUTO 0
struct aic_usb_dev {
    bool d80_m80_variant;
    u32 id, cache;
    int syscfg_writes, syscfg_masks, patches, fmacs, starts;
};
struct dbg_mem_read_cfm { u32 memdata; };
struct aicbt_patch_info_t { u32 addr_adid, addr_patch; };
static u8 chip_id, chip_mcu_id;
static int testmode = FW_NORMAL_MODE, fail_read, fail_write;
static u32 syscfg_tbl_8800d80[0][2], syscfg_tbl_masked_8800d80[0][3];
static int rwnx_send_dbg_mem_read_req(struct aic_usb_dev *dev, u32 addr,
                                    struct dbg_mem_read_cfm *cfm) {
    if (fail_read) return -EIO;
    cfm->memdata = addr == 0x40500000 ? dev->id : dev->cache; return 0;
}
static int rwnx_send_dbg_mem_write_req(struct aic_usb_dev *dev, u32 addr, u32 val) {
    if (fail_write) return -EIO;
    if (addr == 0x40100020) dev->cache = val;
    if (addr == 0x40500010) { assert(val == 2); dev->syscfg_writes++; }
    return 0;
}
static int rwnx_send_dbg_mem_mask_write_req(struct aic_usb_dev *dev, u32 addr, u32 mask, u32 val) {
    assert(addr == 0x40506024 && mask == 0xff && val == 0xdf);
    dev->syscfg_masks++; return 0;
}
static int rwnx_plat_bin_fw_upload_android(struct aic_usb_dev *dev, u32 addr, const char *name) {
    if (name[0] == 'f' && name[1] == 'm') {
        assert(addr == 0x120000); dev->fmacs++;
    } else { dev->patches++; }
    return 0;
}
static int aicbt_ext_patch_data_load(struct aic_usb_dev *dev, struct aicbt_patch_info_t *info, const char *name) {
    dev->patches++; return 0;
}
static int aicbt_patch_table_load(struct aic_usb_dev *dev, void *head) {
    dev->patches++; return 0;
}
static int rwnx_send_dbg_start_app_req(struct aic_usb_dev *dev, u32 addr, int mode) {
    assert(addr == 0x120000); dev->starts++; return 0;
}
'''
normal_wrapper = r'''
int aicwf_patch_config_8800d80(struct aic_usb_dev *dev) { dev->patches++; return 0; }
static int load_normal(struct aic_usb_dev *usb_dev) {
    struct aicbt_patch_info_t patch_info = {0};
    void *head = 0;
'''
busy_wrapper = r'''
return 0;
}
enum { RWNX_DRV_STATUS_DISCONNECTED, RWNX_DRV_STATUS_CONNECTING,
       RWNX_DRV_STATUS_DISCONNECTING };
struct vif { int drv_conn_state; };
#define atomic_read(p) (*(p))
static void rwnx_set_conn_state(struct vif *vif, int *state, int val) { *state = val; }
static int busy(struct vif *rwnx_vif) {
'''
checks = r'''
return 0;
}
int main(void) {
    struct aic_usb_dev m80 = {.id = 0x00078820}, d80 = {.id = 0x00078800};
    /* Configuring a second adapter must not change the first's load sequence. */
    assert(system_config_8800d80(&m80) == 0);
    assert(system_config_8800d80(&d80) == 0);
    assert(load_normal(&m80) == 0 && m80.patches == 0 && m80.fmacs == 1 && m80.starts == 1);
    assert(load_normal(&d80) == 0 && d80.patches == 5 && d80.fmacs == 1 && d80.starts == 1);
    assert(m80.syscfg_writes == 1 && m80.syscfg_masks == 1 && (m80.cache & 1));
    assert(d80.syscfg_writes == 0 && d80.syscfg_masks == 0 && (d80.cache & 1));
    /* Reusing a zeroed/reprobed device, and a fresh device after an M80. */
    m80.id = 0x00078800; m80.patches = m80.fmacs = m80.starts = 0;
    assert(system_config_8800d80(&m80) == 0);
    assert(load_normal(&m80) == 0 && m80.patches == 5);
    d80.id = 0x00078820; d80.patches = d80.fmacs = d80.starts = 0;
    assert(system_config_8800d80(&d80) == 0);
    assert(load_normal(&d80) == 0 && d80.patches == 0 && d80.fmacs == 1);
    fail_read = 1; assert(system_config_8800d80(&d80) == -EIO);
    fail_read = 0; fail_write = 1; assert(system_config_8800d80(&d80) == -EIO);
    for (int state = RWNX_DRV_STATUS_CONNECTING; state <= RWNX_DRV_STATUS_DISCONNECTING; state++) {
        struct vif vif = {.drv_conn_state = state};
        assert(busy(&vif) == -EBUSY && vif.drv_conn_state == state);
    }
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix="aic-8820-test-") as temp:
    c_file, binary = Path(temp) / "loader.c", Path(temp) / "loader"
    c_file.write_text(prelude + header + config + normal_wrapper + normal +
                      busy_wrapper + guard + checks)
    subprocess.run([os.environ.get("CC", "cc"), "-std=gnu11", "-o", str(binary),
                    str(c_file)], check=True)
    subprocess.run([str(binary)], check=True)
print("PASS: per-device/reprobe loader selection, MCU1 cache fix, write errors, busy connect state")

