/**
 ******************************************************************************
 *
 * Copyright (C) 2020 AIC semiconductor.
 *
 * @file aicwf_manager.c
 *
 * @brief netlink msg definitions
 *
 ******************************************************************************
 */

#include <linux/module.h>
#include <linux/netlink.h>
#include <net/sock.h>
#include "aicwf_manager.h"
#include "lmac_mac.h"
#include "aicwf_debug.h"

#define MAC_FMT "%02x:%02x:%02x:%02x:%02x:%02x"
#define MAC_ARG(x) ((u8_l *)(x))[0], ((u8_l *)(x))[1], ((u8_l *)(x))[2], ((u8_l *)(x))[3], ((u8_l *)(x))[4], ((u8_l *)(x))[5]
#define MANAGER_STR "[MANAGER] USB "


static struct sock *nl_sock = NULL;

static struct rwnx_vif *nl_rwnx_vif[PHY_BAND_MAX][CONFIG_IFACE_NUMBER] = {{NULL}};
static u8_l nl_hook[PHY_BAND_MAX][CONFIG_IFACE_NUMBER] = {{0}};

static u8_l nl_daemon_on = 0;

#define FREQ_2G_MIN 2412
#define FREQ_2G_MAX 2484
#define FREQ_5G_MIN 5170
#define FREQ_5G_MAX 5825
#define FREQ_6G_MIN 5925
#define FREQ_6G_MAX 7125

static int freq_to_channel(int freq)
{
	if (freq >= FREQ_2G_MIN && freq <= FREQ_2G_MAX) {
		if (freq == 2484) {
			return 14;
		} else {
			return (freq - 2412) / 5 + 1;
		}
	} else if (freq >= FREQ_5G_MIN && freq <= FREQ_5G_MAX) {
		return (freq - 5000) / 5;
	} else if (freq >= FREQ_6G_MIN && freq <= FREQ_6G_MAX) {
		return (freq - 5950) / 5;
	} else {
		AICWFDBG(LOGERROR, MANAGER_STR"Unsupported frequency: %d MHz\n", freq);
		return -1;
	}
}


