/**
 ******************************************************************************
 *
 * @file rwnx_compat.h
 *
 * Ensure driver compilation for linux 3.16 to 3.19
 *
 * To avoid too many #if LINUX_VERSION_CODE if the code, when prototype change
 * between different kernel version:
 * - For external function, define a macro whose name is the function name with
 *   _compat suffix and prototype (actually the number of parameter) of the
 *   latest version. Then latest version this macro simply call the function
 *   and for older kernel version it call the function adapting the api.
 * - For internal function (e.g. cfg80211_ops) do the same but the macro name
 *   doesn't need to have the _compat suffix when the function is not used
 *   directly by the driver
 *
 * Copyright (C) RivieraWaves 2018
 *
 ******************************************************************************
 */
#ifndef _RWNX_COMPAT_H_
#define _RWNX_COMPAT_H_
#include <linux/version.h>

/* Keep wireless API checks separate from timer, USB and other kernel APIs. */
#ifndef AICWF_CFG80211_VERSION_CODE
#define AICWF_CFG80211_VERSION_CODE LINUX_VERSION_CODE
#endif

/*
 * cfg80211_ops callbacks change signature between kernels, and a plain
 * LINUX_VERSION_CODE test is not enough: distributions and OpenWrt backport
 * individual API changes into stable series without moving the version, so a
 * numeric check picks the wrong prototype and the build dies with a
 * -Wincompatible-pointer-types error. Asking the compiler which prototype the
 * headers in front of it actually declare is immune to that.
 *
 * The helpers below read the real type of a cfg80211_ops member and let the
 * compiler decide, at compile time, which callback variant to install. They
 * are deliberately generic so every reshaped callback uses one mechanism
 * instead of growing yet another version test.
 *
 * Note these are compiler builtins, not preprocessor values: they are meant
 * for __builtin_choose_expr() and array-size checks, never for #if.
 */
#define AICWF_CFG80211_OP_FIELD(_member) \
    typeof(((struct cfg80211_ops *)0)->_member)

#define AICWF_CFG80211_OP_IS(_member, _shape) \
    __builtin_types_compatible_p(AICWF_CFG80211_OP_FIELD(_member), _shape)

/*
 * AICWF_CFG80211_OP_REQUIRE() pins the accepted shapes of a callback into a
 * compile-time assertion. The generated array is size 1 when the running
 * headers declare one of the expected forms and -1 otherwise, so a future
 * kernel that reshapes the callback fails loudly with an explicit message
 * instead of a cryptic initializer warning. Pass the accepted shapes as a
 * list of AICWF_CFG80211_OP_IS() terms joined by ||.
 */
#define AICWF_CFG80211_OP_REQUIRE(_member, _accepted_shapes) \
    typedef char aicwf_cfg80211_##_member##_shape_must_be_supported[ \
        (_accepted_shapes) ? 1 : -1]

/*
 * set_monitor_channel() grew a "struct net_device *dev" argument. The change
 * reached mainline in 6.13 and was backported to the stable series starting
 * with 6.12.101, so 6.11 and earlier 6.12.x still use the two argument form.
 * The compiler tells us which one this kernel declares.
 */
typedef int (*aicwf_set_monitor_channel_2_t)(
    struct wiphy *wiphy, struct cfg80211_chan_def *chandef);
typedef int (*aicwf_set_monitor_channel_3_t)(
    struct wiphy *wiphy, struct net_device *dev,
    struct cfg80211_chan_def *chandef);

AICWF_CFG80211_OP_REQUIRE(
    set_monitor_channel,
    AICWF_CFG80211_OP_IS(set_monitor_channel, aicwf_set_monitor_channel_2_t)
        || AICWF_CFG80211_OP_IS(set_monitor_channel, aicwf_set_monitor_channel_3_t));

#define AICWF_CFG80211_SET_MONITOR_CHANNEL_TAKES_DEV \
    AICWF_CFG80211_OP_IS(set_monitor_channel, aicwf_set_monitor_channel_3_t)

