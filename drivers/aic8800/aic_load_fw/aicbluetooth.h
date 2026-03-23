#ifndef _AICBLUETOOTH_H
#define _AICBLUETOOTH_H

struct aicbt_patch_table {
	char     *name;
	uint32_t type;
	uint32_t *data;
	uint32_t len;
	struct aicbt_patch_table *next;
};

typedef struct
{
    int8_t enable;
    int8_t dsss;
    int8_t ofdmlowrate_2g4;
    int8_t ofdm64qam_2g4;
    int8_t ofdm256qam_2g4;
    int8_t ofdm1024qam_2g4;
    int8_t ofdmlowrate_5g;
    int8_t ofdm64qam_5g;
    int8_t ofdm256qam_5g;
    int8_t ofdm1024qam_5g;
} txpwr_idx_conf_t;


typedef struct
{
    int8_t enable;
    int8_t chan_1_4;
    int8_t chan_5_9;
    int8_t chan_10_13;
    int8_t chan_36_64;
    int8_t chan_100_120;
    int8_t chan_122_140;
    int8_t chan_142_165;
} txpwr_ofst_conf_t;

typedef struct
{
    int8_t enable;
    int8_t xtal_cap;
    int8_t xtal_cap_fine;
} xtal_cap_conf_t;

u32 aic_crc32(u8 *p, u32 len, u32 crc);
void get_fw_path(char* fw_path);
void set_testmode(int val);
int get_testmode(void);
int get_hardware_info(void);
int get_adap_test(void);
int get_flash_bin_size(void);
u32 get_flash_bin_crc(void);
void get_userconfig_xtal_cap(xtal_cap_conf_t *xtal_cap);
void get_userconfig_txpwr_idx(txpwr_idx_conf_t *txpwr_idx);
void get_userconfig_txpwr_ofst(txpwr_ofst_conf_t *txpwr_ofst);
void rwnx_plat_userconfig_set_value(char *command, char *value);
void rwnx_plat_userconfig_parsing(char *buffer, int size);

int aic_bt_platform_init(struct aic_usb_dev *sdiodev);

void aic_bt_platform_deinit(struct aic_usb_dev *sdiodev);

int rwnx_plat_bin_fw_upload_android(struct aic_usb_dev *sdiodev, u32 fw_addr,
                               char *filename);

int rwnx_plat_m2d_flash_ota_android(struct aic_usb_dev *usbdev, char *filename);

int rwnx_plat_m2d_flash_ota_check(struct aic_usb_dev *usbdev, char *filename);

int rwnx_plat_bin_fw_patch_table_upload_android(struct aic_usb_dev *usbdev, char *filename);

int rwnx_plat_userconfig_upload_android(struct aic_usb_dev *usbdev, char *filename);
int rwnx_plat_flash_bin_upload_android(struct aic_usb_dev *usbdev, u32 fw_addr, char *filename);

int8_t rwnx_atoi(char *value);
uint32_t rwnx_atoli(char *value);
int aicbt_patch_table_free(struct aicbt_patch_table *head);
struct aicbt_patch_table *aicbt_patch_table_alloc(struct aic_usb_dev *usbdev, const char *filename);
#ifdef CONFIG_LOAD_BT_CONF
void aicbt_parse_config(struct aic_usb_dev *usbdev, const char *filename);
#endif
int aicbt_patch_info_unpack(struct aicbt_patch_info_t *patch_info, struct aicbt_patch_table *head_t);
int aicbt_patch_table_load(struct aic_usb_dev *usbdev, struct aicbt_patch_table *_head);

#endif