void aicwf_nl_recv_msg(struct sk_buff *skb)
{
	struct rwnx_vif *rwnx_vif;
	struct rwnx_sta *sta = NULL;
	u8_l rwnx_hook = 0;
	u8_l band;
	u8_l ssid;
	struct nlmsghdr *nlh = NULL;
	struct b_nl_message *msg = NULL;
	struct b_elm_header *hdr = NULL;
	struct b_elm_intf *intf = NULL;
	struct b_elm_sta_info *sta_info = NULL;
	struct b_elm_roam_info *roam_info = NULL;
	u32 offset = 0;

	if (skb == NULL) {
		AICWFDBG(LOGERROR, MANAGER_STR"skb is null.\n");
		return;
	}

	nlh = (struct nlmsghdr *)skb->data;
	msg = (struct b_nl_message *)NLMSG_DATA(nlh);

	AICWFDBG(LOGSTEER, MANAGER_STR"%s, %d\n", __func__, msg->type);

	/* message type */
	switch(msg->type) {
	case AIC_NL_DAEMON_ON_TYPE:
		nl_daemon_on = 1;
		AICWFDBG(LOGSTEER, MANAGER_STR"AIC_NL_DAEMON_ON_TYPE\n");
		for (band = 0; band < PHY_BAND_MAX; band++) {
			for (ssid = 0; ssid < CONFIG_IFACE_NUMBER; ssid++) {
				rwnx_vif = nl_rwnx_vif[band][ssid];
				rwnx_hook = nl_hook[band][ssid];

				if (!rwnx_hook) {
					AICWFDBG(LOGSTEER, MANAGER_STR"%s, search for the next rwnx_hook, %d,%d\n", __func__, band, ssid);
					continue;
				}
				if (rwnx_vif == NULL || rwnx_vif->up == false) {
					AICWFDBG(LOGSTEER, MANAGER_STR"%s, !rwnx_vif, %d,%d\n", __func__, band, ssid);
					continue;
				}

#ifdef CONFIG_BAND_STEERING
				aicwf_band_steering_init(rwnx_vif);
#endif

				spin_lock_bh(&rwnx_vif->rwnx_hw->cb_lock);
				list_for_each_entry(sta, &rwnx_vif->ap.sta_list, list){
					if (sta->valid)
						aicwf_nl_send_new_sta_msg(rwnx_vif, sta->mac_addr);
				}
				spin_unlock_bh(&rwnx_vif->rwnx_hw->cb_lock);
			}
		}
		break;
	case AIC_NL_DAEMON_OFF_TYPE:
		nl_daemon_on = 0;
		break;
#ifdef CONFIG_BAND_STEERING
	case AIC_NL_B_STEER_BLOCK_ADD_TYPE:
	case AIC_NL_B_STEER_BLOCK_DEL_TYPE:
		while (offset < msg->len) {
			hdr = (struct b_elm_header *)(msg->content + offset);
			offset += ELM_HEADER_LEN;

			switch(hdr->id) {
			case AIC_ELM_INTF_ID:
				intf = (struct b_elm_intf *)(msg->content + offset);
				break;
			case AIC_ELM_STA_INFO_ID:
				sta_info = (struct b_elm_sta_info *)(msg->content + offset);
				break;
			default:
				AICWFDBG(LOGERROR, MANAGER_STR"unknown element id.\n");
				break;
			}
			offset += hdr->len;
		}

		if (intf && sta_info) {
			band = intf->band;
			ssid = intf->ssid;
			AICWFDBG(LOGSTEER, MANAGER_STR"STEER_BLOCK band: %d, ssid: %d\n", band, ssid);
			rwnx_vif = nl_rwnx_vif[band][ssid];
			rwnx_hook = nl_hook[band][ssid];

			if (!rwnx_hook) {
				//AICWFDBG(LOGSTEER, MANAGER_STR"Not this driver's msg p1.\n");
				break;
			}
			if (rwnx_vif == NULL || rwnx_vif->up == false) {
				AICWFDBG(LOGSTEER, MANAGER_STR"rwnx_vif is null/down p1.\n");
				break;
			}

			if (msg->type == AIC_NL_B_STEER_BLOCK_ADD_TYPE)
				aicwf_band_steering_block_entry_add(rwnx_vif, sta_info->mac);
			else if (msg->type == AIC_NL_B_STEER_BLOCK_DEL_TYPE)
				aicwf_band_steering_block_entry_del(rwnx_vif, sta_info->mac);
		}
		break;
	case AIC_NL_B_STEER_ROAM_TYPE:
		while (offset < msg->len) {
			hdr = (struct b_elm_header *)(msg->content + offset);
			offset += ELM_HEADER_LEN;

			/* element type: should handle wrong length */
			switch(hdr->id) {
			case AIC_ELM_INTF_ID:
				intf = (struct b_elm_intf *)(msg->content + offset);
				break;
			case AIC_ELM_ROAM_INFO_ID:
				roam_info = (struct b_elm_roam_info *)(msg->content + offset);
				break;
			default:
				AICWFDBG(LOGERROR, MANAGER_STR"unknown element id.\n");
				break;
			}
			offset += hdr->len;
		}

		if (intf && roam_info) {
			band = intf->band;
			ssid = intf->ssid;
			rwnx_vif = nl_rwnx_vif[band][ssid];
			rwnx_hook = nl_hook[band][ssid];

			if (!rwnx_hook) {
				//AICWFDBG(LOGERROR, MANAGER_STR"Not this driver's msg p2.\n");
				break;
			}
			if (rwnx_vif == NULL || rwnx_vif->up == false) {
				AICWFDBG(LOGERROR, MANAGER_STR"rwnx_vif is null/down p2.\n");
				break;
			}

			AICWFDBG(LOGSTEER, MANAGER_STR"AIC_NL_B_STEER_ROAM_TYPE (hostapd_cli)!\n");
			AICWFDBG(LOGSTEER, MANAGER_STR"sta_mac="MAC_FMT"\n", MAC_ARG(roam_info->sta_mac));
			AICWFDBG(LOGSTEER, MANAGER_STR"bss_mac="MAC_FMT" bss_ch=%u method=%s\n",
				MAC_ARG(roam_info->bss_mac),
				roam_info->bss_ch,
				roam_info->method == 0 ? "11V" : "Deauth");

			if (roam_info->method == 1)
				aicwf_band_steering_roam_block_entry_add(rwnx_vif, roam_info->sta_mac);
		}
		break;
#endif
	default:
		AICWFDBG(LOGERROR, MANAGER_STR"unknown message type.\n");
		break;
	}

	return;
}