/*
 * Free cfg80211 helpers (functions, not cfg80211_ops members) change shape
 * the same way callbacks do. AICWF_CFG80211_HELPER_IS() reads the declared
 * type of the helper itself, and AICWF_CFG80211_HELPER_REQUIRE() pins its
 * accepted shapes into a compile-time assertion.
 */
#define AICWF_CFG80211_HELPER_IS(_helper, _shape) \
    __builtin_types_compatible_p(typeof(&_helper), _shape)

#define AICWF_CFG80211_HELPER_REQUIRE(_helper, _accepted_shapes) \
    typedef char aicwf_cfg80211_##_helper##_shape_must_be_supported[ \
        (_accepted_shapes) ? 1 : -1]

/*
 * The remaining cfg80211_ops callbacks below are not reshaped by the driver
 * yet, but they have changed between kernels before and some are backported
 * independently of LINUX_VERSION_CODE. These assertions do not select a
 * variant: they only turn a silent -Wincompatible-pointer-types failure on a
 * future kernel into an explicit, greppable diagnostic. Adding the missing
 * variant is then a small, well-defined change.
 */

/* set_wiphy_params() grew an int radio_idx argument for MLO. */
typedef int (*aicwf_set_wiphy_params_2_t)(struct wiphy *wiphy, u32 changed);
typedef int (*aicwf_set_wiphy_params_3_t)(
    struct wiphy *wiphy, int radio_idx, u32 changed);

AICWF_CFG80211_OP_REQUIRE(
    set_wiphy_params,
    AICWF_CFG80211_OP_IS(set_wiphy_params, aicwf_set_wiphy_params_2_t)
        || AICWF_CFG80211_OP_IS(set_wiphy_params, aicwf_set_wiphy_params_3_t));

/* start_radar_detection() gained cac_time_ms, then MLO's int link_id. */
typedef int (*aicwf_start_radar_detection_3_t)(
    struct wiphy *wiphy, struct net_device *dev,
    struct cfg80211_chan_def *chandef);
typedef int (*aicwf_start_radar_detection_4_t)(
    struct wiphy *wiphy, struct net_device *dev,
    struct cfg80211_chan_def *chandef, u32 cac_time_ms);
typedef int (*aicwf_start_radar_detection_5_t)(
    struct wiphy *wiphy, struct net_device *dev,
    struct cfg80211_chan_def *chandef, u32 cac_time_ms, int link_id);

AICWF_CFG80211_OP_REQUIRE(
    start_radar_detection,
    AICWF_CFG80211_OP_IS(start_radar_detection, aicwf_start_radar_detection_3_t)
        || AICWF_CFG80211_OP_IS(start_radar_detection, aicwf_start_radar_detection_4_t)
        || AICWF_CFG80211_OP_IS(start_radar_detection, aicwf_start_radar_detection_5_t));

/* set_tx_power()/get_tx_power() gained the same int radio_idx argument. */
typedef int (*aicwf_set_tx_power_4_t)(
    struct wiphy *wiphy, struct wireless_dev *wdev,
    enum nl80211_tx_power_setting type, int mbm);
typedef int (*aicwf_set_tx_power_5_t)(
    struct wiphy *wiphy, int radio_idx, struct wireless_dev *wdev,
    enum nl80211_tx_power_setting type, int mbm);

AICWF_CFG80211_OP_REQUIRE(
    set_tx_power,
    AICWF_CFG80211_OP_IS(set_tx_power, aicwf_set_tx_power_4_t)
        || AICWF_CFG80211_OP_IS(set_tx_power, aicwf_set_tx_power_5_t));

typedef int (*aicwf_get_tx_power_3_t)(
    struct wiphy *wiphy, struct wireless_dev *wdev, int *dbm);
typedef int (*aicwf_get_tx_power_4_t)(
    struct wiphy *wiphy, int radio_idx, struct wireless_dev *wdev, int *dbm);

AICWF_CFG80211_OP_REQUIRE(
    get_tx_power,
    AICWF_CFG80211_OP_IS(get_tx_power, aicwf_get_tx_power_3_t)
        || AICWF_CFG80211_OP_IS(get_tx_power, aicwf_get_tx_power_4_t));

