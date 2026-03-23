#include <linux/version.h>
#include <linux/vmalloc.h>
#include "aicbluetooth_cmds.h"
#include "aicwf_usb.h"
#include "aic_txrxif.h"
#include "md5.h"
#include "aicbluetooth.h"
#include "aicwf_debug.h"
#ifdef CONFIG_USE_FW_REQUEST
#include <linux/firmware.h>
#endif

//Parser state
#define INIT 0
#define CMD 1
#define PRINT 2
#define GET_VALUE 3

extern int flash_erase_len;
int flash_write_size = 0;
u32 flash_write_bin_crc = 0;


txpwr_idx_conf_t userconfig_txpwr_idx = {
	.enable 		  = 1,
	.dsss			  = 9,
	.ofdmlowrate_2g4  = 8,
	.ofdm64qam_2g4	  = 8,
	.ofdm256qam_2g4   = 8,
	.ofdm1024qam_2g4  = 8,
	.ofdmlowrate_5g   = 11,
	.ofdm64qam_5g	  = 10,
	.ofdm256qam_5g	  = 9,
	.ofdm1024qam_5g   = 9

};


txpwr_ofst_conf_t userconfig_txpwr_ofst = {
	.enable = 1,
	.chan_1_4 = 0,
	.chan_5_9 = 0,
	.chan_10_13 = 0,
	.chan_36_64 = 0,
	.chan_100_120 = 0,
	.chan_122_140 = 0,
	.chan_142_165 = 0
};


xtal_cap_conf_t userconfig_xtal_cap = {
	.enable = 0,
	.xtal_cap = 24,
	.xtal_cap_fine = 31,
};

struct aicbt_info_t {
    uint32_t btmode;
    uint32_t btport;
    uint32_t uart_baud;
    uint32_t uart_flowctrl;
	uint32_t lpm_enable;
	uint32_t txpwr_lvl;
};

struct aicbsp_info_t {
    int hwinfo;
    uint32_t cpmode;
};


enum aicbt_btport_type {
    AICBT_BTPORT_NULL,
    AICBT_BTPORT_MB,
    AICBT_BTPORT_UART,
};

/*  btmode
 * used for force bt mode,if not AICBSP_MODE_NULL
 * efuse valid and vendor_info will be invalid, even has beed set valid
*/
enum aicbt_btmode_type {
    AICBT_BTMODE_BT_ONLY_SW = 0x0,    // bt only mode with switch
    AICBT_BTMODE_BT_WIFI_COMBO,       // wifi/bt combo mode
    AICBT_BTMODE_BT_ONLY,             // bt only mode without switch
    AICBT_BTMODE_BT_ONLY_TEST,        // bt only test mode
    AICBT_BTMODE_BT_WIFI_COMBO_TEST,  // wifi/bt combo test mode
    AICBT_BTMODE_BT_ONLY_COANT,       // bt only mode with no external switch
    AICBT_MODE_NULL = 0xFF,           // invalid value
};

/*  uart_baud
 * used for config uart baud when btport set to uart,
 * otherwise meaningless
*/
enum aicbt_uart_baud_type {
    AICBT_UART_BAUD_115200     = 115200,
    AICBT_UART_BAUD_921600     = 921600,
    AICBT_UART_BAUD_1_5M       = 1500000,
    AICBT_UART_BAUD_3_25M      = 3250000,
};

enum aicbt_uart_flowctrl_type {
    AICBT_UART_FLOWCTRL_DISABLE = 0x0,    // uart without flow ctrl
    AICBT_UART_FLOWCTRL_ENABLE,           // uart with flow ctrl
};

enum aicbsp_cpmode_type {
    AICBSP_CPMODE_WORK,
    AICBSP_CPMODE_TEST,
};
#define AIC_M2D_OTA_INFO_ADDR       0x88000020
#define AIC_M2D_OTA_DATA_ADDR       0x88000040
#define AIC_M2D_OTA_FLASH_ADDR      0x08004000
#define AIC_M2D_OTA_CODE_START_ADDR 0x08004188
#define AIC_M2D_OTA_VER_ADDR        0x0800418c
///aic bt tx pwr lvl :lsb->msb: first byte, min pwr lvl; second byte, max pwr lvl;
///pwr lvl:20(min), 30 , 40 , 50 , 60(max)
#define AICBT_TXPWR_LVL            0x00006020
#define AICBT_TXPWR_LVL_8800d80    0x00006F2F
#define AICBT_TXPWR_LVL_8800d80x2  0x00006F2F


#define AICBSP_MODE_BT_HCI_MODE_NULL              0
#define AICBSP_MODE_BT_HCI_MODE_MB                1
#define AICBSP_MODE_BT_HCI_MODE_UART              2

#define AICBSP_HWINFO_DEFAULT       (-1)
#define AICBSP_CPMODE_DEFAULT       AICBSP_CPMODE_WORK

#define AICBT_BTMODE_DEFAULT_8800d80x2      AICBT_BTMODE_BT_ONLY_COANT
#define AICBT_BTMODE_DEFAULT_8800d80        AICBT_BTMODE_BT_ONLY_COANT
#define AICBT_BTMODE_DEFAULT                AICBT_BTMODE_BT_ONLY
#define AICBT_BTPORT_DEFAULT                AICBT_BTPORT_MB
#define AICBT_UART_BAUD_DEFAULT             AICBT_UART_BAUD_1_5M
#define AICBT_UART_FC_DEFAULT               AICBT_UART_FLOWCTRL_ENABLE
#define AICBT_LPM_ENABLE_DEFAULT            0
#define AICBT_TXPWR_LVL_DEFAULT             AICBT_TXPWR_LVL
#define AICBT_TXPWR_LVL_DEFAULT_8800d80     AICBT_TXPWR_LVL_8800d80
#define AICBT_TXPWR_LVL_DEFAULT_8800d80x2   AICBT_TXPWR_LVL_8800d80x2


#define AIC_HW_INFO 0x21

#define FW_PATH_MAX 200
#if defined(CONFIG_PLATFORM_UBUNTU)
static const char* aic_default_fw_path = "/lib/firmware";
#else
static const char* aic_default_fw_path = "/vendor/etc/firmware";
#endif
char aic_fw_path[FW_PATH_MAX];
module_param_string(aic_fw_path, aic_fw_path, FW_PATH_MAX, 0660);
#ifdef CONFIG_M2D_OTA_AUTO_SUPPORT
char saved_sdk_ver[64];
module_param_string(saved_sdk_ver, saved_sdk_ver,64, 0660);
#endif



const u32 crc_tab[256] =
{
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
    0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
    0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
    0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
    0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
    0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
    0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
    0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
    0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
    0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
    0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
    0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
    0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
    0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
    0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
    0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
    0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
    0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
    0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
    0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
    0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
    0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
};
u32 aic_crc32(u8 *p, u32 len, u32 crc)
{
    while(len--)
    {
        crc = crc_tab[((crc & 0xFF) ^ *p++)] ^ (crc >> 8);
    }
    return crc;
}

int aic_bt_platform_init(struct aic_usb_dev *usbdev)
{
    rwnx_cmd_mgr_init(&usbdev->cmd_mgr);
    usbdev->cmd_mgr.usbdev = (void *)usbdev;
    return 0;

}

void aic_bt_platform_deinit(struct aic_usb_dev *usbdev)
{
	rwnx_cmd_mgr_deinit(&usbdev->cmd_mgr);
}