void aicwf_nl_send_msg(void *msg, u32 msg_len)
{
	struct nlmsghdr *nlh = NULL;
	struct sk_buff *skb = NULL;
	u32 skb_len;
	s32 err;

	skb_len = NLMSG_SPACE(NL_MAX_MSG_SIZE);
	skb = __dev_alloc_skb(skb_len, GFP_ATOMIC);
	if (!skb) {
		AICWFDBG(LOGERROR, MANAGER_STR"allocate skb failed.\n");
		goto func_return;
	}

	nlh = nlmsg_put(skb, 0, 0, 0, NL_MAX_MSG_SIZE, 0);
	if (!nlh) {
		AICWFDBG(LOGERROR, MANAGER_STR"put netlink header failed.\n");
		kfree_skb(skb);
		goto func_return;
	}

	NETLINK_CB(skb).portid = 0;
	NETLINK_CB(skb).dst_group = 0;
	memset(NLMSG_DATA(nlh), 0, msg_len);
	memcpy(NLMSG_DATA(nlh), (u8_l *)msg, msg_len);

	if (!nl_sock) {
		AICWFDBG(LOGERROR, MANAGER_STR"[%s %u] nl_sock is NULL\n", __FUNCTION__, __LINE__);
		goto msg_fail_skb;
	}

	err = netlink_unicast(nl_sock, skb, NL_AIC_MANAGER_PID, MSG_DONTWAIT);

	if (err < 0) {
		/* netlink_unicast() already kfree_skb */
		AICWFDBG(LOGSTEER, MANAGER_STR"send netlink unicast failed.\n");
		goto func_return;
	}

func_return:
	return;

msg_fail_skb:
	kfree_skb(skb);
}

static void aicwf_netlink_set_msg(
	struct b_nl_message *msg, u32 *msg_len, void *elm, u32 elm_len)
{
	memcpy(msg->content + (*msg_len), elm, elm_len);
	(*msg_len) += elm_len;

	return;
}

void aicwf_nl_send_del_sta_msg(struct rwnx_vif *rwnx_vif, u8_l *mac)
{
	u32 msg_len = 0;
	struct b_nl_message msg = {0};
	struct b_elm_header hdr = {0};
	struct b_elm_intf intf = {{0}};
	struct b_elm_sta_info sta_info = {{0}};

	if (!nl_daemon_on)
		return;

	AICWFDBG(LOGSTEER, MANAGER_STR"%s, "MAC_FMT"\n", __func__, MAC_ARG(mac));

	/* element header */
	hdr.id = AIC_ELM_INTF_ID;
	hdr.len = ELM_INTF_LEN;
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);
	
	/* element: AIC_ELM_INTF_ID */
	intf.root = 0; /* TBD */
	intf.band = rwnx_vif->ap.band;
	intf.ssid = rwnx_vif->rwnx_hw->iface_idx;
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&intf, ELM_INTF_LEN);

	/* element header */
	hdr.id = AIC_ELM_STA_INFO_ID;
	hdr.len = ELM_STA_INFO_LEN;
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);

	/* element: AIC_ELM_STA_INFO_ID */
	memcpy(sta_info.mac, mac, 6);
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&sta_info, ELM_STA_INFO_LEN);

	/* finish message */
	msg.type = AIC_NL_DEL_STA_TYPE;
	msg.len = msg_len;

	/* length += (type + len) */
	msg_len += 8;
	aicwf_nl_send_msg((void *)&msg, msg_len);

	return;
}

void aicwf_nl_send_new_sta_msg(struct rwnx_vif *rwnx_vif, u8_l *mac)
{
	u32 msg_len = 0;
	struct b_nl_message msg = {0};
	struct b_elm_header hdr = {0};
	struct b_elm_intf intf = {{0}};
	struct b_elm_sta_info sta_info = {{0}};

	if (!nl_daemon_on)
		return;

	AICWFDBG(LOGSTEER, MANAGER_STR"%s, "MAC_FMT"\n", __func__, MAC_ARG(mac));

	/* element header */
	hdr.id = AIC_ELM_INTF_ID;
	hdr.len = ELM_INTF_LEN;
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);
	
	/* element: AIC_ELM_INTF_ID */
	intf.root = 0; /* TBD */
	intf.band = rwnx_vif->ap.band;
	intf.ssid = rwnx_vif->rwnx_hw->iface_idx;
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&intf, ELM_INTF_LEN);

	/* element header */
	hdr.id = AIC_ELM_STA_INFO_ID;
	hdr.len = ELM_STA_INFO_LEN;
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);

	/* element: AIC_ELM_STA_INFO_ID */
	memcpy(sta_info.mac, mac, 6);
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&sta_info, ELM_STA_INFO_LEN);

	/* finish message */
	msg.type = AIC_NL_NEW_STA_TYPE;
	msg.len = msg_len;

	/* length += (type + len) */
	msg_len += 8;
	aicwf_nl_send_msg((void *)&msg, msg_len);

	return;
}

