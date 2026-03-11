#include "rwnx_main.h"
#include "rwnx_msg_tx.h"
#include "reg_access.h"
#include "rwnx_platform.h"
#include "aicwf_compat_8800d80n.h"

#define FW_USERCONFIG_NAME_8800D80N         "aic_userconfig_8800d80n.txt"
#define FW_POWERLIMIT_NAME_8800D80N         "aic_powerlimit_8800d80n.txt"
#define RWNX_MAC_FW_RF_BASE_NAME_8800D80N   "lmacfw_rf_8800d80n.bin"
#define RWNX_MAC_FW_INITVAR_NAME_8800D80N   "fmacfw_initvar_8800d80n.bin"
#define RWNX_MAC_FW_GAINTBL_NAME_8800D80N   "fmacfw_gaintbl_8800d80n.bin"
#define RWNX_MAC_FW_CINIT_NAME_8800D80N_U02 "fmacfw_cinit_8800d80n_u02.bin"
#define RWNX_MAC_FW_CALIB_NAME_8800D80N_U02 "fmacfw_calib_8800d80n_u02.bin"
#define RWNX_MAC_PATCH_NAME_8800D80N_U02    "fmacfw_patch_8800d80n_u02.bin"
#define RWNX_MAC_PATCHTBL_NAME_8800D80N_U02 "fmacfw_patch_tbl_8800d80n_u02.bin"

#define RAM_LMAC_FW_RF_ADDR_8800D80N        0x00132C00
#define ROM_FMAC_CINIT_ADDR_8800D80N_U02    0x00133000
#define ROM_FMAC_CALIB_ADDR_8800D80N_U02    0x00138000
#define ROM_FMAC_PATCH_ADDR_8800D80N_U02    0x00188000

#define CHIP_INFO_FLAG_CINIT_BEGIN          (0x01U << 12)
#define CHIP_INFO_FLAG_CINIT_DONE           (0x01U << 13)

#define PATCH_VAR_FLAG_CALIB_BEGIN          (0x01U << 0)
#define PATCH_VAR_FLAG_CALIB_DONE           (0x01U << 1)

#define WF_RXGAIN_TBL_IDX_MAX       20
#define WF_RXGAIN_TBL_SIZE          256
#define WF_TXGAIN_TBL_IDX_MAX       21
#define WF_TXGAIN_TBL_SIZE          128

extern char aic_fw_path[200];

int rwnx_plat_bin_fw_upload_2(struct rwnx_hw *rwnx_hw, u32 fw_addr,
                               char *filename);
int rwnx_plat_bin_fw_upload_2_with_version(struct rwnx_hw *rwnx_hw, u32 fw_addr,
                               char *filename, char *version_str, int version_size);
int rwnx_request_firmware_common(struct rwnx_hw *rwnx_hw,
	u32** buffer, const char *filename);
void rwnx_plat_userconfig_parsing(char *buffer, int size);
void rwnx_release_firmware_common(u32** buffer);

extern int get_adap_test(void);

typedef u32 (*array2_tbl_t)[2];
typedef u32 (*array3_tbl_t)[3];