#define MD5(x) x[0],x[1],x[2],x[3],x[4],x[5],x[6],x[7],x[8],x[9],x[10],x[11],x[12],x[13],x[14],x[15]
#define MD5PINRT "file md5:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\r\n"

static int aic_load_firmware(u32 ** fw_buf, const char *name, struct device *device)
{

#ifdef CONFIG_USE_FW_REQUEST
	const struct firmware *fw = NULL;
	u32 *dst = NULL;
	void *buffer=NULL;
	MD5_CTX md5;
	unsigned char decrypt[16];
	int size = 0;
	int ret = 0;

	printk("%s: request firmware = %s \n", __func__ ,name);


	ret = request_firmware(&fw, name, NULL);
	
	if (ret < 0) {
		printk("Load %s fail\n", name);
		release_firmware(fw);
		return -1;
	}
	
	size = fw->size;
	dst = (u32 *)fw->data;

	if (size <= 0) {
		printk("wrong size of firmware file\n");
		release_firmware(fw);
		return -1;
	}


	buffer = vmalloc(size);
	memset(buffer, 0, size);
	memcpy(buffer, dst, size);
	
	*fw_buf = buffer;

	MD5Init(&md5);
	MD5Update(&md5, (unsigned char *)buffer, size);
	MD5Final(&md5, decrypt);
	printk(MD5PINRT, MD5(decrypt));
	
	release_firmware(fw);
	
	return size;
#else
    void *buffer=NULL;
    char *path=NULL;
    struct file *fp=NULL;
    int size = 0, len=0;//, i=0;
    ssize_t rdlen=0;
    //u32 *src=NULL, *dst = NULL;
	MD5_CTX md5;
	unsigned char decrypt[16];
#if defined(CONFIG_PLATFORM_UBUNTU)
    struct aicwf_bus *bus_if = dev_get_drvdata(device);
    struct aic_usb_dev *usb_dev = bus_if->bus_priv.usb;
#endif

    /* get the firmware path */
    path = __getname();
    if (!path){
            *fw_buf=NULL;
            return -1;
    }

    if (strlen(aic_fw_path) > 0) {
		printk("%s: use customer define fw_path\n", __func__);
		len = snprintf(path, FW_PATH_MAX, "%s/%s", aic_fw_path, name);
    } else {
    #if defined(CONFIG_PLATFORM_UBUNTU)
        if (usb_dev->chipid == PRODUCT_ID_AIC8800) {
            len = snprintf(path, FW_PATH_MAX, "%s/%s/%s",aic_default_fw_path, "aic8800", name);
        } else if (usb_dev->chipid == PRODUCT_ID_AIC8800D80) {
            len = snprintf(path, FW_PATH_MAX, "%s/%s/%s",aic_default_fw_path, "aic8800D80", name);
        } else if (usb_dev->chipid == PRODUCT_ID_AIC8800D80X2) {
            len = snprintf(path, FW_PATH_MAX, "%s/%s/%s",aic_default_fw_path, "aic8800D80X2", name);
        }else {
            printk("%s unknown chipid %d\n", __func__, usb_dev->chipid);
        }
	#else
		len = snprintf(path, FW_PATH_MAX, "%s/%s",aic_default_fw_path, name);
	#endif
    }

    if (len >= FW_PATH_MAX) {
    	printk("%s: %s file's path too long\n", __func__, name);
        *fw_buf=NULL;
        __putname(path);
        return -1;
    }

    printk("%s :firmware path = %s  \n", __func__ ,path);


    /* open the firmware file */
    fp=filp_open(path, O_RDONLY, 0);
    if(IS_ERR(fp) || (!fp)){
            printk("%s: %s file failed to open\n", __func__, name);
            if(IS_ERR(fp))
		printk("is_Err\n");
	if((!fp))
		printk("null\n");
	*fw_buf=NULL;
            __putname(path);
            fp=NULL;
            return -1;
    }

    size = i_size_read(file_inode(fp));
    if(size<=0){
            printk("%s: %s file size invalid %d\n", __func__, name, size);
            *fw_buf=NULL;
            __putname(path);
            filp_close(fp,NULL);
            fp=NULL;
            return -1;
}

    /* start to read from firmware file */
    buffer = vmalloc(size);
    memset(buffer, 0, size);
    if(!buffer){
            *fw_buf=NULL;
            __putname(path);
            filp_close(fp,NULL);
            fp=NULL;
            return -1;
    }


    #if LINUX_VERSION_CODE > KERNEL_VERSION(4, 13, 16)
    rdlen = kernel_read(fp, buffer, size, &fp->f_pos);
    #else
    rdlen = kernel_read(fp, fp->f_pos, buffer, size);
    #endif

    if(size != rdlen){
            printk("%s: %s file rdlen invalid %d %d\n", __func__, name, (int)rdlen, size);
            *fw_buf=NULL;
            __putname(path);
            filp_close(fp,NULL);
            fp=NULL;
            vfree(buffer);
            buffer=NULL;
            return -1;
    }
    if(rdlen > 0){
            fp->f_pos += rdlen;
            //printk("f_pos=%d\n", (int)fp->f_pos);
    }


#if 0
   /*start to transform the data format*/
    src = (u32*)buffer;
    //printk("malloc dst\n");
    dst = (u32*)vmalloc(size);
    memset(dst, 0, size);

    if(!dst){
            *fw_buf=NULL;
            __putname(path);
            filp_close(fp,NULL);
            fp=NULL;
            vfree(buffer);
            buffer=NULL;
            return -1;
    }

    for(i=0;i<(size/4);i++){
            dst[i] = src[i];
    }
#endif

    __putname(path);
    filp_close(fp,NULL);
    fp=NULL;
    //vfree(buffer);
    //buffer=NULL;
    //*fw_buf = dst;
	*fw_buf = (u32 *)buffer;

	MD5Init(&md5);
	//MD5Update(&md5, (unsigned char *)dst, size);
	MD5Update(&md5, (unsigned char *)buffer, size);
	MD5Final(&md5, decrypt);

	printk(MD5PINRT, MD5(decrypt));

    return size;
#endif

}

int rwnx_plat_bin_fw_upload_android(struct aic_usb_dev *usbdev, u32 fw_addr,
                               char *filename)
{
    struct device *dev = usbdev->dev;
    unsigned int i=0;
    int size;
    u32 *dst=NULL;
    int err=0;

    /* load aic firmware */
    size = aic_load_firmware(&dst, filename, dev);
    if(size<=0){
            printk("wrong size of firmware file\n");
            vfree(dst);
            dst = NULL;
            return -1;
    }

    /* Copy the file on the Embedded side */
    printk("### Upload %s firmware, @ = %x  size=%d\n", filename, fw_addr, size);

    if (size > 1024) {// > 1KB data
        for (i = 0; i < (size - 1024); i += 1024) {//each time write 1KB
            err = rwnx_send_dbg_mem_block_write_req(usbdev, fw_addr + i, 1024, dst + i / 4);
                if (err) {
                printk("bin upload fail: %x, err:%d\r\n", fw_addr + i, err);
                break;
            }
        }
    }

    if (!err && (i < size)) {// <1KB data
        err = rwnx_send_dbg_mem_block_write_req(usbdev, fw_addr + i, size - i, dst + i / 4);
        if (err) {
            printk("bin upload fail: %x, err:%d\r\n", fw_addr + i, err);
        }
    }

    if (dst) {
        vfree(dst);
        dst = NULL;
    }

    printk("fw download complete\n\n");

    return err;
}