/* cfg80211_cac_event() gained a trailing unsigned int link_id argument. */
typedef void (*aicwf_cfg80211_cac_event_4_t)(
    struct net_device *netdev, const struct cfg80211_chan_def *chandef,
    enum nl80211_radar_event event, gfp_t gfp);
typedef void (*aicwf_cfg80211_cac_event_5_t)(
    struct net_device *netdev, const struct cfg80211_chan_def *chandef,
    enum nl80211_radar_event event, gfp_t gfp, unsigned int link_id);

AICWF_CFG80211_HELPER_REQUIRE(
    cfg80211_cac_event,
    AICWF_CFG80211_HELPER_IS(cfg80211_cac_event, aicwf_cfg80211_cac_event_4_t)
        || AICWF_CFG80211_HELPER_IS(cfg80211_cac_event, aicwf_cfg80211_cac_event_5_t));

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
#error "Minimum kernel version supported is 3.10"
#endif

/* Generic */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
#define __bf_shf(x) (__builtin_ffsll(x) - 1)
#define FIELD_PREP(_mask, _val) \
    (((typeof(_mask))(_val) << __bf_shf(_mask)) & (_mask))
#else
#include <linux/bitfield.h>
#endif

/* CFG80211 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 20, 0)
#define IEEE80211_HE_MAC_CAP3_MAX_AMPDU_LEN_EXP_MASK IEEE80211_HE_MAC_CAP3_MAX_A_AMPDU_LEN_EXP_MASK
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(5, 15, 60)
#define IEEE80211_MAX_AMPDU_BUF IEEE80211_MAX_AMPDU_BUF_HE
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)
#define IEEE80211_RADIOTAP_HE 23
#define IEEE80211_RADIOTAP_HE_MU 24

struct ieee80211_radiotap_he {
	__le16 data1, data2, data3, data4, data5, data6;
};

enum ieee80211_radiotap_he_bits {
	IEEE80211_RADIOTAP_HE_DATA1_FORMAT_MASK		= 3,
	IEEE80211_RADIOTAP_HE_DATA1_FORMAT_SU		= 0,
	IEEE80211_RADIOTAP_HE_DATA1_FORMAT_EXT_SU	= 1,
	IEEE80211_RADIOTAP_HE_DATA1_FORMAT_MU		= 2,
	IEEE80211_RADIOTAP_HE_DATA1_FORMAT_TRIG		= 3,

	IEEE80211_RADIOTAP_HE_DATA1_BSS_COLOR_KNOWN	= 0x0004,
	IEEE80211_RADIOTAP_HE_DATA1_BEAM_CHANGE_KNOWN	= 0x0008,
	IEEE80211_RADIOTAP_HE_DATA1_UL_DL_KNOWN		= 0x0010,
	IEEE80211_RADIOTAP_HE_DATA1_DATA_MCS_KNOWN	= 0x0020,
	IEEE80211_RADIOTAP_HE_DATA1_DATA_DCM_KNOWN	= 0x0040,
	IEEE80211_RADIOTAP_HE_DATA1_CODING_KNOWN	= 0x0080,
	IEEE80211_RADIOTAP_HE_DATA1_LDPC_XSYMSEG_KNOWN	= 0x0100,
	IEEE80211_RADIOTAP_HE_DATA1_STBC_KNOWN		= 0x0200,
	IEEE80211_RADIOTAP_HE_DATA1_SPTL_REUSE_KNOWN	= 0x0400,
	IEEE80211_RADIOTAP_HE_DATA1_SPTL_REUSE2_KNOWN	= 0x0800,
	IEEE80211_RADIOTAP_HE_DATA1_SPTL_REUSE3_KNOWN	= 0x1000,
	IEEE80211_RADIOTAP_HE_DATA1_SPTL_REUSE4_KNOWN	= 0x2000,
	IEEE80211_RADIOTAP_HE_DATA1_BW_RU_ALLOC_KNOWN	= 0x4000,
	IEEE80211_RADIOTAP_HE_DATA1_DOPPLER_KNOWN	= 0x8000,

	IEEE80211_RADIOTAP_HE_DATA2_PRISEC_80_KNOWN	= 0x0001,
	IEEE80211_RADIOTAP_HE_DATA2_GI_KNOWN		= 0x0002,
	IEEE80211_RADIOTAP_HE_DATA2_NUM_LTF_SYMS_KNOWN	= 0x0004,
	IEEE80211_RADIOTAP_HE_DATA2_PRE_FEC_PAD_KNOWN	= 0x0008,
	IEEE80211_RADIOTAP_HE_DATA2_TXBF_KNOWN		= 0x0010,
	IEEE80211_RADIOTAP_HE_DATA2_PE_DISAMBIG_KNOWN	= 0x0020,
	IEEE80211_RADIOTAP_HE_DATA2_TXOP_KNOWN		= 0x0040,
	IEEE80211_RADIOTAP_HE_DATA2_MIDAMBLE_KNOWN	= 0x0080,
	IEEE80211_RADIOTAP_HE_DATA2_RU_OFFSET		= 0x3f00,
	IEEE80211_RADIOTAP_HE_DATA2_RU_OFFSET_KNOWN	= 0x4000,
	IEEE80211_RADIOTAP_HE_DATA2_PRISEC_80_SEC	= 0x8000,

	IEEE80211_RADIOTAP_HE_DATA3_BSS_COLOR		= 0x003f,
	IEEE80211_RADIOTAP_HE_DATA3_BEAM_CHANGE		= 0x0040,
	IEEE80211_RADIOTAP_HE_DATA3_UL_DL		= 0x0080,
	IEEE80211_RADIOTAP_HE_DATA3_DATA_MCS		= 0x0f00,
	IEEE80211_RADIOTAP_HE_DATA3_DATA_DCM		= 0x1000,
	IEEE80211_RADIOTAP_HE_DATA3_CODING		= 0x2000,
	IEEE80211_RADIOTAP_HE_DATA3_LDPC_XSYMSEG	= 0x4000,
	IEEE80211_RADIOTAP_HE_DATA3_STBC		= 0x8000,

	IEEE80211_RADIOTAP_HE_DATA4_SU_MU_SPTL_REUSE	= 0x000f,
	IEEE80211_RADIOTAP_HE_DATA4_MU_STA_ID		= 0x7ff0,
	IEEE80211_RADIOTAP_HE_DATA4_TB_SPTL_REUSE1	= 0x000f,
	IEEE80211_RADIOTAP_HE_DATA4_TB_SPTL_REUSE2	= 0x00f0,
	IEEE80211_RADIOTAP_HE_DATA4_TB_SPTL_REUSE3	= 0x0f00,
	IEEE80211_RADIOTAP_HE_DATA4_TB_SPTL_REUSE4	= 0xf000,

	IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC	= 0x000f,
		IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_20MHZ	= 0,
		IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_40MHZ	= 1,
		IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_80MHZ	= 2,
		IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_160MHZ	= 3,
		IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_26T	= 4,
		IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_52T	= 5,
		IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_106T	= 6,
		IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_242T	= 7,
		IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_484T	= 8,
		IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_996T	= 9,
		IEEE80211_RADIOTAP_HE_DATA5_DATA_BW_RU_ALLOC_2x996T	= 10,

	IEEE80211_RADIOTAP_HE_DATA5_GI			= 0x0030,
		IEEE80211_RADIOTAP_HE_DATA5_GI_0_8			= 0,
		IEEE80211_RADIOTAP_HE_DATA5_GI_1_6			= 1,
		IEEE80211_RADIOTAP_HE_DATA5_GI_3_2			= 2,

	IEEE80211_RADIOTAP_HE_DATA5_LTF_SIZE		= 0x00c0,
		IEEE80211_RADIOTAP_HE_DATA5_LTF_SIZE_UNKNOWN		= 0,
		IEEE80211_RADIOTAP_HE_DATA5_LTF_SIZE_1X			= 1,
		IEEE80211_RADIOTAP_HE_DATA5_LTF_SIZE_2X			= 2,
		IEEE80211_RADIOTAP_HE_DATA5_LTF_SIZE_4X			= 3,
	IEEE80211_RADIOTAP_HE_DATA5_NUM_LTF_SYMS	= 0x0700,
	IEEE80211_RADIOTAP_HE_DATA5_PRE_FEC_PAD		= 0x3000,
	IEEE80211_RADIOTAP_HE_DATA5_TXBF		= 0x4000,
	IEEE80211_RADIOTAP_HE_DATA5_PE_DISAMBIG		= 0x8000,

	IEEE80211_RADIOTAP_HE_DATA6_NSTS		= 0x000f,
	IEEE80211_RADIOTAP_HE_DATA6_DOPPLER		= 0x0010,
	IEEE80211_RADIOTAP_HE_DATA6_TXOP		= 0x7f00,
	IEEE80211_RADIOTAP_HE_DATA6_MIDAMBLE_PDCTY	= 0x8000,
};

struct ieee80211_radiotap_he_mu {
	__le16 flags1, flags2;
	u8 ru_ch1[4];
	u8 ru_ch2[4];
};

enum ieee80211_radiotap_he_mu_bits {
	IEEE80211_RADIOTAP_HE_MU_FLAGS1_SIG_B_MCS		= 0x000f,
	IEEE80211_RADIOTAP_HE_MU_FLAGS1_SIG_B_MCS_KNOWN		= 0x0010,
	IEEE80211_RADIOTAP_HE_MU_FLAGS1_SIG_B_DCM		= 0x0020,
	IEEE80211_RADIOTAP_HE_MU_FLAGS1_SIG_B_DCM_KNOWN		= 0x0040,
	IEEE80211_RADIOTAP_HE_MU_FLAGS1_CH2_CTR_26T_RU_KNOWN	= 0x0080,
	IEEE80211_RADIOTAP_HE_MU_FLAGS1_CH1_RU_KNOWN		= 0x0100,
	IEEE80211_RADIOTAP_HE_MU_FLAGS1_CH2_RU_KNOWN		= 0x0200,
	IEEE80211_RADIOTAP_HE_MU_FLAGS1_CH1_CTR_26T_RU_KNOWN	= 0x1000,
	IEEE80211_RADIOTAP_HE_MU_FLAGS1_CH1_CTR_26T_RU		= 0x2000,
	IEEE80211_RADIOTAP_HE_MU_FLAGS1_SIG_B_COMP_KNOWN	= 0x4000,
	IEEE80211_RADIOTAP_HE_MU_FLAGS1_SIG_B_SYMS_USERS_KNOWN	= 0x8000,

	IEEE80211_RADIOTAP_HE_MU_FLAGS2_BW_FROM_SIG_A_BW	= 0x0003,
		IEEE80211_RADIOTAP_HE_MU_FLAGS2_BW_FROM_SIG_A_BW_20MHZ	= 0x0000,
		IEEE80211_RADIOTAP_HE_MU_FLAGS2_BW_FROM_SIG_A_BW_40MHZ	= 0x0001,
		IEEE80211_RADIOTAP_HE_MU_FLAGS2_BW_FROM_SIG_A_BW_80MHZ	= 0x0002,
		IEEE80211_RADIOTAP_HE_MU_FLAGS2_BW_FROM_SIG_A_BW_160MHZ	= 0x0003,
	IEEE80211_RADIOTAP_HE_MU_FLAGS2_BW_FROM_SIG_A_BW_KNOWN	= 0x0004,
	IEEE80211_RADIOTAP_HE_MU_FLAGS2_SIG_B_COMP		= 0x0008,
	IEEE80211_RADIOTAP_HE_MU_FLAGS2_SIG_B_SYMS_USERS	= 0x00f0,
	IEEE80211_RADIOTAP_HE_MU_FLAGS2_PUNC_FROM_SIG_A_BW	= 0x0300,
	IEEE80211_RADIOTAP_HE_MU_FLAGS2_PUNC_FROM_SIG_A_BW_KNOWN= 0x0400,
	IEEE80211_RADIOTAP_HE_MU_FLAGS2_CH2_CTR_26T_RU		= 0x0800,
};
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
#define rwnx_cfg80211_add_iface(wiphy, name, name_assign_type, type, params) \
    rwnx_cfg80211_add_iface(wiphy, name, type, u32 *flags, params)
#else
#define rwnx_cfg80211_add_iface(wiphy, name, name_assign_type, type, params) \
    rwnx_cfg80211_add_iface(wiphy, name, name_assign_type, type, u32 *flags, params)
#endif

#define rwnx_cfg80211_change_iface(wiphy, dev, type, params) \
    rwnx_cfg80211_change_iface(wiphy, dev, type, u32 *flags, params)

#define CCFS0(vht) vht->center_freq_seg1_idx
#define CCFS1(vht) vht->center_freq_seg2_idx

#else
#define CCFS0(vht) vht->center_freq_seg0_idx
#define CCFS1(vht) vht->center_freq_seg1_idx

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
#define cfg80211_cqm_rssi_notify(dev, event, level, gfp) \
    cfg80211_cqm_rssi_notify(dev, event, gfp)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 9, 0)
#define ieee80211_amsdu_to_8023s(skb, list, addr, iftype, extra_headroom, check_da, check_sa) \
    ieee80211_amsdu_to_8023s(skb, list, addr, iftype, extra_headroom, false)
#endif

#if LINUX_VERSION_CODE  < KERNEL_VERSION(4, 7, 0)
#define NUM_NL80211_BANDS IEEE80211_NUM_BANDS
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 2, 0)
#define cfg80211_disconnected(dev, reason, ie, len, local, gfp) \
    cfg80211_disconnected(dev, reason, ie, len, gfp)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)) && !(defined CONFIG_VENDOR_RWNX)
#define ieee80211_chandef_to_operating_class(chan_def, op_class) 0
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
#define SURVEY_INFO_TIME          SURVEY_INFO_CHANNEL_TIME
#define SURVEY_INFO_TIME_BUSY     SURVEY_INFO_CHANNEL_TIME_BUSY
#define SURVEY_INFO_TIME_EXT_BUSY SURVEY_INFO_CHANNEL_TIME_EXT_BUSY
#define SURVEY_INFO_TIME_RX       SURVEY_INFO_CHANNEL_TIME_RX
#define SURVEY_INFO_TIME_TX       SURVEY_INFO_CHANNEL_TIME_TX

#define SURVEY_TIME(s) s->channel_time
#define SURVEY_TIME_BUSY(s) s->channel_time_busy
#else
#define SURVEY_TIME(s) s->time
#define SURVEY_TIME_BUSY(s) s->time_busy
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
#define cfg80211_ch_switch_started_notify(dev, chandef, count)

#define WLAN_BSS_COEX_INFORMATION_REQUEST	BIT(0)
#define WLAN_EXT_CAPA1_EXT_CHANNEL_SWITCHING	BIT(2)
#define WLAN_EXT_CAPA4_TDLS_BUFFER_STA		BIT(4)
#define WLAN_EXT_CAPA4_TDLS_PEER_PSM		BIT(5)
#define WLAN_EXT_CAPA4_TDLS_CHAN_SWITCH		BIT(6)
#define WLAN_EXT_CAPA5_TDLS_CH_SW_PROHIBITED	BIT(7)
#define NL80211_FEATURE_TDLS_CHANNEL_SWITCH     0

#define STA_TDLS_INITIATOR(sta) 0

#define REGULATORY_IGNORE_STALE_KICKOFF 0
#else
#define STA_TDLS_INITIATOR(sta) sta->tdls_initiator

#ifndef REGULATORY_IGNORE_STALE_KICKOFF
#define REGULATORY_IGNORE_STALE_KICKOFF 0
#endif

#endif


#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 18, 0)) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0))
#define cfg80211_rx_mgmt(wdev, freq, rssi, buf, len, flags)             \
    cfg80211_rx_mgmt(wdev, freq, rssi, buf, len, flags, GFP_ATOMIC)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3, 11, 0)
#define cfg80211_rx_mgmt(wdev, freq, rssi, buf, len, flags)             \
    cfg80211_rx_mgmt(wdev, freq, rssi, buf, len, GFP_ATOMIC)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)

#if 0
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0)
#define rwnx_cfg80211_tdls_mgmt(wiphy, dev, peer, act, tok, status, peer_capability, initiator, buf, len) \
    rwnx_cfg80211_tdls_mgmt(wiphy, dev, peer, act, tok, status, peer_capability, buf, len)
#else
#define rwnx_cfg80211_tdls_mgmt(wiphy, dev, peer, act, tok, status, peer_capability, initiator, buf, len) \
    rwnx_cfg80211_tdls_mgmt(wiphy, dev, peer, act, tok, status, buf, len)
#endif
#endif

#include <linux/types.h>

struct ieee80211_wmm_ac_param {
	u8 aci_aifsn; /* AIFSN, ACM, ACI */
	u8 cw; /* ECWmin, ECWmax (CW = 2^ECW - 1) */
	__le16 txop_limit;
} __packed;

