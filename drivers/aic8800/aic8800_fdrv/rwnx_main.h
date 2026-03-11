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
int rwnx_fill_station_info(struct rwnx_sta *sta, struct rwnx_vif *vif,
								  struct station_info *sinfo, u8 *phymode, u32 *tx_phyrate, u32 *rx_phyrate);

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
#define PWR_FAST_SWITCH_PROTECT_TIME     (2  * 1000)   //quickly switch protection time (ms)
#define RSSI_HYSTERESIS_OFFSET           2             //buffer zone (dB)
#define RSSI_HYSTERESIS_THRESHOLD        2             //range of signal variation (dB)

#ifdef CONFIG_TEMP_CONTROL
#define TEMP_GET_INTERVAL                (10 * 1000)    //time interval
#define TEMP_THD_0                       (110)           //℃
#define TEMP_THD_1                       (95)          //℃
#define TEMP_THD_2                       (85)          //℃

#define TC_LOSS_LVL0                     (-10)        //TEMP >= TEMP_THD_0
#define TC_LOSS_LVL1                     (-5)         //TEMP_THD_1 < TEMP <= TEMP_THD_0
#define TC_LOSS_LVL2                     (-2)         //TEMP_THD_2 < TEMP <= TEMP_THD_1
#define TC_LOSS_LVL3                     (0)          //TEMP <= TEMP_THD_2
#endif

struct rwnx_sta *rwnx_retrieve_sta(struct rwnx_hw *rwnx_hw,
                                          struct rwnx_vif *rwnx_vif, u8 *addr,
                                          __le16 fc, bool ap);


#ifdef CONFIG_BAND_STEERING
void aicwf_steering_work(struct work_struct *work);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
void aicwf_steering_timeout(ulong data);
#else
void aicwf_steering_timeout(struct timer_list *t);
#endif
#endif

#ifdef CONFIG_DYNAMIC_PERPWR
void rssi_update_txpwrloss(struct rwnx_sta *sta, s8_l rssi, struct rwnx_vif *vif);
void aicwf_txpwer_per_sta_worker(struct work_struct *work);
#endif


void set_txpwrloss_ctrl(struct rwnx_hw *rwnx_hw, s8 value);
#ifdef CONFIG_DYNAMIC_PWR
void aicwf_pwrloss_worker(struct work_struct *work);
#endif
#ifdef CONFIG_TEMP_CONTROL
void aicwf_tcloss_worker(struct work_struct *work);
#endif
void rwnx_skb_align_8bytes(struct sk_buff *skb);
void rwnx_frame_parser(char* tag, char* data, unsigned long len);
void rwnx_update_mesh_power_mode(struct rwnx_vif *vif);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
void aicwf_p2p_alive_timeout(ulong data);
#else
void aicwf_p2p_alive_timeout(struct timer_list *t);
#endif
int rwnx_send_check_p2p(struct cfg80211_scan_request *param);
void apm_staloss_work_process(struct work_struct *work);
void apm_probe_sta_work_process(struct work_struct *work);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0))
int rwnx_cfg80211_set_monitor_channel_(struct wiphy *wiphy,
                                             struct net_device *dev,
                                             struct cfg80211_chan_def *chandef);
#else
int rwnx_cfg80211_set_monitor_channel_(struct wiphy *wiphy,
                                             struct cfg80211_chan_def *chandef);
#endif
int rwnx_cfg80211_probe_client(struct wiphy *wiphy, struct net_device *dev,
            const u8 *peer, u64 *cookie);
void rwnx_cfg80211_mgmt_frame_register(struct wiphy *wiphy,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0))
                   struct net_device *dev,
#else
                   struct wireless_dev *wdev,
#endif
                   u16 frame_type, bool reg);

int rwnx_cfg80211_channel_switch(struct wiphy *wiphy,
                                 struct net_device *dev,
                                 struct cfg80211_csa_settings *params);
int rwnx_cfg80211_change_bss(struct wiphy *wiphy, struct net_device *dev,
                             struct bss_parameters *params);
int rwnx_ic_system_init(struct rwnx_hw *rwnx_hw);
int rwnx_ic_rf_init(struct rwnx_hw *rwnx_hw);
void aic_ipc_setting(struct rwnx_vif *rwnx_vif);
u16 rwnx_select_queue(struct net_device *dev, struct sk_buff *skb,
                      struct net_device *sb_dev);

#endif /* _RWNX_MAIN_H_ */
