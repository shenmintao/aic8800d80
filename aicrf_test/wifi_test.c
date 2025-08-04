#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include<stdlib.h>
#include <errno.h>
#define WIFI_DRIVER_FW_PATH "sta"
#define WIFI_DRIVER_MODULE_NAME "rwnx_fdrv"
#define WIFI_DRIVER_MODULE_PATH "/vendor/modules/rwnx_fdrv.ko"
#define MAX_DRV_CMD_SIZE 1536
#define TXRX_PARA								SIOCDEVPRIVATE+1

#define CHIP_AIC8800D       0
#define CHIP_AIC8800DCDW    1
#define CHIP_AIC8800D80     2
#define CHIP_AIC8800D80X2	3

#define CHIP_SELECT         CHIP_AIC8800D80

#if (CHIP_SELECT == CHIP_AIC8800D)
#define EFUSE_CMD_OLD_FORMAT_EN 1
#else
#define EFUSE_CMD_OLD_FORMAT_EN 0
#endif

#define RELEASE_DATE "2024_0701"

//static const char IFACE_DIR[]           = "";
//static const char DRIVER_MODULE_NAME[]  = "rwnx_fdrv";
//static const char DRIVER_MODULE_TAG[]   = WIFI_DRIVER_MODULE_NAME " ";
//static const char DRIVER_MODULE_PATH[]  = WIFI_DRIVER_MODULE_PATH;
//static const char DRIVER_MODULE_ARG[]   = "";
//static const char FIRMWARE_LOADER[]     = "";
//static const char DRIVER_PROP_NAME[]    = "wlan.driver.status";

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression) \
  (__extension__                                                              \
    ({ long int __result;                                                     \
       do __result = (long int) (expression);                                 \
       while (__result == -1L && errno == EINTR);                             \
       __result; }))
#endif

typedef struct android_wifi_priv_cmd {
	char *buf;
	int used_len;
	int total_len;
} android_wifi_priv_cmd;
typedef struct cob_result_ptr_t {
	u_int16_t dut_rcv_golden_num;
	u_int8_t golden_rcv_dut_num;
	int8_t rssi_static;
	int8_t snr_static;
	int8_t dut_rssi_static;
	u_int16_t reserved;
}cob_result_ptr_t;

struct aicwf_cs_info {
    u_int8_t phymode;
    u_int8_t bandwidth;
    u_int16_t freq;

    int8_t rssi;
    int8_t snr;
    int8_t noise;
    u_int8_t txpwr;

    //chanutil
    u_int16_t chan_time_ms;
    u_int16_t chan_time_busy_ms;

    char countrycode[4];
    u_int8_t rxnss;
    u_int8_t rxmcs;
    u_int8_t txnss;
    u_int8_t txmcs;

    u_int32_t tx_phyrate;
    u_int32_t rx_phyrate;

    u_int32_t tx_ack_succ_stat;
    u_int32_t tx_ack_fail_stat;

    u_int16_t chan_tx_time_busy_ms;
};

int wifi_send_cmd_to_net_interface(const char* if_name, int argC, char *argV[])
{
	int sock;
	struct ifreq ifr;
	int ret = 0;
	int i = 0;
	char buf[MAX_DRV_CMD_SIZE];
	struct android_wifi_priv_cmd priv_cmd;
    struct cob_result_ptr_t *cob_result_ptr;
	char is_param_err = 0;
	int buf_len = 0;

	sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		printf("bad sock!\n");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, if_name);

	if (ioctl(sock, SIOCGIFFLAGS, &ifr) != 0) {
		printf("%s Could not read interface %s flags: %s",__func__, if_name, strerror(errno));
		return -1;
	}

	if (!(ifr.ifr_flags & IFF_UP)) {
		printf("%s is not up!\n",if_name);
		return -1;
	}