void aicwf_nl_send_intf_rpt_msg(struct rwnx_vif *rwnx_vif)
{
	u32 msg_len = 0;
	struct b_nl_message msg = {0};
	struct b_elm_header hdr = {0};
	struct b_elm_intf intf = {{0}};
	struct b_elm_intf_info intf_info = {0};

	if (!nl_daemon_on)
		return;

	/* element header */
	hdr.id = AIC_ELM_INTF_ID;
	hdr.len = ELM_INTF_LEN;
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);

	/* element: AIC_ELM_INTF_ID */
	memcpy(intf.mac, rwnx_vif->ndev->dev_addr, 6);
	intf.root = 0; /* TBD */
	intf.band = rwnx_vif->ap.band;
	intf.ssid = rwnx_vif->rwnx_hw->iface_idx;
	memcpy(intf.name, rwnx_vif->ndev->name, 16);
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&intf, ELM_INTF_LEN);

	/* element header */
	hdr.id = AIC_ELM_INTF_INFO_ID;
	hdr.len = ELM_INTF_INFO_LEN;
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);

	/* element: AIC_ELM_INTF_INFO_ID */
	intf_info.ch = freq_to_channel(rwnx_vif->ap.freq);
	intf_info.tx_tp = 0; /* TBD */
	intf_info.rx_tp = 0; /* TBD */

	intf_info.bss_info = 0;
	intf_info.reg_class = (rwnx_vif->ap.band == 0) ? 81 : 128;
	intf_info.phy_type = 7;

	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&intf_info, ELM_INTF_INFO_LEN);

	/* finish message */
	msg.type = AIC_NL_INTF_RPT_TYPE;
	msg.len = msg_len;

	/* length += (type + len) */
	msg_len += 8;
	aicwf_nl_send_msg((void *)&msg, msg_len);

	return;
}

void aicwf_nl_send_sta_rpt_msg(struct rwnx_vif *rwnx_vif, struct rwnx_sta *sta)
{
	u32 msg_len = 0;
	struct b_nl_message msg = {0};
	struct b_elm_header hdr = {0};
	struct b_elm_intf intf = {{0}};
	struct b_elm_sta_info sta_info = {{0}};
	struct b_elm_sta_info_ext sta_info_ext = {{0}};

	if (!nl_daemon_on)
		return;

	/* element header */
	hdr.id = AIC_ELM_INTF_ID;
	hdr.len = ELM_INTF_LEN;
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);

	/* element: AIC_ELM_INTF_ID */
	memcpy(intf.mac, rwnx_vif->ndev->dev_addr, 6);
	intf.root = 0; /* TBD */
	intf.band = rwnx_vif->ap.band;
	intf.ssid = rwnx_vif->rwnx_hw->iface_idx;
	memcpy(intf.name, rwnx_vif->ndev->name, 16);
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&intf, ELM_INTF_LEN);

	/* element header */
	hdr.id = AIC_ELM_STA_INFO_ID;
	hdr.len = ELM_STA_INFO_LEN;
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);

	/* element: AIC_ELM_STA_INFO_ID */
	memcpy(sta_info.mac, sta->mac_addr, 6);
	sta_info.rssi = sta->rssi;
	sta_info.link_time = sta->link_time;
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&sta_info, ELM_STA_INFO_LEN);

	/* element header */
	hdr.id = AIC_ELM_STA_INFO_EXT_ID;
	hdr.len = ELM_STA_INFO_EXT_LEN;
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);

	/* element: AIC_ELM_STA_INFO_EXT_ID */
	memcpy(sta_info_ext.mac, sta->mac_addr, 6);
	sta_info_ext.supported_band = sta->support_band;
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&sta_info_ext, ELM_STA_INFO_EXT_LEN);

	/* finish message */
	msg.type = AIC_NL_STA_RPT_TYPE;
	msg.len = msg_len;

	/* length += (type + len) */
	msg_len += 8;
	aicwf_nl_send_msg((void *)&msg, msg_len);

	return;
}