extern int testmode;
#ifdef CONFIG_M2D_OTA_AUTO_SUPPORT
int rwnx_plat_m2d_flash_ota_android(struct aic_usb_dev *usbdev, char *filename)
{
    struct device *dev = usbdev->dev;
    unsigned int i=0;
    int size;
    u32 *dst=NULL;
    int err=0;
	int ret;
	u8 bond_id;
    const u32 mem_addr = 0x40500000;
    struct dbg_mem_read_cfm rd_mem_addr_cfm;

    ret = rwnx_send_dbg_mem_read_req(usbdev, mem_addr, &rd_mem_addr_cfm);
    if (ret) {
        printk("m2d %x rd fail: %d\n", mem_addr, ret);
        return ret;
    }
    bond_id = (u8)(rd_mem_addr_cfm.memdata >> 24);
    printk("%x=%x\n", rd_mem_addr_cfm.memaddr, rd_mem_addr_cfm.memdata);
	if (bond_id & (1<<1)) {
		//flash is invalid
		printk("m2d flash is invalid\n");
		return -1;
	}

    /* load aic firmware */
    size = aic_load_firmware(&dst, filename, dev);
    if(size<=0){
            printk("wrong size of m2d file\n");
            vfree(dst);
            dst = NULL;
            return -1;
    }

    /* Copy the file on the Embedded side */
    printk("### Upload m2d %s flash, size=%d\n", filename, size);

	/*send info first*/
	err = rwnx_send_dbg_mem_block_write_req(usbdev, AIC_M2D_OTA_INFO_ADDR, 4, (u32 *)&size);
	
	/*send data first*/
    if (size > 1024) {// > 1KB data
        for (i = 0; i < (size - 1024); i += 1024) {//each time write 1KB
            err = rwnx_send_dbg_mem_block_write_req(usbdev, AIC_M2D_OTA_DATA_ADDR, 1024, dst + i / 4);
                if (err) {
                printk("m2d upload fail: %x, err:%d\r\n", AIC_M2D_OTA_DATA_ADDR, err);
                break;
            }
        }
    }

    if (!err && (i < size)) {// <1KB data
        err = rwnx_send_dbg_mem_block_write_req(usbdev, AIC_M2D_OTA_DATA_ADDR, size - i, dst + i / 4);
        if (err) {
            printk("m2d upload fail: %x, err:%d\r\n", AIC_M2D_OTA_DATA_ADDR, err);
        }
    }

    if (dst) {
        vfree(dst);
        dst = NULL;
    }
	testmode = FW_NORMAL_MODE;

    printk("m2d flash update complete\n\n");

    return err;
}

int rwnx_plat_m2d_flash_ota_check(struct aic_usb_dev *usbdev, char *filename)
{
    struct device *dev = usbdev->dev;
    unsigned int i=0,j=0;
    int size;
    u32 *dst=NULL;
    int err=0;
	int ret=0;
	u8 bond_id;
    const u32 mem_addr = 0x40500000;
	const u32 mem_addr_code_start = AIC_M2D_OTA_CODE_START_ADDR;
	const u32 mem_addr_sdk_ver = AIC_M2D_OTA_VER_ADDR;
	const u32 driver_code_start_idx = (AIC_M2D_OTA_CODE_START_ADDR-AIC_M2D_OTA_FLASH_ADDR)/4;
	const u32 driver_sdk_ver_idx = (AIC_M2D_OTA_VER_ADDR-AIC_M2D_OTA_FLASH_ADDR)/4;
	u32 driver_sdk_ver_addr_idx = 0;
	u32 code_start_addr = 0xffffffff;
	u32 sdk_ver_addr = 0xffffffff;
	u32 drv_code_start_addr = 0xffffffff;
	u32 drv_sdk_ver_addr = 0xffffffff;
    struct dbg_mem_read_cfm rd_mem_addr_cfm;
	char m2d_sdk_ver[64];
	char flash_sdk_ver[64];
	u32 flash_ver[16];
	u32 ota_ver[16];

    ret = rwnx_send_dbg_mem_read_req(usbdev, mem_addr, &rd_mem_addr_cfm);
    if (ret) {
        printk("m2d %x rd fail: %d\n", mem_addr, ret);
        return ret;
    }
    bond_id = (u8)(rd_mem_addr_cfm.memdata >> 24);
    printk("%x=%x\n", rd_mem_addr_cfm.memaddr, rd_mem_addr_cfm.memdata);
	if (bond_id & (1<<1)) {
		//flash is invalid
		printk("m2d flash is invalid\n");
		return -1;
	}
    ret = rwnx_send_dbg_mem_read_req(usbdev, mem_addr_code_start, &rd_mem_addr_cfm);
	if (ret){
        printk("mem_addr_code_start %x rd fail: %d\n", mem_addr_code_start, ret);
        return ret;
	}
	code_start_addr = rd_mem_addr_cfm.memdata;

    ret = rwnx_send_dbg_mem_read_req(usbdev, mem_addr_sdk_ver, &rd_mem_addr_cfm);
	if (ret){
        printk("mem_addr_sdk_ver %x rd fail: %d\n", mem_addr_code_start, ret);
        return ret;
	}
	sdk_ver_addr = rd_mem_addr_cfm.memdata;
	printk("code_start_addr: 0x%x,  sdk_ver_addr: 0x%x\n", code_start_addr,sdk_ver_addr);

	/* load aic firmware */
	size = aic_load_firmware(&dst, filename, dev);
	if(size<=0){
			printk("wrong size of m2d file\n");
			vfree(dst);
			dst = NULL;
			return -1;
	}
	if(code_start_addr == 0xffffffff && sdk_ver_addr == 0xffffffff) {
		printk("########m2d flash old version , must be upgrade\n");
		drv_code_start_addr = dst[driver_code_start_idx];
		drv_sdk_ver_addr = dst[driver_sdk_ver_idx];

		printk("drv_code_start_addr: 0x%x,	drv_sdk_ver_addr: 0x%x\n", drv_code_start_addr,drv_sdk_ver_addr);

		if(drv_sdk_ver_addr == 0xffffffff){
			printk("########driver m2d_ota.bin is old ,not need upgrade\n");
			return -1;
		}

	} else {
		for(i=0;i<16;i++){
			ret = rwnx_send_dbg_mem_read_req(usbdev, (sdk_ver_addr+i*4), &rd_mem_addr_cfm);
			if (ret){
				printk("mem_addr_sdk_ver %x rd fail: %d\n", mem_addr_code_start, ret);
				return ret;
			}
			flash_ver[i] = rd_mem_addr_cfm.memdata;
		}
		memcpy((u8 *)flash_sdk_ver,(u8 *)flash_ver,64);
        memcpy((u8 *)saved_sdk_ver,(u8 *)flash_sdk_ver,64);
		printk("flash SDK Version: %s\r\n\r\n", flash_sdk_ver);
				
		drv_code_start_addr = dst[driver_code_start_idx];
		drv_sdk_ver_addr = dst[driver_sdk_ver_idx];

		printk("drv_code_start_addr: 0x%x,	drv_sdk_ver_addr: 0x%x\n", drv_code_start_addr,drv_sdk_ver_addr);

		if(drv_sdk_ver_addr == 0xffffffff){
			printk("########driver m2d_ota.bin is old ,not need upgrade\n");
			return -1;
		}

		driver_sdk_ver_addr_idx = (drv_sdk_ver_addr-drv_code_start_addr)/4;
		printk("driver_sdk_ver_addr_idx %d\n",driver_sdk_ver_addr_idx);

		if (driver_sdk_ver_addr_idx){
			for(j = 0; j < 16; j++){
				ota_ver[j] = dst[driver_sdk_ver_addr_idx+j];
			}
			memcpy((u8 *)m2d_sdk_ver,(u8 *)ota_ver,64);
			printk("m2d_ota SDK Version: %s\r\n\r\n", m2d_sdk_ver);
		} else {
			return -1;
		}
		
		if(!strcmp(m2d_sdk_ver,flash_sdk_ver)){
			printk("######## m2d %s flash is not need upgrade\r\n", filename);
			return -1;
		}
	}

    /* Copy the file on the Embedded side */
    printk("### Upload m2d %s flash, size=%d\n", filename, size);

	/*send info first*/
	err = rwnx_send_dbg_mem_block_write_req(usbdev, AIC_M2D_OTA_INFO_ADDR, 4, (u32 *)&size);
	
	/*send data first*/
    if (size > 1024) {// > 1KB data
        for (i = 0; i < (size - 1024); i += 1024) {//each time write 1KB
            err = rwnx_send_dbg_mem_block_write_req(usbdev, AIC_M2D_OTA_DATA_ADDR, 1024, dst + i / 4);
                if (err) {
                printk("m2d upload fail: %x, err:%d\r\n", AIC_M2D_OTA_DATA_ADDR, err);
                break;
            }
        }
    }

    if (!err && (i < size)) {// <1KB data
        err = rwnx_send_dbg_mem_block_write_req(usbdev, AIC_M2D_OTA_DATA_ADDR, size - i, dst + i / 4);
        if (err) {
            printk("m2d upload fail: %x, err:%d\r\n", AIC_M2D_OTA_DATA_ADDR, err);
        }
    }

    if (dst) {
        vfree(dst);
        dst = NULL;
    }
	testmode = FW_NORMAL_MODE;

    printk("m2d flash update complete\n\n");

    return err;
}
#endif//CONFIG_M2D_OTA_AUTO_SUPPORT