u32 syscfg_tbl_masked_8800d80n[][3] = {
    // {Address, mask, value}
    #if DEF_PATCH_METHOD_VER_1 // moved to cinit.bin
    // anareg1, u01 only
    {0x4050201C, (0x7 << 0), (0x7 << 0)},
    // pmic_pmu_init
    {0x70001024, ((0x1F << 0) | (0x1 << 14) | (0x1 << 15)),
                 ((0x08 << 0) | (0x1 << 14) | (0x1 << 15))},
    {0x70001044, (0xF << 20), (0x4 << 20)},
    {0x70001084, ((0xF << 23) | (0x1 << 27) | (0x1 << 31)),
                 ((0x4 << 23) | (0x0 << 27) | (0x1 << 31))},
    {0x70002118, (0x3 << 0), (0x0 << 0)},
    {0x7000211C, (0x3 << 2), (0x0 << 2)},
    {0x700021D8, (0x3 << 4), (0x0 << 4)},
    {0x700021D0, ((0x1 << 5) | (0x1 << 6)),
                 ((0x1 << 5) | (0x1 << 6))},
    #if defined(CONFIG_VRF_DCDC_MODE)
    {0x70001000, (0x1 << 0), (0x1 << 0)},
    #else
    {0x70001000, (0x1 << 0), (0x0 << 0)},
    #endif
    // cm_rf_init
    {0x4050500C, ((0x7 << 27) | (0x3 << 30)),
                 ((0x3 << 27) | (0x3 << 30))},
    {0x40505008, ((0x3 << 11) | (0xF << 15)),
                 ((0x1 << 11) | (0x4 << 15))},
    #endif
    {0x00000000, 0x00000000, 0x00000000}, // last one
};

u32 patch_tbl_wifisetting_8800d80n[][2] =
{
    //{0x00b8, 0x00009d08 | (0x01U << 13)}, // debug_mask, bit13: CALIB_BIT
};

//adap test
u32 adaptivity_patch_tbl_8800d80n[][2] = {
};

u32 patch_tbl_rf_func_8800d80n[][2] =
{
};

extern int testmode;
extern u8 chip_id;
extern u8 chip_mcu_id;

void system_config_8800d80n(struct rwnx_hw *rwnx_hw)
{
    int syscfg_num;
    array3_tbl_t p_syscfg_msk_tbl;
    int ret, cnt;
    const u32 mem_addr = 0x40500000;
    struct dbg_mem_read_cfm rd_mem_addr_cfm;

    ret = rwnx_send_dbg_mem_read_req(rwnx_hw, mem_addr, &rd_mem_addr_cfm);
    if (ret) {
        AICWFDBG(LOGERROR, "%x rd fail: %d\n", mem_addr, ret);
        return;
    }
    chip_id = (u8)(rd_mem_addr_cfm.memdata >> 16);
    //printk("%x=%x\n", rd_mem_addr_cfm.memaddr, rd_mem_addr_cfm.memdata);
    if (((rd_mem_addr_cfm.memdata >> 25) & 0x01UL) == 0x00UL) {
        chip_mcu_id = 1;
    }

    ret = rwnx_send_dbg_mem_read_req(rwnx_hw, 0x00000020, &rd_mem_addr_cfm);
    if (ret) {
        AICWFDBG(LOGERROR, "[0x00000020] rd fail: %d\n", ret);
        return;
    }
    chip_sub_id = (u8)(rd_mem_addr_cfm.memdata);
    //printk("%x=%x\n", rd_mem_addr_cfm.memaddr, rd_mem_addr_cfm.memdata);
    AICWFDBG(LOGINFO, "chip_id=%x, chip_sub_id=%x\n", chip_id, chip_sub_id);

    syscfg_num = sizeof(syscfg_tbl_masked_8800d80n) / sizeof(u32) / 3;
    p_syscfg_msk_tbl = syscfg_tbl_masked_8800d80n;

    for (cnt = 0; cnt < syscfg_num; cnt++) {
        if (p_syscfg_msk_tbl[cnt][0] == 0x00000000) {
            break;
        }

        ret = rwnx_send_dbg_mem_mask_write_req(rwnx_hw,
            p_syscfg_msk_tbl[cnt][0], p_syscfg_msk_tbl[cnt][1], p_syscfg_msk_tbl[cnt][2]);
        if (ret) {
            AICWFDBG(LOGERROR, "%x mask write fail: %d\n", p_syscfg_msk_tbl[cnt][0], ret);
            return;
        }
    }
}

