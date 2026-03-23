#include <linux/types.h>

#define DEF_PATCH_METHOD_VER_1  0 // deprecated
#define DEF_PATCH_METHOD_VER_2  1 // in used

void system_config_8800d80n(struct rwnx_hw *rwnx_hw);

int aicwf_plat_patch_load_8800d80n(struct rwnx_hw *rwnx_hw);
int aicwf_plat_patch_table_load_8800d80n(struct rwnx_hw *rwnx_hw);

void aicwf_patch_config_8800d80n(struct rwnx_hw *rwnx_hw);

int aicwf_plat_rftest_load_8800d80n(struct rwnx_hw *rwnx_hw);
int aicwf_plat_rftest_exec_8800d80n(struct rwnx_hw *rwnx_hw);

#if DEF_PATCH_METHOD_VER_1
int aicwf_plat_initvar_load_8800d80n(struct rwnx_hw *rwnx_hw, u32 var_base_addr);
int aicwf_plat_gain_table_load_8800d80n(struct rwnx_hw *rwnx_hw);
#endif

#if DEF_PATCH_METHOD_VER_2
int aicwf_plat_cinit_exec_8800d80n(struct rwnx_hw *rwnx_hw);
int aicwf_plat_calib_exec_8800d80n(struct rwnx_hw *rwnx_hw);
#endif

int rwnx_plat_userconfig_load_8800d80n(struct rwnx_hw *rwnx_hw);
#ifdef CONFIG_POWER_LIMIT
int rwnx_plat_powerlimit_load_8800d80n(struct rwnx_hw *rwnx_hw);
#endif
int aicwf_set_rf_config_8800d80n(struct rwnx_hw *rwnx_hw, struct mm_set_rf_calib_cfm *cfm);


