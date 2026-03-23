
struct scanu_result_wext{
	struct list_head scanu_re_list;
	struct cfg80211_bss *bss;
	struct scanu_result_ind *ind;
	u32_l *payload;
};

void aicwf_set_wireless_ext( struct net_device *ndev, struct rwnx_hw *rwnx_hw);
void aicwf_scan_complete_event(struct net_device *dev);
int aic_get_sec_ie(u8 *in_ie, uint in_len, u8 *rsn_ie, u16 *rsn_len, u8 *wpa_ie, u16 *wpa_len);
u8 aicwf_get_is_wps_ie(u8 *ie_ptr, uint *wps_ielen);