//	printf("ifr.ifr_name = %s\n", ifr.ifr_name);
	memset(&priv_cmd, 0, sizeof(priv_cmd));
	memset(buf, 0, sizeof(buf));

	for(i=2; i<argC; i++){
		strcat(buf, argV[i]);
		strcat(buf, " ");
	}

	priv_cmd.buf = buf;
	priv_cmd.used_len = strlen(buf);
	priv_cmd.total_len = sizeof(buf);
	ifr.ifr_data = (void*)&priv_cmd;

	printf("%s:\n", argV[2]);
	if (strcasecmp(argV[2], "SET_TX") == 0) {
		if (argC < 8) {
			is_param_err = 1;
		}
	} else if (strcasecmp(argV[2], "country_set") == 0) {
		if (argC < 3)
			is_param_err = 1;
	} else if (strcasecmp(argV[2], "SET_TXTONE") == 0) {
		if (((argC == 4) && (argV[3][0] != '0'))
			|| ((argC == 5) && (argV[3][0] == '0'))) {
			is_param_err = 1;
		}
	} else if ((strcasecmp(argV[2], "SET_RX") == 0)
			|| (strcasecmp(argV[2], "SET_COB_CAL") == 0)) {
		if (argC < 5) {
			is_param_err = 1;
		}
	} else if ((strcasecmp(argV[2], "SET_XTAL_CAP") == 0)
			|| (strcasecmp(argV[2], "SET_XTAL_CAP_FINE") == 0)) {
		if (argC < 4) {
			is_param_err = 1;
		}
	} else if ((strcasecmp(argV[2], "GET_EFUSE_BLOCK") == 0)
			|| (strcasecmp(argV[2], "SET_FREQ_CAL") == 0)
			|| (strcasecmp(argV[2], "SET_FREQ_CAL_FINE") == 0)) {
		if (argC < 4) {
			is_param_err = 1;
		}
	} else if ((strcasecmp(argV[2], "SET_MAC_ADDR") == 0)
			|| (strcasecmp(argV[2], "SET_BT_MAC_ADDR") == 0)) {
		if (argC < 8) {
			is_param_err = 1;
		}
	} else if (strcasecmp(argV[2], "SET_VENDOR_INFO") == 0) {
		if (argC < 4) {
			is_param_err = 1;
		}
	} else if (strcasecmp(argV[2], "GET_VENDOR_INFO") == 0) {
		if (argC < 3) {
			is_param_err = 1;
		}
	} else if ((strcasecmp(argV[2], "RDWR_PWRIDX") == 0)
			|| (strcasecmp(argV[2], "RDWR_PWRLVL") == 0)
			|| (strcasecmp(argV[2], "RDWR_PWROFST") == 0)
			|| (strcasecmp(argV[2], "RDWR_PWROFSTFINE") == 0)
			|| (strcasecmp(argV[2], "RDWR_EFUSE_PWROFST") == 0)
			|| (strcasecmp(argV[2], "RDWR_EFUSE_PWROFSTFINE") == 0)) {
		if (((argC == 4) && (argV[3][0] != '0'))
			|| (argC == 5)
			|| ((argC == 6) && (argV[3][0] == '0'))) {
			is_param_err = 1;
		}
	} else if ((strcasecmp(argV[2], "RDWR_DRVIBIT") == 0)
			|| (strcasecmp(argV[2], "RDWR_EFUSE_DRVIBIT") == 0)
			|| (strcasecmp(argV[2], "RDWR_EFUSE_SDIOCFG") == 0)
			|| (strcasecmp(argV[2], "RDWR_EFUSE_USBVIDPID") == 0)) {
		if (((argC == 4) && (argV[3][0] != '0'))
			|| ((argC == 5) && (argV[3][0] == '0'))) {
			is_param_err = 1;
		}
	} else if ((strcasecmp(argV[2], "SET_PAPR") == 0)) {
		if (argC < 4) {
			is_param_err = 1;
		}
	} else if ((strcasecmp(argV[2], "SET_NOTCH") == 0)
			|| (strcasecmp(argV[2], "SET_SRRC") == 0)
			|| (strcasecmp(argV[2], "SET_FSS") == 0)) {
		if (argC < 4) {
			is_param_err = 1;
		}
	} else if ((strcasecmp(argV[2], "BT_RESET") == 0)) {
		char bt_reset_hci_cmd[32] = "01 03 0c 00";
		if (argC == 3) {
			buf_len = priv_cmd.used_len;
			memcpy(&priv_cmd.buf[buf_len], &bt_reset_hci_cmd[0], strlen(bt_reset_hci_cmd));
			buf_len += strlen(bt_reset_hci_cmd);
			priv_cmd.used_len = buf_len;
		} else {
			is_param_err = 1;
		}
	} else if ((strcasecmp(argV[2], "BT_TXDH") == 0)) {
		char bt_txdh_hci_cmd[255] = "01 06 18 0e ";
		if (argC == 17) {
			buf_len = priv_cmd.used_len;
			int arg_len = strlen(argV[2]);
			int txdh_cmd_len = strlen(bt_txdh_hci_cmd);
			memcpy(&bt_txdh_hci_cmd[txdh_cmd_len], &priv_cmd.buf[arg_len+1], buf_len - arg_len - 1);
			memcpy(&priv_cmd.buf[arg_len+1], &bt_txdh_hci_cmd[0], strlen(bt_txdh_hci_cmd));
			buf_len += strlen(bt_txdh_hci_cmd);
			priv_cmd.used_len = buf_len;
		} else {
			is_param_err = 1;
		}
	} else if ((strcasecmp(argV[2], "BT_RXDH") == 0)) {
		if (argC == 16) {
			char bt_rxdh_hci_cmd[255] = "01 0b 18 0d ";
			buf_len = priv_cmd.used_len;
			int arg_len = strlen(argV[2]);
			int rxdh_cmd_len = strlen(bt_rxdh_hci_cmd);
			memcpy(&bt_rxdh_hci_cmd[rxdh_cmd_len], &priv_cmd.buf[arg_len+1], buf_len - arg_len - 1);
			memcpy(&priv_cmd.buf[arg_len+1], &bt_rxdh_hci_cmd[0], strlen(bt_rxdh_hci_cmd));
			buf_len += strlen(bt_rxdh_hci_cmd);
			priv_cmd.used_len = buf_len;
		} else {
			is_param_err = 1;
		}
	}  else if ((strcasecmp(argV[2], "BT_STOP") == 0)) {
		char bt_stop_hci_cmd[255] = "01 0C 18 01 ";
		if (argC == 4) {
			buf_len = priv_cmd.used_len;
			int arg_len = strlen(argV[2]);
			int stop_cmd_len = strlen(bt_stop_hci_cmd);
			memcpy(&bt_stop_hci_cmd[stop_cmd_len], &priv_cmd.buf[arg_len+1], buf_len - arg_len - 1);
			memcpy(&priv_cmd.buf[arg_len+1], &bt_stop_hci_cmd[0], strlen(bt_stop_hci_cmd));
			buf_len += strlen(bt_stop_hci_cmd);
			priv_cmd.used_len = buf_len;
		} else {
			is_param_err = 1;
		}
	}  else if ((strcasecmp(argV[2], "BT_DATA") == 0)) {
			//char bt_raw_data_cmd[255];
			int arg_len = strlen(argV[2]);
			buf_len = priv_cmd.used_len;
			memcpy(&priv_cmd.buf[arg_len+1], &priv_cmd.buf[arg_len+1], buf_len - arg_len - 1);
			priv_cmd.used_len = buf_len - arg_len - 1;
	} else {
		is_param_err = 0;
	}

	if (is_param_err) {
		printf("param error!!!\n");
		return 0;
	}

	if ((ret = ioctl(sock, TXRX_PARA, &ifr)) < 0) {
		printf("cmd or param error\n");
		printf("%s: error ioctl[TX_PARA] ret= %d\n", __func__, ret);
		return ret;
	}

	memcpy(&priv_cmd, ifr.ifr_data, sizeof(struct android_wifi_priv_cmd));
	if (strcasecmp(argV[2], "SET_FREQ_CAL") == 0) {
		#if (EFUSE_CMD_OLD_FORMAT_EN)
		printf("done: freq_cal: 0x%8x\n", *(unsigned int *)priv_cmd.buf);
		#else
		signed char rem_cnt = (signed char)priv_cmd.buf[1];
		if (rem_cnt < 0) {
			printf("failed to set freq_cal, no room!\n");
		} else {
			printf("done: freq_cal: 0x%2x (remain:%x)\n", (unsigned char)priv_cmd.buf[0], rem_cnt);
		}
		#endif
	} else if (strcasecmp(argV[2], "SET_FREQ_CAL_FINE") == 0) {
		#if (EFUSE_CMD_OLD_FORMAT_EN)
		printf("done: freq_cal_fine: 0x%8x\n", *(unsigned int *)priv_cmd.buf);
		#else
		signed char rem_cnt = (signed char)priv_cmd.buf[1];
		if (rem_cnt < 0) {
			printf("failed to set freq_cal_fine, no room!\n");
		} else {
			printf("done: freq_cal_fine: 0x%2x (remain:%x)\n", (unsigned char)priv_cmd.buf[0], rem_cnt);
		}
		#endif
	} else if (strcasecmp(argV[2], "GET_EFUSE_BLOCK") == 0)
		printf("done:efuse: 0x%8x\n", *(unsigned int *)priv_cmd.buf);
	else if (strcasecmp(argV[2], "SET_XTAL_CAP") == 0)
		printf("done:xtal cap: 0x%x\n", *(unsigned int *)priv_cmd.buf);
	else if (strcasecmp(argV[2], "SET_XTAL_CAP_FINE") == 0)
		printf("done:xtal cap fine: 0x%x\n", *(unsigned int *)priv_cmd.buf);
	else if (strcasecmp(argV[2], "GET_RX_RESULT") == 0)
		printf("done: getrx fcsok=%d, total=%d\n", *(unsigned int *)priv_cmd.buf, *(unsigned int *)&priv_cmd.buf[4]);
	else if (strcasecmp(argV[2], "GET_MAC_ADDR") == 0) {
		printf("done: get macaddr = %02x : %02x : %02x : %02x : %02x : %02x\n",
			*(unsigned char *)&priv_cmd.buf[5], *(unsigned char *)&priv_cmd.buf[4], *(unsigned char *)&priv_cmd.buf[3],
			*(unsigned char *)&priv_cmd.buf[2], *(unsigned char *)&priv_cmd.buf[1], *(unsigned char *)&priv_cmd.buf[0]);
		#if (!EFUSE_CMD_OLD_FORMAT_EN)
		printf("  (remain:%x)\n", priv_cmd.buf[6]);
		#endif
	} else if (strcasecmp(argV[2], "GET_BT_MAC_ADDR") == 0) {
		printf("done: get bt macaddr = %02x : %02x : %02x : %02x : %02x : %02x\n",
			*(unsigned char *)&priv_cmd.buf[5], *(unsigned char *)&priv_cmd.buf[4], *(unsigned char *)&priv_cmd.buf[3],
			*(unsigned char *)&priv_cmd.buf[2], *(unsigned char *)&priv_cmd.buf[1], *(unsigned char *)&priv_cmd.buf[0]);
		#if (!EFUSE_CMD_OLD_FORMAT_EN)
		printf("  (remain:%x)\n", priv_cmd.buf[6]);
		#endif
	} else if (strcasecmp(argV[2], "GET_FREQ_CAL") == 0) {
		unsigned int val = *(unsigned int *)&priv_cmd.buf[0];
		#if (EFUSE_CMD_OLD_FORMAT_EN)
		printf("done: get_freq_cal: xtal_cap=0x%x, xtal_cap_fine=0x%x\n", val & 0x000000ff, (val >> 8) & 0x000000ff);
		#elif (CHIP_SELECT == CHIP_AIC8800DCDW)
		printf("done: get_freq_cal: xtal_cap=0x%x (remain:%x), xtal_cap_fine=0x%x (remain:%x)\n",
			val & 0xff, (val >> 8) & 0xff, (val >> 16) & 0xff, (val >> 24) & 0xff);
		#else
                printf("done: get_freq_cal: xtal_cap=0x%x (remain:%x), xtal_cap_fine=0x%x (remain:%x)\n",
                        val & 0xff, (val >> 16) & 0xff, (val >> 8) & 0xff, (val >> 24) & 0xff);
		#endif
	} else if (strcasecmp(argV[2], "GET_VENDOR_INFO") == 0) {
		#if (EFUSE_CMD_OLD_FORMAT_EN)
		printf("done: get_vendor_info = 0x%x\n", *(unsigned char *)&priv_cmd.buf[0]);
		#else
		printf("done: get_vendor_info = 0x%x (remain:%x)\n", *(unsigned char *)&priv_cmd.buf[0], priv_cmd.buf[1]);
		#endif
	} else if (strcasecmp(argV[2], "RDWR_PWRMM") == 0) {
		printf("done: txpwr manual mode = %x\n", *(unsigned int *)&priv_cmd.buf[0]);
	} else if (strcasecmp(argV[2], "RDWR_PWRIDX") == 0) {
		char *buff = &priv_cmd.buf[0];
		printf("done:\n"
			"txpwr index 2.4g:\n"
			"  [0]=%d(ofdmlowrate)\n"
			"  [1]=%d(ofdm64qam)\n"
			"  [2]=%d(ofdm256qam)\n"
			"  [3]=%d(ofdm1024qam)\n"
			"  [4]=%d(dsss)\n", buff[0], buff[1], buff[2], buff[3], buff[4]);
		printf("txpwr index 5g:\n"
			"  [0]=%d(ofdmlowrate)\n"
			"  [1]=%d(ofdm64qam)\n"
			"  [2]=%d(ofdm256qam)\n"
			"  [3]=%d(ofdm1024qam)\n", buff[5], buff[6], buff[7], buff[8]);
	} else if (strcasecmp(argV[2], "RDWR_PWRLVL") == 0) {
		char *buff = &priv_cmd.buf[0];
		int grp = 0;
		int idx = 0;
		int cnt = 0;
		int tmp_idx = 0;
		printf("done:\n"
			"txpwr index 2.4g: [0]:11b+11a/g, [1]:11n/11ac, [2]:11ax\n");
		for (grp = 0; grp < 3; grp++) {
			int cnt = 12;
			if (grp == 1) {
				cnt = 10;
			}
			printf("  [%x] =", grp);
			for (idx = 0; idx < cnt; idx++) {
				if (idx && !(idx & 0x3)) {
					printf(" ");
				}
			printf(" %2d", buff[12 * grp + idx]);
			}
			printf("\r\n");
		}
	printf("txpwr index 5g: [0]:11a, [1]:11n/11ac, [2]:11ax\n");
	for (grp = 0; grp < 3; grp++) {
		cnt = 12;
		idx = 0;
		if (grp == 0) {
		    tmp_idx = 4;
		}
		if (grp == 1) {
		    cnt = 10;
                    tmp_idx = 0;
		}
		printf("  [%x] =", grp);
		for (idx = tmp_idx ; idx < cnt; idx++) {
		    if (idx & !(idx & 0x3)) {
			printf(" ");
		    }
		    printf(" %2d", buff[12 * (grp + 3) + idx]);
		}
		printf("\r\n");
	}

	} else if (strcasecmp(argV[2], "RDWR_PWROFST") == 0) {
		signed char *buff = (signed char *)&priv_cmd.buf[0];
		#if (CHIP_SELECT < CHIP_AIC8800D80)
		printf("done:\n"
			"txpwr offset 2.4g: \n"
			"  [0]=%d(ch1~4)\n"
			"  [1]=%d(ch5~9)\n"
			"  [2]=%d(ch10~13)\n", (int8_t)buff[0], (int8_t)buff[1], (int8_t)buff[2]);
		printf("txpwr offset 5g:\n"
			"  [0]=%d(ch36~64)\n"
			"  [1]=%d(ch100~120)\n"
			"  [2]=%d(ch122~140)\n"
			"  [3]=%d(ch142~165)\n", (int8_t)buff[3], (int8_t)buff[4], (int8_t)buff[5], (int8_t)buff[6]);
		#elif (CHIP_SELECT == CHIP_AIC8800D80X2)
		int type, ch_grp;
		printf("done:\n"
			"pwrofst2x_2.4g(ant0/ant1): [0]:11b, [1]:ofdm_highrate\n"
			"  chan=" "\t1-4" "\t5-9" "\t10-13");
			for (type = 0; type < 2; type++) {
            printf("\n  [%d] =", type);
            for (ch_grp = 0; ch_grp < 3; ch_grp++) {
				printf("\t%d/%d", buff[type + 3 * ch_grp], buff[type + 3 * ch_grp + 3 * 3]);
            }
        }
        printf("\npwrofst2x_5g(ant0/ant1): [0]:ofdm_lowrate, [1]:ofdm_highrate\n"
            "  chan=" "\t36-50" "\t51-64" "\t98-114" "\t115-130" "\t131-146" "\t147-166");
        buff = (signed char *)&priv_cmd.buf[3 * 3 * 2];
        for (type = 0; type < 1; type++) {
            printf("\n  [%d] =", type);
            for (ch_grp = 0; ch_grp < 6; ch_grp++) {
                printf("\t%d/%d", buff[type + 3 *ch_grp], buff[type + 3 *ch_grp + 3 * 6]);
            }
        }
        printf("\n");
		#else
		int type, ch_grp;
		printf("done:\n"
			"pwrofst2x 2.4g: [0]:11b, [1]:ofdm_highrate, [2]:ofdm_lowrate\n"
			"  chan=" "\t1-4" "\t5-9" "\t10-13");
		for (type = 0; type < 3; type++) {
			printf("\n  [%d] =", type);
			for (ch_grp = 0; ch_grp < 3; ch_grp++) {
				printf("\t%d", buff[3 * type + ch_grp]);
			}
		}
		printf("\npwrofst2x 5g: [0]:ofdm_lowrate, [1]:ofdm_highrate, [2]:ofdm_midrate\n"
			"  chan=" "\t36-50" "\t51-64" "\t98-114" "\t115-130" "\t131-146" "\t147-166");
		buff = (signed char *)&priv_cmd.buf[3 * 3];
		for (type = 0; type < 3; type++) {
			printf("\n  [%d] =", type);
			for (ch_grp = 0; ch_grp < 6; ch_grp++) {
				printf("\t%d", buff[6 * type + ch_grp]);
			}
		}
		printf("\n");
		#endif
	} else if (strcasecmp(argV[2], "RDWR_PWROFSTFINE") == 0) {
		signed char *buff = (signed char *)&priv_cmd.buf[0];
		printf("done:\n"
			"txpwr offset fine 2.4g: \n"
			"  [0]=%d(ch1~4)\n"
			"  [1]=%d(ch5~9)\n"
			"  [2]=%d(ch10~13)\n", (int8_t)buff[0], (int8_t)buff[1], (int8_t)buff[2]);
		printf("txpwr offset fine 5g:\n"
			"  [0]=%d(ch36~64)\n"
			"  [1]=%d(ch100~120)\n"
			"  [2]=%d(ch122~140)\n"
			"  [3]=%d(ch142~165)\n", (int8_t)buff[3], (int8_t)buff[4], (int8_t)buff[5], (int8_t)buff[6]);
	} else if (strcasecmp(argV[2], "RDWR_DRVIBIT") == 0) {
		char *buff = &priv_cmd.buf[0];
		int idx = 0;
		printf("done: 2.4g txgain tbl pa drv_ibit:\n");
		for (idx = 0; idx < 16; idx++) {
			printf(" %x", buff[idx]);
			if (!((idx + 1) & 0x03)) {
				printf(" [%x~%x]\n", idx - 3, idx);
			}
		}
	} else if (strcasecmp(argV[2], "RDWR_EFUSE_PWROFST") == 0) {
		signed char *buff = (signed char *)&priv_cmd.buf[0];
		#if (EFUSE_CMD_OLD_FORMAT_EN)
		printf("done:\n"
			"efuse txpwr offset 2.4g:\n"
			"  [0]=%d(ch1~4)\n"
			"  [1]=%d(ch5~9)\n"
			"  [2]=%d(ch10~13)\n", (int8_t)buff[0], (int8_t)buff[1], (int8_t)buff[2]);
		printf("efuse txpwr offset 5g:\n"
			"  [0]=%d(ch36~64)\n"
			"  [1]=%d(ch100~120)\n"
			"  [2]=%d(ch122~140)\n"
			"  [3]=%d(ch142~165)\n", (int8_t)buff[3], (int8_t)buff[4], (int8_t)buff[5], (int8_t)buff[6]);
		#else
		#if (CHIP_SELECT < CHIP_AIC8800D80)
		printf("done:\n"
			"efuse txpwr offset 2.4g:\n"
			"  [0]=%d(remain:%x, ch1~4)\n"
			"  [1]=%d(remain:%x, ch5~9)\n"
			"  [2]=%d(remain:%x, ch10~13)\n",
			(int8_t)buff[0], (int8_t)buff[3],
			(int8_t)buff[1], (int8_t)buff[4],
			(int8_t)buff[2], (int8_t)buff[5]);
		if (ret > 6) { // 5g_en
			printf("efuse txpwr offset 5g:\n"
				"  [0]=%d(remain:%x, ch36~64)\n"
				"  [1]=%d(remain:%x, ch100~120)\n"
				"  [2]=%d(remain:%x, ch122~140)\n"
				"  [3]=%d(remain:%x, ch142~165)\n",
				(int8_t)buff[6], (int8_t)buff[10],
				(int8_t)buff[7], (int8_t)buff[11],
				(int8_t)buff[8], (int8_t)buff[12],
				(int8_t)buff[9], (int8_t)buff[13]);
		}
		#elif (CHIP_SELECT == CHIP_AIC8800D80X2)
        int type, ch_grp;
		int tmp_index_fls = 3*3 + 3*3 + 3*6 + 3*6;
		signed char * rem_buff_fls ;
		rem_buff_fls = (signed char *)&priv_cmd.buf[tmp_index_fls];
        printf("done:\n"
            "ef_pwrofst2x_2.4g(ant0/ant1): [0]:11b, [1]:ofdm_highrate\n"
            "  chan=" "\t1-4" "\t5-9" "\t10-13");
            for (type = 0; type < 2; type++) {
            printf("\n  [%d] =", type);
            for (ch_grp = 0; ch_grp < 3; ch_grp++) {
                printf("\t%d(r:%x)/%d(r:%x)", buff[type + 3 * ch_grp],rem_buff_fls[type + 3 * ch_grp],
					buff[type + 3 * ch_grp + 3 * 3],rem_buff_fls[type + 3 * ch_grp + 3 * 3]);
            }
        }
        printf("\nef_pwrofst2x_5g(ant0/ant1): [0]:ofdm_highrate\n"
            "  chan=" "\t36-50" "\t51-64" "\t98-114" "\t115-130" "\t131-146" "\t147-166");
        buff = (signed char *)&priv_cmd.buf[3 * 3 * 2];
        for (type = 0; type < 1; type++) {
            printf("\n  [%d] =", type);
            for (ch_grp = 0; ch_grp < 6; ch_grp++) {
                printf("\t%d(r:%x)/%d(r:%x)", buff[type + 3 *ch_grp],rem_buff_fls[type + 3 *ch_grp + 18],
					buff[type + 3 *ch_grp + 3 * 6],rem_buff_fls[type + 3 *ch_grp + 3 * 6 + 18]);
            }
        }
        printf("\n");
		#else
		int type, ch_grp;
		int tmp_index = 3*3 + 3*6;;
		signed char * rem_buff ;
		rem_buff = (signed char *)&priv_cmd.buf[tmp_index];
		printf("done:\n"
			"pwrofst2x 2.4g: [0]:11b, [1]:ofdm_highrate, [2]:ofdm_lowrate\n"
			"  chan=" "\t1-4\t" "\t5-9\t" "\t10-13\t");
		for (type = 0; type < 3; type++) {
			printf("\n	[%d] =", type);
			for (ch_grp = 0; ch_grp < 3; ch_grp++) {
				if (rem_buff[3 * type + ch_grp] < 0) {
					printf("\t%d(no room,r%d)", (signed char)priv_cmd.buf[3 * type + ch_grp], rem_buff[3 * type + ch_grp]);
				} else {
					printf("\t%d(r:%x)\t", (signed char)priv_cmd.buf[3 * type + ch_grp], rem_buff[3 * type + ch_grp]);
				}
			}
		}
		printf("\npwrofst2x 5g: [0]:ofdm_lowrate, [1]:ofdm_highrate, [2]:ofdm_midrate\n"
			"  chan=" "\t36-50\t" "\t51-64\t" "\t98-114\t" "\t115-130\t" "\t131-146\t" "\t147-166\t");
		buff = (signed char *)&priv_cmd.buf[3 * 3];
		tmp_index += 3*3;
		rem_buff = (signed char *)&priv_cmd.buf[tmp_index];
		for (type = 0; type < 3; type++) {
			printf("\n	[%d] =", type);
			for (ch_grp = 0; ch_grp < 6; ch_grp++) {
				if (rem_buff[6 * type + ch_grp] < 0) {
					printf("\t%d(no room,r%d)", buff[6 * type + ch_grp], rem_buff[6 * type + ch_grp]);
				} else {
					printf("\t%d(r:%x)\t", buff[6 * type + ch_grp], rem_buff[6 * type + ch_grp]);
				}
			}
		}
		printf("\n");
		#endif
		#endif
	} else if (strcasecmp(argV[2], "RDWR_EFUSE_PWROFSTFINE") == 0) {
		signed char *buff = (signed char *)&priv_cmd.buf[0];
		#if (EFUSE_CMD_OLD_FORMAT_EN)
		printf("done:\n"
			"efuse txpwr offset fine 2.4g:\n"
			"  [0]=%d(ch1~4)\n"
			"  [1]=%d(ch5~9)\n"
			"  [2]=%d(ch10~13)\n", (int8_t)buff[0], (int8_t)buff[1], (int8_t)buff[2]);
		printf("efuse txpwr offset fine 5g:\n"
			"  [0]=%d(ch36~64)\n"
			"  [1]=%d(ch100~120)\n"
			"  [2]=%d(ch122~140)\n"
			"  [3]=%d(ch142~165)\n", (int8_t)buff[3], (int8_t)buff[4], (int8_t)buff[5], (int8_t)buff[6]);
		#else
		printf("done:\n"
			"efuse txpwr offset fine 2.4g:\n"
			"  [0]=%d(remain:%x, ch1~4)\n"
			"  [1]=%d(remain:%x, ch5~9)\n"
			"  [2]=%d(remain:%x, ch10~13)\n",
			(int8_t)buff[0], (int8_t)buff[3],
			(int8_t)buff[1], (int8_t)buff[4],
			(int8_t)buff[2], (int8_t)buff[5]);
		if (ret > 6) { // 5g_en
			printf("efuse txpwr offset fine 5g:\n"
					"  [0]=%d(remain:%x, ch36~64)\n"
					"  [1]=%d(remain:%x, ch100~120)\n"
					"  [2]=%d(remain:%x, ch122~140)\n"
					"  [3]=%d(remain:%x, ch142~165)\n",
					(int8_t)buff[6], (int8_t)buff[10],
					(int8_t)buff[7], (int8_t)buff[11],
					(int8_t)buff[8], (int8_t)buff[12],
					(int8_t)buff[9], (int8_t)buff[13]);
		}
		#endif
	} else if (strcasecmp(argV[2], "RDWR_EFUSE_DRVIBIT") == 0) {
		#if (EFUSE_CMD_OLD_FORMAT_EN)
		printf("done: efsue 2.4g txgain tbl pa drv_ibit: %x\n", priv_cmd.buf[0]);
		#else
		int val = *(int *)&priv_cmd.buf[0];
		if (val < 0) {
			printf("failed to rd/wr efuse drv_ibit, ret=%d\n", val);
		} else {
			printf("done: efsue 2.4g txgain tbl pa drv_ibit: %x (remain: %x)\n", priv_cmd.buf[0], priv_cmd.buf[1]);
		}
		#endif
	} else if (strcasecmp(argV[2], "RDWR_EFUSE_SDIOCFG") == 0) {
		printf("done: efsue sdio cfg: %x\n", priv_cmd.buf[0]);
	} else if (strcasecmp(argV[2], "RDWR_EFUSE_USBVIDPID") == 0) {
		unsigned int val = (unsigned int)priv_cmd.buf[0] |
			(unsigned int)(priv_cmd.buf[1] & 0xff) <<  8 |
			(unsigned int)(priv_cmd.buf[2] & 0xff) << 16 |
			(unsigned int)(priv_cmd.buf[3] & 0xff) << 24;
		printf("done: efsue usb vid/pid: %x\n", val);
	} else if (strcasecmp(argV[2], "GET_CAL_XTAL_RES") == 0) {
		unsigned int val = *(unsigned int *)&priv_cmd.buf[0];
		printf("done: get_cal_xtal_res: cap=0x%x, cap_fine=0x%x\n", val & 0x000000ff, (val >> 8) & 0x000000ff);
	} else if ((strcasecmp(argV[2], "GET_COB_CAL_RES") == 0) || (strcasecmp(argV[2], "DO_COB_TEST") == 0)){
		unsigned int val = *(unsigned int *)&priv_cmd.buf[0];
		unsigned int val0 = *(unsigned int *)&priv_cmd.buf[4];
		cob_result_ptr = (cob_result_ptr_t *) (unsigned int *)&priv_cmd.buf[8];
		printf("done:\ncap= 0x%x cap_fine= 0x%x freq_ofst= %d Hz golden_rcv_dut= %d tx_rssi= %d dBm snr= %d dB dut_rcv_godlden= %d rx_rssi= %d dBm\n",
				val & 0x000000ff, (val >> 8) & 0x000000ff, val0, cob_result_ptr->golden_rcv_dut_num, cob_result_ptr->rssi_static, cob_result_ptr->snr_static, cob_result_ptr->dut_rcv_golden_num, cob_result_ptr->dut_rssi_static);
	} else if (strcasecmp(argV[2], "RDWR_EFUSE_USRDATA") == 0) {
		unsigned int usr_data[3];
		usr_data[0] = *(unsigned int *)&priv_cmd.buf[0];
		usr_data[1] = *(unsigned int *)&priv_cmd.buf[4];
		usr_data[2] = *(unsigned int *)&priv_cmd.buf[8];
		printf("done: efuse usrdata:\n [0]=0x%08x\n [1]=0x%08x\n [2]=0x%08x\n",
			usr_data[0], usr_data[1], usr_data[2]);
	} else if (strcasecmp(argV[2], "RDWR_EFUSE_HE_OFF") == 0) {
		printf("EFUSE_HE_OFF: %d\n", priv_cmd.buf[0]);
	} else if (strcasecmp(argV[2], "GET_BT_RX_RESULT") == 0) {
		printf("done: get bt rx total=%d, ok=%d, err=%d\n", *(unsigned int *)priv_cmd.buf,
			*(unsigned int *)&priv_cmd.buf[4],
			*(unsigned int *)&priv_cmd.buf[8]);
	} else if (strcasecmp(argV[2], "BT_DATA") == 0) {
		unsigned char *buff = (unsigned char *)&priv_cmd.buf[1];
		int  len = priv_cmd.buf[0];
		int idx = 0;
		printf("done: %d\n", len);
		for (idx = 0; idx < len; idx++) {
			printf("%02x ", buff[idx]);
		}
		printf("\n");
	} else if (strcasecmp(argV[2], "RDWR_BT_EFUSE_PWROFST") == 0) {
		signed char *buff = (signed char *)&priv_cmd.buf[0];
		printf("done: bt efsue pwrofst %d (remain: %x)\n", priv_cmd.buf[0], priv_cmd.buf[1]);
	} else if(strcasecmp(argV[2], "GET_CS_INFO") == 0) {
		struct aicwf_cs_info *cs_info = (struct aicwf_cs_info *)priv_cmd.buf;
		printf("phymode=%d(0:B 1:G 2:A 3:N 4:AC 5:AX)\n", cs_info->phymode);
		printf("bandwidth=%d(0:20 1:40 2:80)\n", cs_info->bandwidth);
		printf("freq=%d\n", cs_info->freq);
		printf("rssi=%d\n", cs_info->rssi);
		printf("snr=%d\n", cs_info->snr);
		printf("noise=%d\n", cs_info->noise);
		printf("txpwr=%d\n", cs_info->txpwr);
		//chanutil
		printf("chan rxbusy times=%d/%d(ms), txbusy times=%d/%d(ms)\n", cs_info->chan_time_busy_ms, cs_info->chan_time_ms, cs_info->chan_tx_time_busy_ms, cs_info->chan_time_ms);
		printf("coutry code =%s\n", cs_info->countrycode);
		printf("rx nss=%d, mcs=%x\n", cs_info->rxnss, cs_info->rxmcs);
		printf("tx nss=%d, mcs=%x\n", cs_info->txnss, cs_info->txmcs);
		printf("tx_phyrate=%d(Kbps)\n", cs_info->tx_phyrate);
		printf("rx_phyrate=%d(Kbps)\n", cs_info->rx_phyrate);
		printf("tx_ack_succ_stat=%d\n", cs_info->tx_ack_succ_stat);
		printf("tx_ack_fail_stat=%d\n", cs_info->tx_ack_fail_stat);
	} else if(strcasecmp(argV[2], "GET_NOISE") == 0) {
		printf("noise: %d,%d\n", priv_cmd.buf[0], priv_cmd.buf[1]);
        } else {
		printf("done\n");
	}

	return ret;
}

int main(int argC, char *argV[])
{

	//char* ins = "insmod";
	//char* rm = "rmmod";
	//char* ko = "rwnx_fdrv.ko";

	//printf("enter!!!AIC    argC=%d    argV[0]=%s    argV[1]=%s    argV[2]=%s\n", argC, argV[0], argV[1],argV[2]);
	if (argC == 1) {
		printf("Please enter parameters!\n");
		return -1;
	}

	if (argC >= 3)
		wifi_send_cmd_to_net_interface(argV[1], argC, argV);
	else if ((strcasecmp(argV[1], "-v") == 0) || (strcasecmp(argV[1], "version") == 0))
		printf("wifi_test %s_%d\n", RELEASE_DATE, CHIP_SELECT);
	else
		printf("Bad parameter! %d\n",argC);


	return 0;
}