int rwnx_plat_flash_bin_upload_android(struct aic_usb_dev *usbdev, u32 fw_addr,
                               char *filename)
{
    struct device *dev = usbdev->dev;
    unsigned int i=0;
    int size;
    u32 *dst=NULL;
    int err=0;
    const u32 mem_addr = fw_addr;
    struct dbg_mem_read_cfm rd_mem_addr_cfm;
    u32 crc = (u32)~0UL;

    /* load aic firmware */
    size = aic_load_firmware(&dst, filename, dev);
    flash_write_size = size;
    if(size<=0){
            printk("wrong size of firmware file\n");
            vfree(dst);
            dst = NULL;
            return ENOENT;
    }

    printk("size %x, flash_erase_len %x\n", size, flash_erase_len);
    if (size != flash_erase_len || (flash_erase_len & 0xFFF)) {
        printk("wrong size of flash_erase_len %d\n", flash_erase_len);
        vfree(dst);
        dst = NULL;
        return -1;
    }

    err = rwnx_send_dbg_mem_read_req(usbdev, mem_addr, &rd_mem_addr_cfm);
    if (err) {
        printk("%x rd fail: %d\n", mem_addr, err);
        return err;
    }

    if (rd_mem_addr_cfm.memdata != 0xffffffff) {
        //erase flash
        if (size > 0x40000) {
            for (i = 0; i < (size - 0x40000); i +=0x40000) {//each time erase 256K
                err = rwnx_send_dbg_mem_mask_write_req(usbdev, fw_addr+i, 0xf150e250, 0x40000);
                if (err) {
                    printk("flash erase fail: %x, err:%d\r\n", fw_addr + i, err);
                    return err;
                }
            }
        }
        if (!err && (i < size)) {// <256KB data
            err = rwnx_send_dbg_mem_mask_write_req(usbdev, fw_addr + i, 0xf150e250, size - i);
            if (err) {
                printk("flash erase fail: %x, err:%d\r\n", fw_addr + i, err);
            }
        }
    }

    /* Copy the file on the Embedded side */
    printk("### Upload %s firmware, @ = %x  size=%d\n", filename, fw_addr, size);
    crc = aic_crc32((u8 *)dst, size, crc);
    flash_write_bin_crc = crc;

    if (size > 1024) {// > 1KB data
        for (i = 0; i < (size - 1024); i += 1024) {//each time write 1KB
            err = rwnx_send_dbg_mem_block_write_req(usbdev, fw_addr + i, 1024, dst + i / 4);
                if (err) {
                printk("bin upload fail: %x, err:%d\r\n", fw_addr + i, err);
                break;
            }
        }
    }

    if (!err && (i < size)) {// <1KB data
        err = rwnx_send_dbg_mem_block_write_req(usbdev, fw_addr + i, size - i, dst + i / 4);
        if (err) {
            printk("bin upload fail: %x, err:%d\r\n", fw_addr + i, err);
        }
    }

    if (dst) {
        vfree(dst);
        dst = NULL;
    }

    printk("fw download complete\n\n");

    return err;
}


uint32_t rwnx_atoli(char *value){
	int len = 0;
	int temp_len = 0;
	int i = 0;
	uint32_t result = 0;
	
	temp_len = strlen(value);

	for(i = 0;i < temp_len; i++){
		if((value[i] >= 48 && value[i] <= 57) ||
			(value[i] >= 65 && value[i] <= 70) ||
			(value[i] >= 97 && value[i] <= 102)){
			len++;
		}
	}

	//printk("%s len:%d \r\n", __func__, len);
	
	for(i = 0; i < len; i++){
		result = result * 16;
		if(value[i] >= 48 && value[i] <= 57){
			result += value[i] - 48;
		}else if(value[i] >= 65 && value[i] <= 70){
			result += (value[i] - 65) + 10;
		}else if(value[i] >= 97 && value[i] <= 102){
			result += (value[i] - 97) + 10;
		}
	}
	
	return result;
}

int8_t rwnx_atoi(char *value){
	int len = 0;
	int i = 0;
	int8_t result = 0;
	int8_t signal = 1;

	len = strlen(value);
	//printk("%s len:%d \r\n", __func__, len);

	for(i = 0;i < len ;i++){
		if(i == 0 && value[0] == '-'){
			signal = -1;
			continue;
		}

		result = result * 10;
		if(value[i] >= 48 && value[i] <= 57){
			result += value[i] - 48;
		}else{
			result = 0;
			break;
		}
	}

	result = result * signal;
	//printk("%s result:%d \r\n", __func__, result);

	return result;
}

void get_fw_path(char* fw_path){
	if (strlen(aic_fw_path) > 0) {
		memcpy(fw_path, aic_fw_path, strlen(aic_fw_path));
	}else{
		memcpy(fw_path, aic_default_fw_path, strlen(aic_default_fw_path));
	}
} 

void set_testmode(int val){
	testmode = val;
}

int get_testmode(void){
	return testmode;
}

int get_hardware_info(void){
	return AIC_HW_INFO;
}

extern int adap_test;
int get_adap_test(void){
    return adap_test;
}

int get_flash_bin_size(void)
{
    return flash_write_size;
}

u32 get_flash_bin_crc(void)
{
    return flash_write_bin_crc;
}

EXPORT_SYMBOL(get_fw_path);

EXPORT_SYMBOL(get_testmode);