int aicwf_plat_patch_load_8800d80n(struct rwnx_hw *rwnx_hw)
{
    int ret = 0;
    char patch_ver_str[128];
    //wifi patch
    ret = rwnx_plat_bin_fw_upload_2_with_version(rwnx_hw,
        ROM_FMAC_PATCH_ADDR_8800D80N_U02, RWNX_MAC_PATCH_NAME_8800D80N_U02, patch_ver_str, sizeof(patch_ver_str));
    if (ret) {
        AICWFDBG(LOGERROR, "load patch bin fail: %d\n", ret);
        return ret;
    }
    AICWFDBG(LOGINFO, "PatchVer: %s", patch_ver_str);
    return ret;
}

int aicwf_plat_patch_table_load_8800d80n(struct rwnx_hw *rwnx_hw)
{
    int err = 0;
    unsigned int i, size;
    u32 *dst = NULL;
    char *filename = RWNX_MAC_PATCHTBL_NAME_8800D80N_U02;

    /* Copy the file on the Embedded side */
    AICWFDBG(LOGINFO, "### Upload %s \n", filename);

    size = rwnx_request_firmware_common(rwnx_hw, &dst, filename);
    if (!dst) {
       AICWFDBG(LOGERROR, "No such file or directory\n");
       return -1;
    }
    if (size <= 0) {
        AICWFDBG(LOGERROR, "wrong size of firmware file\n");
        dst = NULL;
        err = -1;
    }
    AICWFDBG(LOGINFO, "tbl size = %d \n", size);

    if (!err) {
        for (i = 0; i < (size / 4); i += 2) {
            if ((dst[i] == 0x0) || (dst[i] == 0xFFFFFFFF)) {
                break; // end of tbl
            }
            AICWFDBG(LOGERROR, "patch_tbl:  %x  %x\n", dst[i], dst[i+1]);
            err = rwnx_send_dbg_mem_write_req(rwnx_hw, dst[i], dst[i+1]);
        }
        if (err) {
            AICWFDBG(LOGERROR, "tbl bin upload fail: %x, err:%d\r\n", dst[i], err);
        }
    }

    if (dst) {
        rwnx_release_firmware_common(&dst);
    }

   return err;

}

