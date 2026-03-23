#include "rwnx_main.h"
#include "rwnx_msg_tx.h"
#include "reg_access.h"
#include "rwnx_platform.h"
#include "aicwf_compat_8800dln.h"

#define FW_USERCONFIG_NAME_8800DLN          "aic_userconfig_8800dln.txt"
#define FW_POWERLIMIT_NAME_8800DLN          "aic_powerlimit_8800dln.txt"
#define RWNX_MAC_FW_RF_BASE_NAME_8800DLN    "lmacfw_rf_8800dln.bin"
#define RWNX_MAC_FW_INITVAR_NAME_8800DLN    "fmacfw_initvar_8800dln.bin"
#define RWNX_MAC_FW_GAINTBL_NAME_8800DLN    "fmacfw_gaintbl_8800dln.bin"

#define WF_RXGAIN_TBL_IDX_MAX       20
#define WF_RXGAIN_TBL_SIZE          256
#define WF_TXGAIN_TBL_IDX_MAX       21
#define WF_TXGAIN_TBL_SIZE          128

extern char aic_fw_path[200];

int rwnx_plat_bin_fw_upload_2(struct rwnx_hw *rwnx_hw, u32 fw_addr,
                               char *filename);
int rwnx_request_firmware_common(struct rwnx_hw *rwnx_hw,
	u32** buffer, const char *filename);
void rwnx_plat_userconfig_parsing(char *buffer, int size);
void rwnx_release_firmware_common(u32** buffer);

extern int get_adap_test(void);

typedef u32 (*array2_tbl_t)[2];
typedef u32 (*array3_tbl_t)[3];


u32 syscfg_tbl_masked_8800dln[][3] = {
    // {Address, mask, value}
    {0x00000000, 0x00000000, 0x00000000}, // last one
};

u32 patch_tbl_wifisetting_8800dln[][2] =
{
};

//adap test
u32 adaptivity_patch_tbl_8800dln[][2] = {
};

u32 patch_tbl_rf_func_8800dln[][2] =
{
};

extern int testmode;
extern u8 chip_id;
extern u8 chip_mcu_id;

void system_config_8800dln(struct rwnx_hw *rwnx_hw)
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

    syscfg_num = sizeof(syscfg_tbl_masked_8800dln) / sizeof(u32) / 3;
    p_syscfg_msk_tbl = syscfg_tbl_masked_8800dln;

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