void aicwf_nl_send_frame_rpt_msg(struct rwnx_vif *rwnx_vif, u16_l frame_type, u8_l *sa, s8_l rssi)
{
	u32 msg_len = 0;
	struct b_nl_message msg = {0};
	struct b_elm_header hdr = {0};
	struct b_elm_frame_info frame_info = {0};
	struct b_elm_intf intf = {{0}};

	if (!nl_daemon_on)
		return;

	//AICWFDBG(LOGSTEER, "[NETLINK] %s, sta: "MAC_FMT"\n", __func__, MAC_ARG(sa));

	/* TBD */
	if (frame_type == WIFI_PROBEREQ && rssi <= LOW_RSSI_IGNORE) {
		AICWFDBG(LOGSTEER, MANAGER_STR"WIFI_PROBEREQ <= %d, ignore\n", LOW_RSSI_IGNORE);
		return;
	}

	/* element header */
	hdr.id = AIC_ELM_INTF_ID;
	hdr.len = ELM_INTF_LEN;
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);

	/* element: AIC_ELM_INTF_ID */
	intf.root = 0; /* TBD */
	intf.band = rwnx_vif->ap.band;
	intf.ssid = rwnx_vif->rwnx_hw->iface_idx;
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&intf, ELM_INTF_LEN);

	/* element header */
	hdr.id = AIC_ELM_FRAME_INFO_ID;
	hdr.len = ELM_FRAME_INFO_LEN;
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);
	
	/* element: AIC_ELM_FRAME_INFO_ID */
	frame_info.frame_type = frame_type;
	memcpy(frame_info.sa, sa, 6);
	frame_info.rssi = rssi;
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&frame_info, ELM_FRAME_INFO_LEN);

	/* finish message */
	msg.type = AIC_NL_FRAME_RPT_TYPE;
	msg.len = msg_len;

	/* length += (type + len) */
	msg_len += 8;
	aicwf_nl_send_msg((void *)&msg, msg_len);

	return;
}

void aicwf_nl_send_time_tick_msg(struct rwnx_vif *rwnx_vif)
{
	u32 msg_len = 0;
	struct b_nl_message msg = {0};
	struct b_elm_header hdr = {0};
	struct b_elm_intf intf = {{0}};

	if (!nl_daemon_on)
		return;

	/* element header */
	hdr.id = AIC_ELM_INTF_ID;
	hdr.len = ELM_INTF_LEN;
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&hdr, ELM_HEADER_LEN);

	/* element: AIC_ELM_INTF_ID */
	intf.root = 0; /* TBD */
	intf.band = rwnx_vif->ap.band;
	intf.ssid = rwnx_vif->rwnx_hw->iface_idx;
	aicwf_netlink_set_msg(&msg, &msg_len, (void *)&intf, ELM_INTF_LEN);

	/* finish message */
	msg.type = AIC_NL_TIME_TICK_TYPE;
	msg.len = msg_len;

	/* length += (type + len) */
	msg_len += 8;
	aicwf_nl_send_msg((void *)&msg, msg_len);

	return;
}

void aicwf_nl_hook(struct rwnx_vif *rwnx_vif, u8_l band, u8_l ssid)
{
	AICWFDBG(LOGSTEER, MANAGER_STR"%s band:%d, ssid:%d\n", __func__, band, ssid);

	nl_hook[band][ssid] = 1;
	nl_rwnx_vif[band][ssid] = rwnx_vif;

	return;
}

void aicwf_nl_hook_deinit(u8_l band, u8_l ssid)
{

	AICWFDBG(LOGSTEER, MANAGER_STR"%s band:%d, ssid:%d\n", __func__, band, ssid);

	nl_hook[band][ssid] = 0;
	nl_rwnx_vif[band][ssid] = NULL;

	return;
}


void aicwf_nl_init(void)
{
	if (nl_sock) {
		AICWFDBG(LOGSTEER, MANAGER_STR"netlink already init.\n");
		return;
	}
	struct netlink_kernel_cfg cfg = {
		.input = aicwf_nl_recv_msg,
	};

	nl_sock = netlink_kernel_create(&init_net, NL_AIC_PROTOCOL, &cfg);
	if (!nl_sock)
		AICWFDBG(LOGERROR, MANAGER_STR"create netlink falied.\n");

	return;
}

void aicwf_nl_deinit(void)
{
	if(nl_sock)
	{
		netlink_kernel_release(nl_sock);
		nl_sock = NULL;
	}
	AICWFDBG(LOGSTEER, MANAGER_STR"[aicwf_nl_deinit] delete nl_sock netlink succeed.\n");

	return;
}