void aicwf_patch_config_8800d80n(struct rwnx_hw *rwnx_hw)
{
    #ifdef CONFIG_ROM_PATCH_EN
    int ret = 0;
    int cnt = 0;

//adap test
    int adap_test = 0;
    int adap_patch_num = 0;

    adap_test = get_adap_test();
//adap test

    if (testmode == 0) {
        const u32 cfg_base        = 0x10170;
        struct dbg_mem_read_cfm cfm;
        //int i;
        u32 wifisetting_cfg_addr;
        u32 agc_cfg_addr;
        u32 txgain_cfg_24g_addr, txgain_cfg_5g_addr;
        u32 jump_tbl_addr = 0;

        u32 patch_tbl_wifisetting_num = sizeof(patch_tbl_wifisetting_8800d80n)/sizeof(u32)/2;
        //u32 jump_tbl_size = 0;
        //u32 patch_tbl_func_num = 0;

        //array2_tbl_t jump_tbl_base = NULL;
        //array2_tbl_t patch_tbl_func_base = NULL;

        if ((ret = rwnx_send_dbg_mem_read_req(rwnx_hw, cfg_base, &cfm))) {
            AICWFDBG(LOGERROR, "setting base[0x%x] rd fail: %d\n", cfg_base, ret);
        }
        wifisetting_cfg_addr = cfm.memdata;

        if ((ret = rwnx_send_dbg_mem_read_req(rwnx_hw, cfg_base + 4, &cfm))) {
             AICWFDBG(LOGERROR, "jump_tbl base[0x%x] rd fail: %d\n", cfg_base + 4, ret);
        }
        jump_tbl_addr = cfm.memdata;

        if ((ret = rwnx_send_dbg_mem_read_req(rwnx_hw, cfg_base + 0x10, &cfm))) {
            AICWFDBG(LOGERROR, "agc_cfg base[0x%x] rd fail: %d\n", cfg_base + 0xc, ret);
        }
        agc_cfg_addr = cfm.memdata;

        if ((ret = rwnx_send_dbg_mem_read_req(rwnx_hw, cfg_base + 0x14, &cfm))) {
            AICWFDBG(LOGERROR, "txgain_cfg_24g base[0x%x] rd fail: %d\n", cfg_base + 0x10, ret);
        }
        txgain_cfg_24g_addr = cfm.memdata;

        if ((ret = rwnx_send_dbg_mem_read_req(rwnx_hw, cfg_base + 0x18, &cfm))) {
            AICWFDBG(LOGERROR, "txgain_cfg_5g base[0x%x] rd fail: %d\n", cfg_base + 0x10, ret);
        }
        txgain_cfg_5g_addr = cfm.memdata;

        AICWFDBG(LOGINFO, "wifisetting_cfg_addr=%x, jump_tbl_addr=%x, agc_cfg_addr=%x, txgain_cfg_24g_addr=%x, txgain_cfg_5g_addr=%x\n",
            wifisetting_cfg_addr, jump_tbl_addr, agc_cfg_addr, txgain_cfg_24g_addr, txgain_cfg_5g_addr);

        for (cnt = 0; cnt < patch_tbl_wifisetting_num; cnt++) {
            if ((ret = rwnx_send_dbg_mem_write_req(rwnx_hw, wifisetting_cfg_addr + patch_tbl_wifisetting_8800d80n[cnt][0], patch_tbl_wifisetting_8800d80n[cnt][1]))) {
                AICWFDBG(LOGERROR, "wifisetting %x write fail\n", patch_tbl_wifisetting_8800d80n[cnt][0]);
            }
        }

        //adap test
        if (adap_test) {
            adap_patch_num = sizeof(adaptivity_patch_tbl_8800d80n)/sizeof(u32)/2;
            for(cnt = 0; cnt < adap_patch_num; cnt++)
            {
                if((ret = rwnx_send_dbg_mem_write_req(rwnx_hw, wifisetting_cfg_addr + adaptivity_patch_tbl_8800d80n[cnt][0], adaptivity_patch_tbl_8800d80n[cnt][1]))) {
                    AICWFDBG(LOGERROR, "%x write fail\n", wifisetting_cfg_addr + adaptivity_patch_tbl_8800d80n[cnt][0]);
                }
            }
        }
        //adap test

        #if DEF_PATCH_METHOD_VER_1
        ret = aicwf_plat_initvar_load_8800d80n(rwnx_hw, txgain_cfg_24g_addr);
        if (ret) {
            AICWFDBG(LOGERROR, "initvar load fail: %d\n", ret);
            return;
        }
        #endif
    }
    else {
        u32 patch_tbl_rf_func_num = sizeof(patch_tbl_rf_func_8800d80n)/sizeof(u32)/2;
        for (cnt = 0; cnt < patch_tbl_rf_func_num; cnt++) {
            if ((ret = rwnx_send_dbg_mem_write_req(rwnx_hw, patch_tbl_rf_func_8800d80n[cnt][0], patch_tbl_rf_func_8800d80n[cnt][1]))) {
                AICWFDBG(LOGERROR, "patch_tbl_rf_func %x write fail\n", patch_tbl_rf_func_8800d80n[cnt][0]);
            }
        }
    }
    #endif
}

int aicwf_plat_rftest_load_8800d80n(struct rwnx_hw *rwnx_hw)
{
    int ret = 0;
    ret = rwnx_plat_bin_fw_upload_2(rwnx_hw, RAM_LMAC_FW_RF_ADDR_8800D80N, RWNX_MAC_FW_RF_BASE_NAME_8800D80N);
    if (ret) {
        AICWFDBG(LOGINFO, "load rftest bin fail: %d\n", ret);
        return ret;
    }
    return ret;
}