EXPORT_SYMBOL(set_testmode);

EXPORT_SYMBOL(get_hardware_info);

EXPORT_SYMBOL(get_adap_test);

EXPORT_SYMBOL(get_flash_bin_size);
EXPORT_SYMBOL(get_flash_bin_crc);


void get_userconfig_xtal_cap(xtal_cap_conf_t *xtal_cap)
{
	xtal_cap->enable = userconfig_xtal_cap.enable;
	xtal_cap->xtal_cap = userconfig_xtal_cap.xtal_cap;
	xtal_cap->xtal_cap_fine = userconfig_xtal_cap.xtal_cap_fine;

    printk("%s:enable       :%d\r\n", __func__, xtal_cap->enable);
    printk("%s:xtal_cap     :%d\r\n", __func__, xtal_cap->xtal_cap);
    printk("%s:xtal_cap_fine:%d\r\n", __func__, xtal_cap->xtal_cap_fine);
}

EXPORT_SYMBOL(get_userconfig_xtal_cap);

void get_userconfig_txpwr_idx(txpwr_idx_conf_t *txpwr_idx){
	txpwr_idx->enable = userconfig_txpwr_idx.enable;
	txpwr_idx->dsss = userconfig_txpwr_idx.dsss;
	txpwr_idx->ofdmlowrate_2g4 = userconfig_txpwr_idx.ofdmlowrate_2g4;
	txpwr_idx->ofdm64qam_2g4 = userconfig_txpwr_idx.ofdm64qam_2g4;
	txpwr_idx->ofdm256qam_2g4 = userconfig_txpwr_idx.ofdm256qam_2g4;
	txpwr_idx->ofdm1024qam_2g4 = userconfig_txpwr_idx.ofdm1024qam_2g4;
	txpwr_idx->ofdmlowrate_5g = userconfig_txpwr_idx.ofdmlowrate_5g;
	txpwr_idx->ofdm64qam_5g = userconfig_txpwr_idx.ofdm64qam_5g;
	txpwr_idx->ofdm256qam_5g = userconfig_txpwr_idx.ofdm256qam_5g;
	txpwr_idx->ofdm1024qam_5g = userconfig_txpwr_idx.ofdm1024qam_5g;

	printk("%s:enable:%d\r\n", __func__, txpwr_idx->enable);
	printk("%s:dsss:%d\r\n", __func__, txpwr_idx->dsss);
	printk("%s:ofdmlowrate_2g4:%d\r\n", __func__, txpwr_idx->ofdmlowrate_2g4);
	printk("%s:ofdm64qam_2g4:%d\r\n", __func__, txpwr_idx->ofdm64qam_2g4);
	printk("%s:ofdm256qam_2g4:%d\r\n", __func__, txpwr_idx->ofdm256qam_2g4);
	printk("%s:ofdm1024qam_2g4:%d\r\n", __func__, txpwr_idx->ofdm1024qam_2g4);
	printk("%s:ofdmlowrate_5g:%d\r\n", __func__, txpwr_idx->ofdmlowrate_5g);
	printk("%s:ofdm64qam_5g:%d\r\n", __func__, txpwr_idx->ofdm64qam_5g);
	printk("%s:ofdm256qam_5g:%d\r\n", __func__, txpwr_idx->ofdm256qam_5g);
	printk("%s:ofdm1024qam_5g:%d\r\n", __func__, txpwr_idx->ofdm1024qam_5g);

}

EXPORT_SYMBOL(get_userconfig_txpwr_idx);

void get_userconfig_txpwr_ofst(txpwr_ofst_conf_t *txpwr_ofst){
	txpwr_ofst->enable = userconfig_txpwr_ofst.enable;
	txpwr_ofst->chan_1_4 = userconfig_txpwr_ofst.chan_1_4;
	txpwr_ofst->chan_5_9 = userconfig_txpwr_ofst.chan_5_9;
	txpwr_ofst->chan_10_13 = userconfig_txpwr_ofst.chan_10_13;
	txpwr_ofst->chan_36_64 = userconfig_txpwr_ofst.chan_36_64;
	txpwr_ofst->chan_100_120 = userconfig_txpwr_ofst.chan_100_120;
	txpwr_ofst->chan_122_140 = userconfig_txpwr_ofst.chan_122_140;
	txpwr_ofst->chan_142_165 = userconfig_txpwr_ofst.chan_142_165;

	printk("%s:ofst_enable:%d\r\n", __func__, txpwr_ofst->enable);
	printk("%s:ofst_chan_1_4:%d\r\n", __func__, txpwr_ofst->chan_1_4);
	printk("%s:ofst_chan_5_9:%d\r\n", __func__, txpwr_ofst->chan_5_9);
	printk("%s:ofst_chan_10_13:%d\r\n", __func__, txpwr_ofst->chan_10_13);
	printk("%s:ofst_chan_36_64:%d\r\n", __func__, txpwr_ofst->chan_36_64);
	printk("%s:ofst_chan_100_120:%d\r\n", __func__, txpwr_ofst->chan_100_120);
	printk("%s:ofst_chan_122_140:%d\r\n", __func__, txpwr_ofst->chan_122_140);
	printk("%s:ofst_chan_142_165:%d\r\n", __func__, txpwr_ofst->chan_142_165);

}

EXPORT_SYMBOL(get_userconfig_txpwr_ofst);

void rwnx_plat_userconfig_set_value(char *command, char *value){	
	//TODO send command
	printk("%s:command=%s value=%s \r\n", __func__, command, value);
	if(!strcmp(command, "enable")){
		userconfig_txpwr_idx.enable = rwnx_atoi(value);
	}else if(!strcmp(command, "dsss")){
		userconfig_txpwr_idx.dsss = rwnx_atoi(value);
	}else if(!strcmp(command, "ofdmlowrate_2g4")){
		userconfig_txpwr_idx.ofdmlowrate_2g4 = rwnx_atoi(value);
	}else if(!strcmp(command, "ofdm64qam_2g4")){
		userconfig_txpwr_idx.ofdm64qam_2g4 = rwnx_atoi(value);
	}else if(!strcmp(command, "ofdm256qam_2g4")){
		userconfig_txpwr_idx.ofdm256qam_2g4 = rwnx_atoi(value);
	}else if(!strcmp(command, "ofdm1024qam_2g4")){
		userconfig_txpwr_idx.ofdm1024qam_2g4 = rwnx_atoi(value);
	}else if(!strcmp(command, "ofdmlowrate_5g")){
		userconfig_txpwr_idx.ofdmlowrate_5g = rwnx_atoi(value);
	}else if(!strcmp(command, "ofdm64qam_5g")){
		userconfig_txpwr_idx.ofdm64qam_5g = rwnx_atoi(value);
	}else if(!strcmp(command, "ofdm256qam_5g")){
		userconfig_txpwr_idx.ofdm256qam_5g = rwnx_atoi(value);
	}else if(!strcmp(command, "ofdm1024qam_5g")){
		userconfig_txpwr_idx.ofdm1024qam_5g = rwnx_atoi(value);
	}else if(!strcmp(command, "ofst_enable")){
		userconfig_txpwr_ofst.enable = rwnx_atoi(value);
	}else if(!strcmp(command, "ofst_chan_1_4")){
		userconfig_txpwr_ofst.chan_1_4 = rwnx_atoi(value);
	}else if(!strcmp(command, "ofst_chan_5_9")){
		userconfig_txpwr_ofst.chan_5_9 = rwnx_atoi(value);
	}else if(!strcmp(command, "ofst_chan_10_13")){
		userconfig_txpwr_ofst.chan_10_13 = rwnx_atoi(value);
	}else if(!strcmp(command, "ofst_chan_36_64")){
		userconfig_txpwr_ofst.chan_36_64 = rwnx_atoi(value);
	}else if(!strcmp(command, "ofst_chan_100_120")){
		userconfig_txpwr_ofst.chan_100_120 = rwnx_atoi(value);
	}else if(!strcmp(command, "ofst_chan_122_140")){
		userconfig_txpwr_ofst.chan_122_140 = rwnx_atoi(value);
	}else if(!strcmp(command, "ofst_chan_142_165")){
		userconfig_txpwr_ofst.chan_142_165 = rwnx_atoi(value);
	}else if(!strcmp(command, "xtal_enable")){
		userconfig_xtal_cap.enable = rwnx_atoi(value);
	}else if(!strcmp(command, "xtal_cap")){
		userconfig_xtal_cap.xtal_cap = rwnx_atoi(value);
	}else if(!strcmp(command, "xtal_cap_fine")){
		userconfig_xtal_cap.xtal_cap_fine = rwnx_atoi(value);
	}
}

