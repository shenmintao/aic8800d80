/**
 ******************************************************************************
 *
 * @file rwnx_main.h
 *
 * Copyright (C) RivieraWaves 2012-2019
 *
 ******************************************************************************
 */

#ifndef _RWNX_MAIN_H_
#define _RWNX_MAIN_H_

#include "rwnx_defs.h"

int rwnx_cfg80211_init(struct rwnx_plat *rwnx_plat, void **platform_data);
void rwnx_cfg80211_deinit(struct rwnx_hw *rwnx_hw);
extern int testmode;
extern u8 chip_id;
extern u8 chip_sub_id;
extern u8 chip_mcu_id;

#define CHIP_ID_H_MASK  0xC0
#define IS_CHIP_ID_H()  ((chip_id & CHIP_ID_H_MASK) == CHIP_ID_H_MASK)

#define RSSI_GET_INTERVAL                (10 * 1000)   //time interval
#define RSSI_THD_0                       (-20)         //rssi 0 (dBm)
#define RSSI_THD_1                       (-30)         //rssi 1 (dBm)
#define RSSI_THD_2                       (-75)         //rssi 2 (dBm)

#define PWR_LOSS_LVL0                     (-10)        //RSSI > RSSI_THD_0
#define PWR_LOSS_LVL1                     (-5 )        //RSSI_THD_1 < RSSI <RSSI_THD_0
#define PWR_LOSS_LVL2                     (0)          //RSSI_THD_2 < RSSI <RSSI_THD_1
#define PWR_LOSS_LVL3                     (0)//(2)          //RSSI <RSSI_THD_2

#define PWR_DELAY_TIME                   (10 * 1000)   //pwr reduced latency time (ms)

#ifdef CONFIG_DYNAMIC_PERPWR
void rssi_update_txpwrloss(struct rwnx_sta *sta, s8_l rssi);
void aicwf_txpwer_per_sta_worker(struct work_struct *work);
#endif

#ifdef CONFIG_DYNAMIC_PWR
void set_txpwrloss_ctrl(struct rwnx_hw *rwnx_hw, s8 value);
void aicwf_pwrloss_worker(struct work_struct *work);
#endif

#endif /* _RWNX_MAIN_H_ */