int aicwf_plat_rftest_exec_8800d80n(struct rwnx_hw *rwnx_hw)
{
    int ret = 0;
    uint32_t fw_addr, boot_type;
    uint32_t rst_hdlr_addr = RAM_LMAC_FW_RF_ADDR_8800D80N + 0x04;
    uint32_t rst_hdlr_val;
    struct dbg_mem_read_cfm rd_mem_addr_cfm;
    ret = rwnx_send_dbg_mem_read_req(rwnx_hw, rst_hdlr_addr, &rd_mem_addr_cfm);
    if (ret) {
        AICWFDBG(LOGERROR, "%x rd fail: %d\n", rst_hdlr_addr, ret);
        return ret;
    }
    rst_hdlr_val = rd_mem_addr_cfm.memdata;
    if ((rst_hdlr_val & ~0x03FF) == RAM_LMAC_FW_RF_ADDR_8800D80N) {
        AICWFDBG(LOGINFO, "rftest loaded, hdlr=%x\n", rst_hdlr_val);
    }
    /* fw start */
    fw_addr = RAM_LMAC_FW_RF_ADDR_8800D80N;
    boot_type = HOST_START_APP_AUTO;
    AICWFDBG(LOGINFO, "Start app: %08x, %d\n", fw_addr, boot_type);
    ret = rwnx_send_dbg_start_app_req(rwnx_hw, fw_addr, boot_type);
    if (ret) {
        AICWFDBG(LOGERROR, "start app fail: %d\n", ret);
        return ret;
    }
    return ret;
}

#if DEF_PATCH_METHOD_VER_1
int aicwf_plat_initvar_load_8800d80n(struct rwnx_hw *rwnx_hw, u32 var_base_addr)
{
    int ret = 0;
    ret = rwnx_plat_bin_fw_upload_2(rwnx_hw, var_base_addr, RWNX_MAC_FW_INITVAR_NAME_8800D80N);
    if (ret) {
        AICWFDBG(LOGERROR, "load initvar bin fail: %d\n", ret);
        return ret;
    }
    return ret;
}

int aicwf_plat_gain_table_load_8800d80n(struct rwnx_hw *rwnx_hw)
{
    int size, desired_size, idx, ret;
    u32 *dst=NULL;
    u8_l *tbl_base;
    char *filename = RWNX_MAC_FW_GAINTBL_NAME_8800D80N;

    AICWFDBG(LOGINFO, "gaintbl file path:%s\n", filename);

    /* load file */
    size = rwnx_request_firmware_common(rwnx_hw, &dst, filename);
    desired_size = WF_RXGAIN_TBL_IDX_MAX * WF_RXGAIN_TBL_SIZE + WF_TXGAIN_TBL_IDX_MAX * WF_TXGAIN_TBL_SIZE;
    if ((size <= 0) || (size != desired_size)) {
        AICWFDBG(LOGERROR, "wrong size of firmware file, desired=%d, size=%d\n", desired_size, size);
        if (dst) {
            rwnx_release_firmware_common(&dst);
            dst = NULL;
        }
        return 0;
    }

    /* Copy the file on the Embedded side */
    AICWFDBG(LOGINFO, "### Load file done: %s, size=%d\n", filename, size);

    /* RX gain */
    tbl_base = (u8_l *)dst;
    for (idx = 0; idx < WF_RXGAIN_TBL_IDX_MAX; idx++) {
        u16_l ofst = idx * 16;
        u8_l *tbl_ptr = tbl_base + idx * WF_RXGAIN_TBL_SIZE;
        ret = rwnx_send_rf_config_v2_req(rwnx_hw, ofst, 0, tbl_ptr, WF_RXGAIN_TBL_SIZE);
        if (ret) {
            AICWFDBG(LOGERROR, "rx gain rf_config_req fail, ret=%d, ofst=%d\n", ret, ofst);
            break;
        }
    }

    /* TX gain */
    tbl_base = (u8_l *)dst + WF_RXGAIN_TBL_IDX_MAX * WF_RXGAIN_TBL_SIZE;
    for (idx = 0; idx < WF_TXGAIN_TBL_IDX_MAX; idx++) {
        u16_l ofst = idx * 16;
        u8_l *tbl_ptr = tbl_base + idx * WF_TXGAIN_TBL_SIZE;
        ret = rwnx_send_rf_config_v2_req(rwnx_hw, ofst, 2, tbl_ptr, WF_TXGAIN_TBL_SIZE);
        if (ret) {
            AICWFDBG(LOGERROR, "tx gain rf_config_req fail, ret=%d, ofst=%d\n", ret, ofst);
            break;
        }
    }

    rwnx_release_firmware_common(&dst);

    AICWFDBG(LOGINFO, "gaintbl download complete\n\n");
    return 0;
}
#endif