void rwnx_plat_userconfig_parsing(char *buffer, int size){
    int i = 0;
	int parse_state = 0;
	char command[30];
	char value[100];
	int char_counter = 0;

	memset(command, 0, 30);
	memset(value, 0, 100);

    for(i = 0; i < size; i++){

		//Send command or print nvram log when char is \r or \n
		if(buffer[i] == 0x0a || buffer[i] == 0x0d){
			if(command[0] != 0 && value[0] != 0){
				if(parse_state == PRINT){
					printk("%s:%s\r\n", __func__, value);
				}else if(parse_state == GET_VALUE){
					rwnx_plat_userconfig_set_value(command, value);
				}
			}
			//Reset command value and char_counter
			memset(command, 0, 30);
			memset(value, 0, 100);
			char_counter = 0;
			parse_state = INIT;
			continue;
		}

		//Switch parser state
		if(parse_state == INIT){
			if(buffer[i] == '#'){
				parse_state = PRINT;
				continue;
			}else if(buffer[i] == 0x0a || buffer[i] == 0x0d){
				parse_state = INIT;
				continue;
			}else{
				parse_state = CMD;
			}
		}

		//Fill data to command and value
		if(parse_state == PRINT){
			command[0] = 0x01;
			value[char_counter] = buffer[i];
			char_counter++;
		}else if(parse_state == CMD){
			if(command[0] != 0 && buffer[i] == '='){
				parse_state = GET_VALUE;
				char_counter = 0;
				continue;
			}
			command[char_counter] = buffer[i];
			char_counter++;
		}else if(parse_state == GET_VALUE){
			value[char_counter] = buffer[i];
			char_counter++;
		}
	}


}

int rwnx_plat_userconfig_upload_android(struct aic_usb_dev *usbdev, char *filename){
    int size;
    u32 *dst=NULL;
    struct device *dev = usbdev->dev;

	printk("userconfig file path:%s \r\n", filename);

    /* load aic firmware */
    size = aic_load_firmware(&dst, filename, dev);
    if(size <= 0){
            printk("wrong size of firmware file\n");
            vfree(dst);
            dst = NULL;
            return 0;
    }

	/* Copy the file on the Embedded side */
    printk("### Upload %s userconfig, size=%d\n", filename, size);

	rwnx_plat_userconfig_parsing((char *)dst, size);

	if (dst) {
        vfree(dst);
        dst = NULL;
    }

	printk("userconfig download complete\n\n");
	return 0;
}



int aicbt_patch_table_free(struct aicbt_patch_table *head)
{
	struct aicbt_patch_table *p = head, *n = NULL;
	while (p) {
		n = p->next;
		vfree(p->name);
		vfree(p->data);
		vfree(p);
		p = n;
	}
	head = NULL;
	return 0;
}

struct aicbt_patch_table *aicbt_patch_table_alloc(struct aic_usb_dev *usbdev,const char *filename)
{
	struct device *dev = usbdev->dev;
	struct aicbt_patch_table *head = NULL;
	struct aicbt_patch_table *new = NULL;
	struct aicbt_patch_table *cur = NULL;
	int size;
	int ret = 0;
	uint8_t *rawdata=NULL;
	uint8_t *p = NULL;

	/* load aic firmware */
	size = aic_load_firmware((u32 **)&rawdata, filename, dev);

	/* Copy the file on the Embedded side */
	printk("### Upload %s fw_patch_table, size=%d\n", filename, size);

	if (size <= 0) {
		printk("wrong size of firmware file\n");
		ret = -1;
		goto err;
	}

	p = rawdata;

	if (memcmp(p, AICBT_PT_TAG, sizeof(AICBT_PT_TAG) < 16 ? sizeof(AICBT_PT_TAG) : 16)) {
		printk("TAG err\n");
		ret = -1;
		goto err;
	}
	p += 16;

	while (p - rawdata < size) {
		//printk("size = %d  p - rawdata = %d \r\n", size, p - rawdata);
		new = (struct aicbt_patch_table *)vmalloc(sizeof(struct aicbt_patch_table));
		memset(new, 0, sizeof(struct aicbt_patch_table));
		if (head == NULL) {
			head = new;
			cur  = new;
		} else {
			cur->next = new;
			cur = cur->next;
		}

		cur->name = (char *)vmalloc(sizeof(char) * 16);
		memset(cur->name, 0, sizeof(char) * 16);
		memcpy(cur->name, p, 16);
		p += 16;

		cur->type = *(uint32_t *)p;
		p += 4;

		cur->len = *(uint32_t *)p;
		p += 4;

		if((cur->type )  >= 1000 || cur->len == 0) {//Temp Workaround
			cur->len = 0;
		}else{
			cur->data = (uint32_t *)vmalloc(sizeof(uint8_t) * cur->len * 8);
			memset(cur->data, 0, sizeof(uint8_t) * cur->len * 8);
			memcpy(cur->data, p, cur->len * 8);
			p += cur->len * 8;
		}
	}

	vfree(rawdata);

	return head;

err:
	aicbt_patch_table_free(head);
	if (rawdata)
		vfree(rawdata);
	return NULL;
}

struct aicbsp_info_t aicbsp_info = {
    .hwinfo   = AICBSP_HWINFO_DEFAULT,
    .cpmode   = AICBSP_CPMODE_DEFAULT,
};



static struct aicbt_info_t aicbt_info[] = {
    {   
        .btmode        = AICBT_BTMODE_DEFAULT,
        .btport        = AICBT_BTPORT_DEFAULT,
        .uart_baud     = AICBT_UART_BAUD_DEFAULT,
        .uart_flowctrl = AICBT_UART_FC_DEFAULT,
        .lpm_enable    = AICBT_LPM_ENABLE_DEFAULT,
        .txpwr_lvl     = AICBT_TXPWR_LVL_DEFAULT,
    },//PRODUCT_ID_AIC8800
    {
    },//PRODUCT_ID_AIC8801
    {
    },//PRODUCT_ID_AIC8800DC
    {
    },//PRODUCT_ID_AIC8800DW
    {
        .btmode        = AICBT_BTMODE_DEFAULT_8800d80,
        .btport        = AICBT_BTPORT_DEFAULT,
        .uart_baud     = AICBT_UART_BAUD_DEFAULT,
        .uart_flowctrl = AICBT_UART_FC_DEFAULT,
        .lpm_enable    = AICBT_LPM_ENABLE_DEFAULT,
        .txpwr_lvl     = AICBT_TXPWR_LVL_DEFAULT_8800d80,
    },//PRODUCT_ID_AIC8800D80
    {
    },//PRODUCT_ID_AIC8800D81
    {
        .btmode        = AICBT_BTMODE_DEFAULT_8800d80x2,
        .btport        = AICBT_BTPORT_DEFAULT,
        .uart_baud     = AICBT_UART_BAUD_DEFAULT,
        .uart_flowctrl = AICBT_UART_FC_DEFAULT,
        .lpm_enable    = AICBT_LPM_ENABLE_DEFAULT,
        .txpwr_lvl     = AICBT_TXPWR_LVL_DEFAULT_8800d80x2,
    },//PRODUCT_ID_AIC8800D80X2
};

