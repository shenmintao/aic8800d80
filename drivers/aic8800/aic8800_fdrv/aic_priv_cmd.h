/**
 ******************************************************************************
 *
 * @file private_cmd.h
 *
 * Copyright (C) Aicsemi 2018-2024
 *
 ******************************************************************************
 */

#ifndef _AIC_PRIV_CMD_H_
#define _AIC_PRIV_CMD_H_

#include "rwnx_defs.h"

typedef struct _android_wifi_priv_cmd {
    char *buf;
    int used_len;
    int total_len;
} android_wifi_priv_cmd;

struct aicwf_cs_info {
    u8_l phymode;
    u8_l bandwidth;
    u16_l freq;

    s8_l rssi;
    s8_l snr;
    s8_l noise;
    u8_l txpwr;

    //chanutil
    u16_l chan_time_ms;
    u16_l chan_time_busy_ms;

    char countrycode[4];
    u8_l rxnss;
    u8_l rxmcs;
    u8_l txnss;
    u8_l txmcs;

    u32_l tx_phyrate;
    u32_l rx_phyrate;

    u32_l tx_ack_succ_stat;
    u32_l tx_ack_fail_stat;

    u16_l chan_tx_busy_time;
};

#ifdef CONFIG_COMPAT
typedef struct _compat_android_wifi_priv_cmd {
    compat_caddr_t buf;
    int used_len;
    int total_len;
} compat_android_wifi_priv_cmd;
#endif /* CONFIG_COMPAT */

int android_priv_cmd(struct net_device *net, struct ifreq *ifr, int cmd);
int get_cs_info(struct rwnx_vif *vif, u8 *mac_addr, u8 *val);

unsigned int command_strtoul(const char *cp, char **endp, unsigned int base);
int str_starts(const char *str, const char *start);
int handle_private_cmd(struct net_device *net, char *command, u32 cmd_len);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 13, 0))
void set_mon_chan(struct rwnx_vif *vif, struct net_device *dev, char *parameter);
#else
void set_mon_chan(struct rwnx_vif *vif, char *parameter);
#endif

#endif /* _AIC_PRIV_CMD_H_ */