#if DEF_PATCH_METHOD_VER_2
int aicwf_plat_cinit_exec_8800d80n(struct rwnx_hw *rwnx_hw)
{
    int ret = 0;
    uint32_t fw_addr, boot_type;
    uint32_t mem_addr = 0x40500184;
    uint32_t mem_val;
    struct dbg_mem_read_cfm rd_mem_addr_cfm;
    ret = rwnx_send_dbg_mem_read_req(rwnx_hw, mem_addr, &rd_mem_addr_cfm);
    if (ret) {
        AICWFDBG(LOGERROR, "%x rd fail: %d\n", mem_addr, ret);
        return ret;
    }
    mem_val = rd_mem_addr_cfm.memdata;
    if (mem_val & CHIP_INFO_FLAG_CINIT_BEGIN) {
        // wait done
        while (!(mem_val & CHIP_INFO_FLAG_CINIT_DONE)) {
            AICWFDBG(LOGINFO, "cinit rd chipinfo=%x\n", mem_val);
            ret = rwnx_send_dbg_mem_read_req(rwnx_hw, mem_addr, &rd_mem_addr_cfm);
            if (ret) {
                AICWFDBG(LOGERROR, "%x rd fail: %d\n", mem_addr, ret);
                return ret;
            }
            mem_val = rd_mem_addr_cfm.memdata;
        }
        AICWFDBG(LOGINFO, "cinit executed, chipinfo=%x\n", mem_val);
        return ret;
    }
    ret = rwnx_plat_bin_fw_upload_2(rwnx_hw, ROM_FMAC_CINIT_ADDR_8800D80N_U02, RWNX_MAC_FW_CINIT_NAME_8800D80N_U02);
    if (ret) {
        AICWFDBG(LOGERROR, "load cinit bin fail: %d\n", ret);
        return ret;
    }
    /* fw start */
    fw_addr = ROM_FMAC_CINIT_ADDR_8800D80N_U02 + 0x0009;
    boot_type = HOST_START_APP_FNCALL;
    AICWFDBG(LOGINFO, "Start app: %08x, %d\n", fw_addr, boot_type);
    ret = rwnx_send_dbg_start_app_req(rwnx_hw, fw_addr, boot_type);
    if (ret) {
        AICWFDBG(LOGERROR, "start app fail: %d\n", ret);
        return ret;
    }
    return ret;
}