#ifdef CONFIG_LOAD_BT_CONF
static const char *aicbt_find_tag(const u8 *file_data, unsigned int file_size,
                                 const char *tag_name, unsigned int tag_len)
{
    unsigned int line_start = 0, tag_name_len = strlen(tag_name);
    const char *comment_symbols = "#;";

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    while (line_start < file_size) {
        unsigned int line_end = line_start;

        while (line_end < file_size && file_data[line_end] != '\n' && file_data[line_end] != '\r') {
            line_end++;
        }

        if (line_end - line_start >= tag_name_len &&
            !strncmp((const char*)&file_data[line_start], tag_name, tag_name_len))
        {
            const char *value_start = (const char*)&file_data[line_start + tag_name_len];
            const char *value_end = (const char*)&file_data[line_end];

            while (value_start < value_end && (*value_start == ' ' || *value_start == '=')) {
                value_start++;
            }
            const char *comment_pos = value_start;
            while (comment_pos < value_end && !strchr(comment_symbols, *comment_pos)) {
                comment_pos++;
            }
            while (comment_pos > value_start && (*(comment_pos-1) == ' ' || *(comment_pos-1) == '\t')) {
                comment_pos--;
            }
            if (comment_pos > value_start) {
                return value_start;
            }
        }

        line_start = line_end;
        while (line_start < file_size && (file_data[line_start] == '\n' || file_data[line_start] == '\r')) {
            line_start++;
        }
    }
    return NULL;
}

void aicbt_parse_config(struct aic_usb_dev *usbdev, const char *filename)
{
    struct device *dev = usbdev->dev;
    u32 *dst = NULL;
    int size;
    const u8 *tag_ptr;
    u32 tmp_val;
    //char *filename = "aicbt.conf";

    RWNX_DBG(RWNX_FN_ENTRY_STR);

    size = aic_load_firmware((u32 **)&dst, filename, dev);
    if (size <= 0) {
        AICWFDBG(LOGERROR, "%s: load %s fail (%d)\n", __func__, filename, size);
        return;
    }

    tag_ptr = aicbt_find_tag((char*)dst, size, "BTMODE=", strlen("0"));
    if (tag_ptr) {
        if (sscanf(tag_ptr, "%x", &aicbt_info[usbdev->chipid].btmode) != 1 ||
            aicbt_info[usbdev->chipid].btmode > AICBT_BTMODE_BT_ONLY_COANT) {
            aicbt_info[usbdev->chipid].btmode = AICBT_BTMODE_DEFAULT;
            AICWFDBG(LOGERROR, "BTMODE invalid, use default %02X\n", AICBT_BTMODE_DEFAULT);
        }
    } else {
        aicbt_info[usbdev->chipid].btmode = AICBT_BTMODE_DEFAULT;
    }

    tag_ptr = aicbt_find_tag((char*)dst, size, "BTPORT=", strlen("0"));
    if (tag_ptr) {
        if (sscanf(tag_ptr, "%x", &aicbt_info[usbdev->chipid].btport) != 1 ||
            aicbt_info[usbdev->chipid].btport > AICBT_BTPORT_UART) {
            aicbt_info[usbdev->chipid].btport = AICBT_BTPORT_DEFAULT;
        }
    } else {
        aicbt_info[usbdev->chipid].btport = AICBT_BTPORT_DEFAULT;
    }

    tag_ptr = aicbt_find_tag((char*)dst, size, "UART_BAUD=", 0);
    if (tag_ptr) {
        if (sscanf(tag_ptr, "%u", &tmp_val) == 1) {
            if(tmp_val >= AICBT_UART_BAUD_115200 && tmp_val <= AICBT_UART_BAUD_3_25M) {
                aicbt_info[usbdev->chipid].uart_baud = tmp_val;
            } else {
                aicbt_info[usbdev->chipid].uart_baud = AICBT_UART_BAUD_DEFAULT;
                AICWFDBG(LOGERROR, "UART_BAUD %u invalid, use default %d\n", tmp_val, AICBT_UART_BAUD_DEFAULT);
            }
        } else {
            aicbt_info[usbdev->chipid].uart_baud = AICBT_UART_BAUD_DEFAULT;
        }
    } else {
        aicbt_info[usbdev->chipid].uart_baud = AICBT_UART_BAUD_DEFAULT;
    }
    tag_ptr = aicbt_find_tag((char*)dst, size, "UART_FC=", strlen("0"));
    if (tag_ptr) {
        if (sscanf(tag_ptr, "%x", &aicbt_info[usbdev->chipid].uart_flowctrl) != 1 ||
            aicbt_info[usbdev->chipid].uart_flowctrl > AICBT_UART_FLOWCTRL_ENABLE) {
            aicbt_info[usbdev->chipid].uart_flowctrl = AICBT_UART_FC_DEFAULT;
        }
    } else {
        aicbt_info[usbdev->chipid].uart_flowctrl = AICBT_UART_FC_DEFAULT;
    }

    tag_ptr = aicbt_find_tag((char*)dst, size, "LPM_ENABLE=", strlen("0"));
    if (tag_ptr) {
        if (sscanf(tag_ptr, "%x", &aicbt_info[usbdev->chipid].lpm_enable) != 1 ||
            aicbt_info[usbdev->chipid].lpm_enable > 1) {
            aicbt_info[usbdev->chipid].lpm_enable = AICBT_LPM_ENABLE_DEFAULT;
        }
    } else {
        aicbt_info[usbdev->chipid].lpm_enable = AICBT_LPM_ENABLE_DEFAULT;
    }
    tag_ptr = aicbt_find_tag((char*)dst, size, "TXPWR_LVL=", strlen("0x6F2F"));
    if (tag_ptr) {
        if (sscanf(tag_ptr, "%08x", &tmp_val) == 1) {
            if (tmp_val >= 0 || tmp_val <= 0X7F7F) {
                aicbt_info[usbdev->chipid].txpwr_lvl = tmp_val;
            } else {
                aicbt_info[usbdev->chipid].txpwr_lvl = AICBT_TXPWR_LVL_DEFAULT;
            }
        } else {
            aicbt_info[usbdev->chipid].txpwr_lvl = AICBT_TXPWR_LVL_DEFAULT;
        }
    } else {
        aicbt_info[usbdev->chipid].txpwr_lvl = AICBT_TXPWR_LVL_DEFAULT;
    }
    vfree(dst);
    AICWFDBG(LOGINFO, "%s: btmode %d btport %d uart baud %d uart fc %d lpm %d txpwrlvl %4X\n",
        __func__,
        aicbt_info[usbdev->chipid].btmode,
        aicbt_info[usbdev->chipid].btport,
        aicbt_info[usbdev->chipid].uart_baud,
        aicbt_info[usbdev->chipid].uart_flowctrl,
        aicbt_info[usbdev->chipid].lpm_enable,
        aicbt_info[usbdev->chipid].txpwr_lvl);
}
#endif