void aicwf_patch_config_8800dln(struct rwnx_hw *rwnx_hw)
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
        const u32 cfg_base        = 0x8150;
        struct dbg_mem_read_cfm cfm;
        u32 wifisetting_cfg_addr;
        u32 agc_cfg_addr;
        u32 txgain_cfg_24g_addr;
        u32 jump_tbl_addr = 0;

        u32 patch_tbl_num;

        //array2_tbl_t jump_tbl_base = NULL;
        array2_tbl_t patch_tbl_base = NULL;

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

        AICWFDBG(LOGINFO, "wifisetting_cfg_addr=%x, jump_tbl_addr=%x, agc_cfg_addr=%x, txgain_cfg_24g_addr=%x\n",
            wifisetting_cfg_addr, jump_tbl_addr, agc_cfg_addr, txgain_cfg_24g_addr);

        patch_tbl_num = sizeof(patch_tbl_wifisetting_8800dln)/sizeof(u32)/2;
        patch_tbl_base = patch_tbl_wifisetting_8800dln;
        for (cnt = 0; cnt < patch_tbl_num; cnt++) {
            if ((ret = rwnx_send_dbg_mem_write_req(rwnx_hw, wifisetting_cfg_addr + patch_tbl_base[cnt][0], patch_tbl_base[cnt][1]))) {
                AICWFDBG(LOGERROR, "wifisetting %x write fail\n", patch_tbl_base[cnt][0]);
            }
        }

        //adap test
        if (adap_test) {
            adap_patch_num = sizeof(adaptivity_patch_tbl_8800dln)/sizeof(u32)/2;
            patch_tbl_base = adaptivity_patch_tbl_8800dln;
            for(cnt = 0; cnt < adap_patch_num; cnt++)
            {
                if((ret = rwnx_send_dbg_mem_write_req(rwnx_hw, wifisetting_cfg_addr + patch_tbl_base[cnt][0], patch_tbl_base[cnt][1]))) {
                    AICWFDBG(LOGERROR, "%x write fail\n", wifisetting_cfg_addr + patch_tbl_base[cnt][0]);
                }
            }
        }
        //adap test

        ret = rwnx_plat_bin_fw_upload_2(rwnx_hw, txgain_cfg_24g_addr, RWNX_MAC_FW_INITVAR_NAME_8800DLN);
        if (ret) {
            AICWFDBG(LOGINFO, "load initvar bin fail: %d\n", ret);
            return;
        }
    }
    else {
        u32 patch_tbl_rf_func_num = sizeof(patch_tbl_rf_func_8800dln)/sizeof(u32)/2;
        for (cnt = 0; cnt < patch_tbl_rf_func_num; cnt++) {
            if ((ret = rwnx_send_dbg_mem_write_req(rwnx_hw, patch_tbl_rf_func_8800dln[cnt][0], patch_tbl_rf_func_8800dln[cnt][1]))) {
                AICWFDBG(LOGERROR, "patch_tbl_rf_func %x write fail\n", patch_tbl_rf_func_8800dln[cnt][0]);
            }
        }
    }
    #endif
}

int aicwf_plat_rftest_load_8800dln(struct rwnx_hw *rwnx_hw)
{
    int ret = 0;
    ret = rwnx_plat_bin_fw_upload_2(rwnx_hw, RAM_LMAC_FW_ADDR, RWNX_MAC_FW_RF_BASE_NAME_8800DLN);
    if (ret) {
        AICWFDBG(LOGINFO, "load rftest bin fail: %d\n", ret);
        return ret;
    }
    return ret;
}

int aicwf_plat_gain_table_load_8800dln(struct rwnx_hw *rwnx_hw)
{
    int size, desired_size, idx, ret;
    u32 *dst=NULL;
    u8_l *tbl_base;
    char *filename = RWNX_MAC_FW_GAINTBL_NAME_8800DLN;

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

int aicwf_set_rf_config_8800dln(struct rwnx_hw *rwnx_hw, struct mm_set_rf_calib_cfm *cfm)
{
	int ret = 0;

	if ((ret = rwnx_send_txpwr_lvl_req(rwnx_hw))) {
		return -1;
	}
	if ((ret = rwnx_send_txpwr_lvl_adj_v2_req(rwnx_hw))) {
		return -1;
	}
	if ((ret = rwnx_send_txpwr_ofst2x_v3_req(rwnx_hw))) {
		return -1;
	}
    if (testmode == FW_NORMAL_MODE) {
        aicwf_plat_gain_table_load_8800dln(rwnx_hw);
    }
	if ((ret = rwnx_send_rf_calib_req(rwnx_hw, cfm))) {
		return -1;
	}
	return 0 ;
}


int	rwnx_plat_userconfig_load_8800dln(struct rwnx_hw *rwnx_hw){
    int size;
    u32 *dst=NULL;
    char *filename = FW_USERCONFIG_NAME_8800DLN;

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
int rwnx_plat_powerlimit_load_8800dln(struct rwnx_hw *rwnx_hw)
{
    int size;
    u32 *dst=NULL;
    char *filename = FW_POWERLIMIT_NAME_8800DLN;

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
    rwnx_plat_powerlimit_parsing((char *)dst, size);

    rwnx_release_firmware_common(&dst);

    AICWFDBG(LOGINFO, "powerlimit download complete\n\n");
    return 0;
}
#endif