int aicwf_plat_calib_exec_8800d80n(struct rwnx_hw *rwnx_hw)
{
    int ret = 0;
    uint32_t fw_addr, boot_type;
    uint32_t patch_var_flags_addr = ROM_FMAC_PATCH_ADDR_8800D80N_U02 + 0x08;
    uint32_t patch_var_flags_val;
    struct dbg_mem_read_cfm rd_mem_addr_cfm;
    ret = rwnx_send_dbg_mem_read_req(rwnx_hw, patch_var_flags_addr, &rd_mem_addr_cfm);
    if (ret) {
        AICWFDBG(LOGERROR, "%x rd fail: %d\n", patch_var_flags_addr, ret);
        return ret;
    }
    patch_var_flags_val = rd_mem_addr_cfm.memdata;
    if (patch_var_flags_val & (PATCH_VAR_FLAG_CALIB_BEGIN | PATCH_VAR_FLAG_CALIB_DONE)) {
        AICWFDBG(LOGINFO, "calib executed, var_flags=%x\n", patch_var_flags_val);
        return ret;
    }
    ret = rwnx_plat_bin_fw_upload_2(rwnx_hw, ROM_FMAC_CALIB_ADDR_8800D80N_U02, RWNX_MAC_FW_CALIB_NAME_8800D80N_U02);
    if (ret) {
        AICWFDBG(LOGERROR, "load calib bin fail: %d\n", ret);
        return ret;
    }
    /* fw start */
    fw_addr = ROM_FMAC_CALIB_ADDR_8800D80N_U02 + 0x0009;
    boot_type = HOST_START_APP_FNCALL;
    AICWFDBG(LOGINFO, "Start app: %08x, %d\n", fw_addr, boot_type);
    ret = rwnx_send_dbg_start_app_req(rwnx_hw, fw_addr, boot_type);
    if (ret) {
        AICWFDBG(LOGERROR, "start app fail: %d\n", ret);
        return ret;
    }
    return ret;
}
#endif

int aicwf_set_rf_config_8800d80n(struct rwnx_hw *rwnx_hw, struct mm_set_rf_calib_cfm *cfm)
{
	int ret = 0;

	if ((ret = rwnx_send_txpwr_lvl_v3_req(rwnx_hw))) {
		return -1;
	}
	if ((ret = rwnx_send_txpwr_lvl_adj_req(rwnx_hw))) {
		return -1;
	}
	if ((ret = rwnx_send_txpwr_ofst2x_req(rwnx_hw))) {
		return -1;
	}
    #if DEF_PATCH_METHOD_VER_1
    if (testmode == FW_NORMAL_MODE) {
        aicwf_plat_gain_table_load_8800d80n(rwnx_hw);
    }
    #endif
	if ((ret = rwnx_send_rf_calib_req(rwnx_hw, cfm))) {
		return -1;
	}
	return 0 ;
}


int	rwnx_plat_userconfig_load_8800d80n(struct rwnx_hw *rwnx_hw){
    int size;
    u32 *dst=NULL;
    char *filename = FW_USERCONFIG_NAME_8800D80N;

    AICWFDBG(LOGINFO, "userconfig file path:%s \r\n", filename);

    /* load file */
    size = rwnx_request_firmware_common(rwnx_hw, &dst, filename);
    if (size <= 0) {
            AICWFDBG(LOGERROR, "wrong size of firmware file\n");
            dst = NULL;
            return 0;
    }

    /* Copy the file on the Embedded side */
    AICWFDBG(LOGINFO, "### Load file done: %s, size=%d\n", filename, size);

    rwnx_plat_userconfig_parsing((char *)dst, size);

    rwnx_release_firmware_common(&dst);

    AICWFDBG(LOGINFO, "userconfig download complete\n\n");
    return 0;

}

#ifdef CONFIG_POWER_LIMIT
extern char country_code[];
int rwnx_plat_powerlimit_load_8800d80n(struct rwnx_hw *rwnx_hw)
{
    int size;
    u32 *dst=NULL;
    char *filename = FW_POWERLIMIT_NAME_8800D80N;

    AICWFDBG(LOGINFO, "powerlimit file path:%s \r\n", filename);

    /* load file */
    size = rwnx_request_firmware_common(rwnx_hw, &dst, filename);
    if (size <= 0) {
        AICWFDBG(LOGERROR, "wrong size of cfg file\n");
        dst = NULL;
        return 0;
    }

    AICWFDBG(LOGINFO, "### Load file done: %s, size=%d\n", filename, size);

    /* parsing the file */
    rwnx_plat_powerlimit_parsing((char *)dst, size, country_code);

    rwnx_release_firmware_common(&dst);

    AICWFDBG(LOGINFO, "powerlimit download complete\n\n");
    return 0;
}
#endif

