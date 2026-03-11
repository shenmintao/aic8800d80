#include <linux/types.h>

void system_config_8800dln(struct rwnx_hw *rwnx_hw);

void aicwf_patch_config_8800dln(struct rwnx_hw *rwnx_hw);

int aicwf_plat_rftest_load_8800dln(struct rwnx_hw *rwnx_hw);

int aicwf_plat_gain_table_load_8800dln(struct rwnx_hw *rwnx_hw);

int rwnx_plat_userconfig_load_8800dln(struct rwnx_hw *rwnx_hw);
#ifdef CONFIG_POWER_LIMIT
int rwnx_plat_powerlimit_load_8800dln(struct rwnx_hw *rwnx_hw);
#endif
int aicwf_set_rf_config_8800dln(struct rwnx_hw *rwnx_hw, struct mm_set_rf_calib_cfm *cfm);