int aicbt_patch_table_load(struct aic_usb_dev *usbdev, struct aicbt_patch_table *_head)
{
	struct aicbt_patch_table *head, *p;
	int ret = 0, i;
	uint32_t *data = NULL;

	head = _head;

	for (p = head; p != NULL; p = p->next) {
		data = p->data;
		if(AICBT_PT_BTMODE == p->type){
			*(data + 1)  = aicbsp_info.hwinfo < 0;
			*(data + 3) = aicbsp_info.hwinfo;
			*(data + 5)  = aicbsp_info.cpmode;

			*(data + 7) = aicbt_info[usbdev->chipid].btmode;
			*(data + 9) = aicbt_info[usbdev->chipid].btport;
			*(data + 11) = aicbt_info[usbdev->chipid].uart_baud;
			*(data + 13) = aicbt_info[usbdev->chipid].uart_flowctrl;
			*(data + 15) = aicbt_info[usbdev->chipid].lpm_enable;
			*(data + 17) = aicbt_info[usbdev->chipid].txpwr_lvl;

			printk("%s bt btmode[%d]:%d\r\n", __func__, usbdev->chipid, aicbt_info[usbdev->chipid].btmode);
			printk("%s bt btport[%d]:%d\r\n", __func__, usbdev->chipid, aicbt_info[usbdev->chipid].btport);
			printk("%s bt uart_baud[%d]:%d\r\n", __func__, usbdev->chipid, aicbt_info[usbdev->chipid].uart_baud);
			printk("%s bt uart_flowctrl[%d]:%d\r\n", __func__, usbdev->chipid, aicbt_info[usbdev->chipid].uart_flowctrl);
			printk("%s bt lpm_enable[%d]:%d\r\n", __func__, usbdev->chipid, aicbt_info[usbdev->chipid].lpm_enable);
			printk("%s bt tx_pwr[%d]:%4X\r\n", __func__, usbdev->chipid, aicbt_info[usbdev->chipid].txpwr_lvl);

		}
		if (p->type == 0x06) {
			char *data_s = (char *)p->data;
			printk("patch version %s\n", data_s);
			continue;
		}
		for (i = 0; i < p->len; i++) {
			ret = rwnx_send_dbg_mem_write_req(usbdev, *data, *(data + 1));
			if (ret != 0)
				return ret;
			data += 2;
		}
		if (p->type == AICBT_PT_PWRON)
			mdelay(100);
		//	udelay(500);
	}
	aicbt_patch_table_free(head);
	return 0;
}

int aicbt_patch_info_unpack(struct aicbt_patch_info_t *patch_info, struct aicbt_patch_table *head_t)
{
    uint8_t *patch_info_array = (uint8_t*)patch_info;
    int base_len = 0;
    int memcpy_len = 0;
    
    if (AICBT_PT_INF == head_t->type) {
        base_len = ((offsetof(struct aicbt_patch_info_t,  ext_patch_nb_addr) - offsetof(struct aicbt_patch_info_t,  adid_addrinf) )/sizeof(uint32_t))/2;
        AICWFDBG(LOGDEBUG, "%s head_t->len:%d base_len:%d \r\n", __func__, head_t->len, base_len);

        if (head_t->len > base_len){
            patch_info->info_len = base_len;
            memcpy_len = patch_info->info_len + 1;//include ext patch nb     
        } else{
            patch_info->info_len = head_t->len;
            memcpy_len = patch_info->info_len;
        }
	head_t->len = patch_info->info_len;
        AICWFDBG(LOGDEBUG, "%s memcpy_len:%d \r\n", __func__, memcpy_len);   

        if (patch_info->info_len == 0)
            return 0;
       
        memcpy(((patch_info_array) + sizeof(patch_info->info_len)), 
            head_t->data, 
            memcpy_len * sizeof(uint32_t) * 2);
        AICWFDBG(LOGDEBUG, "%s adid_addrinf:%x addr_adid:%x \r\n", __func__, 
            ((struct aicbt_patch_info_t *)patch_info_array)->adid_addrinf,
            ((struct aicbt_patch_info_t *)patch_info_array)->addr_adid);

        if (patch_info->ext_patch_nb > 0){
            int index = 0;
            patch_info->ext_patch_param = (uint32_t *)(head_t->data + ((memcpy_len) * 2));
            
            for(index = 0; index < patch_info->ext_patch_nb; index++){
                AICWFDBG(LOGDEBUG, "%s id:%x addr:%x \r\n", __func__, 
                    *(patch_info->ext_patch_param + (index * 2)),
                    *(patch_info->ext_patch_param + (index * 2) + 1));
            }
        }

    }
    return 0;

}

int rwnx_plat_bin_fw_patch_table_upload_android(struct aic_usb_dev *usbdev, char *filename){
    struct device *dev = usbdev->dev;
	struct aicbt_patch_table *head = NULL;
	struct aicbt_patch_table *new = NULL;
	struct aicbt_patch_table *cur = NULL;
   	 int size;
	int ret = 0;
   	uint8_t *rawdata=NULL;
	uint8_t *p = NULL;

    /* load aic firmware */
    size = aic_load_firmware((u32 **)&rawdata, filename, dev);

	/* Copy the file on the Embedded side */
    printk("### Upload %s fw_patch_table, size=%d\n", filename, size);

	if (size <= 0) {
		printk("wrong size of firmware file\n");
		ret = -1;
		goto err;
	}

	p = rawdata;

	if (memcmp(p, AICBT_PT_TAG, sizeof(AICBT_PT_TAG) < 16 ? sizeof(AICBT_PT_TAG) : 16)) {
		printk("TAG err\n");
		ret = -1;
		goto err;
	}
	p += 16;

	while (p - rawdata < size) {
		//printk("size = %d  p - rawdata = %d \r\n", size, p - rawdata);
		new = (struct aicbt_patch_table *)vmalloc(sizeof(struct aicbt_patch_table));
		memset(new, 0, sizeof(struct aicbt_patch_table));
		if (head == NULL) {
			head = new;
			cur  = new;
		} else {
			cur->next = new;
			cur = cur->next;
		}

		cur->name = (char *)vmalloc(sizeof(char) * 16);
		memset(cur->name, 0, sizeof(char) * 16);
		memcpy(cur->name, p, 16);
		p += 16;

		cur->type = *(uint32_t *)p;
		p += 4;

		cur->len = *(uint32_t *)p;
		p += 4;

		if((cur->type )  >= 1000 || cur->len == 0) {//Temp Workaround
			cur->len = 0;
		}else{
			cur->data = (uint32_t *)vmalloc(sizeof(uint8_t) * cur->len * 8);
			memset(cur->data, 0, sizeof(uint8_t) * cur->len * 8);
			memcpy(cur->data, p, cur->len * 8);
			p += cur->len * 8;
		}
	}

	vfree(rawdata);
	aicbt_patch_table_load(usbdev, head);
	printk("fw_patch_table download complete\n\n");

	return ret;
err:
	//aicbt_patch_table_free(&head);

	if (rawdata){
		vfree(rawdata);
	}
	return ret;
}