struct ieee80211_wmm_param_ie {
	u8 element_id; /* Element ID: 221 (0xdd); */
	u8 len; /* Length: 24 */
	/* required fields for WMM version 1 */
	u8 oui[3]; /* 00:50:f2 */
	u8 oui_type; /* 2 */
	u8 oui_subtype; /* 1 */
	u8 version; /* 1 for WMM version 1.0 */
	u8 qos_info; /* AP/STA specific QoS info */
	u8 reserved; /* 0 */
	/* AC_BE, AC_BK, AC_VI, AC_VO */
	struct ieee80211_wmm_ac_param ac[4];
} __packed;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)
enum {
    IEEE80211_HE_MCS_SUPPORT_0_7    = 0,
    IEEE80211_HE_MCS_SUPPORT_0_9    = 1,
    IEEE80211_HE_MCS_SUPPORT_0_11   = 2,
    IEEE80211_HE_MCS_NOT_SUPPORTED  = 3,
};
#endif

/* MAC80211 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 18, 0)
#define rwnx_ops_mgd_prepare_tx(hw, vif, duration) \
    rwnx_ops_mgd_prepare_tx(hw, vif)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)

#define RX_ENC_HT(s) s->flag |= RX_FLAG_HT
#define RX_ENC_HT_GF(s) s->flag |= (RX_FLAG_HT | RX_FLAG_HT_GF)
#define RX_ENC_VHT(s) s->flag |= RX_FLAG_HT
#define RX_ENC_HE(s) s->flag |= RX_FLAG_HT
#define RX_ENC_FLAG_SHORT_GI(s) s->flag |= RX_FLAG_SHORT_GI
#define RX_ENC_FLAG_SHORT_PRE(s) s->flag |= RX_FLAG_SHORTPRE
#define RX_ENC_FLAG_LDPC(s) s->flag |= RX_FLAG_LDPC
#define RX_BW_40MHZ(s) s->flag |= RX_FLAG_40MHZ
#define RX_BW_80MHZ(s) s->vht_flag |= RX_VHT_FLAG_80MHZ
#define RX_BW_160MHZ(s) s->vht_flag |= RX_VHT_FLAG_160MHZ
#define RX_NSS(s) s->vht_nss

#else
#define RX_ENC_HT(s) s->encoding = RX_ENC_HT
#define RX_ENC_HT_GF(s) { s->encoding = RX_ENC_HT;      \
        s->enc_flags |= RX_ENC_FLAG_HT_GF; }
#define RX_ENC_VHT(s) s->encoding = RX_ENC_VHT
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)
#define RX_ENC_HE(s) s->encoding = RX_ENC_VHT
#else
#define RX_ENC_HE(s) s->encoding = RX_ENC_HE
#endif
#define RX_ENC_FLAG_SHORT_GI(s) s->enc_flags |= RX_ENC_FLAG_SHORT_GI
#define RX_ENC_FLAG_SHORT_PRE(s) s->enc_flags |= RX_ENC_FLAG_SHORTPRE
#define RX_ENC_FLAG_LDPC(s) s->enc_flags |= RX_ENC_FLAG_LDPC
#define RX_BW_40MHZ(s) s->bw = RATE_INFO_BW_40
#define RX_BW_80MHZ(s) s->bw = RATE_INFO_BW_80
#define RX_BW_160MHZ(s) s->bw = RATE_INFO_BW_160
#define RX_NSS(s) s->nss

#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
#define ieee80211_cqm_rssi_notify(vif, event, level, gfp) \
    ieee80211_cqm_rssi_notify(vif, event, gfp)
#endif

#ifndef CONFIG_VENDOR_RWNX_AMSDUS_TX
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0))
#define rwnx_ops_ampdu_action(hw, vif, params) \
    rwnx_ops_ampdu_action(hw, vif, enum ieee80211_ampdu_mlme_action action, \
                          struct ieee80211_sta *sta, u16 tid, u16 *ssn, u8 buf_size)
#elif  (LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0))
#define rwnx_ops_ampdu_action(hw, vif, params) \
    rwnx_ops_ampdu_action(hw, vif, enum ieee80211_ampdu_mlme_action action, \
                          struct ieee80211_sta *sta, u16 tid, u16 *ssn, u8 buf_size, \
                          bool amsdu)
#endif
#endif /* CONFIG_VENDOR_RWNX_AMSDUS_TX */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 2, 0)
#define IEEE80211_HW_SUPPORT_FAST_XMIT 0
#define ieee80211_hw_check(hw, feat) (hw->flags & IEEE80211_HW_##feat)
#define ieee80211_hw_set(hw, feat) {hw->flags |= IEEE80211_HW_##feat;}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
#define rwnx_ops_sw_scan_start(hw, vif, mac_addr) \
    rwnx_ops_sw_scan_start(hw)
#define rwnx_ops_sw_scan_complete(hw, vif) \
    rwnx_ops_sw_scan_complete(hw)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
#define rwnx_ops_hw_scan(hw, vif, hw_req) \
    rwnx_ops_hw_scan(hw, vif, struct cfg80211_scan_request *req)
#endif

u16 rwnx_select_txq(struct rwnx_vif *rwnx_vif, struct sk_buff *skb);
/* NET */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 13, 0)
#define rwnx_select_queue(dev, skb, sb_dev) \
    rwnx_select_queue(dev, skb)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4, 19, 0)
#define rwnx_select_queue(dev, skb, sb_dev) \
    rwnx_select_queue(dev, skb, void *accel_priv, select_queue_fallback_t fallback)
#elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 2, 0)
#define rwnx_select_queue(dev, skb, sb_dev) \
    rwnx_select_queue(dev, skb, sb_dev, select_queue_fallback_t fallback)
#else
#define rwnx_select_queue(dev, skb, sb_dev) \
    rwnx_select_queue(dev, skb, sb_dev)
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 16, 0)) && !(defined CONFIG_VENDOR_RWNX)
#define sk_pacing_shift_update(sk, shift)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
#define alloc_netdev_mqs(size, name, assign, setup, txqs, rxqs) \
    alloc_netdev_mqs(size, name, setup, txqs, rxqs)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
#define NET_NAME_UNKNOWN 0
#endif

/* TRACE */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 2, 0)
#define trace_print_symbols_seq ftrace_print_symbols_seq
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
#define trace_seq_buffer_ptr(p) p->buffer + p->len
#endif

/* TIME */
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 8, 0)
#define time64_to_tm(t, o, tm) time_to_tm((time_t)t, o, tm)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 19, 0)
#define ktime_get_real_seconds get_seconds
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
typedef __s64 time64_t;
#endif

#endif /* _RWNX_COMPAT_H_ */
