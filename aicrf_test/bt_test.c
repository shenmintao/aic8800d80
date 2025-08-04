#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <pthread.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ctype.h>

#define RELEASE_DATE "2024_0701"

//tool role
#define SERVER 0
#define CLIENT 1
#define CLIENT_DIRECT 2
#define WLAN_CLIENT 3

//interface
#define UART    0
#define USB     1
#define WLAN    2

//uart param
#define BAUDRATE 1500000
#define DATABITS 8
#define PARTY 'N'
#define STOP '1'
#define SOFTWARE_HANDSHAKE 0
#define HARDWARE_HANDSHAKE 1

//socket param
#define BT_HCI_TOOL_PORT 5001
#define SOCKER_BUFFER_SIZE 1024

#define OK 0
#define ERR -1
#define FALSE 0
#define TRUE 1

#define DOWN_FW_CFG             _IOW('E', 176, int)
#define GET_USB_INFO            _IOR('E', 180, int)

#define WLAN_CMD_MAX_CNT        256
#define TXRX_PARA								SIOCDEVPRIVATE+1

#define ARG_IS(_str) (!strcmp(argv[command_counter], _str))
#define OS_STRCMP(str, _str) (!strcmp(str, _str))

int server_or_client = SERVER;
int interface = USB;
int bt_dev_fd = 0;

char s_baudrate[8];

char* s_hci_command[256];
uint8_t r_hci_command[256];
int r_hci_command_len;
char w_ifname[64];
char *w_command[WLAN_CMD_MAX_CNT];
char hci_event[256];

char dev_path[] = "/dev/aicbt_dev";

char hci_reset[] = { 0x01, 0x03, 0x0c, 0x00 };
char set_filter[] = { 0x01, 0x05, 0x0c, 0x03, 0x02, 0x00, 0x02 };
char scan_on[] = { 0x01, 0x1a, 0x0c, 0x01, 0x03 };
char dut_en[] = { 0x01, 0x03, 0x18, 0x00 };

typedef struct android_wifi_priv_cmd {
	char *buf;
	int used_len;
	int total_len;
} android_wifi_priv_cmd;

enum{
	maxLth_DH1=27,
	maxLth_DH3=138,
	maxLth_DH5=339,
	maxLth_2DH1=54,
	maxLth_2DH3=367,
	maxLth_2DH5=679,
	maxLth_3DH1=83,
	maxLth_3DH3=552,
	maxLth_3DH5=1021,
};

enum{
	_dh1,
	_dh3,
	_dh5,
	_2dh1,
	_2dh3,
	_2dh5,
	_3dh1,
	_3dh3,
	_3dh5,
};

typedef enum{
	bt_start,
	bt_stop,
	ble_start,
	ble_stop,
	tx_tone,
	tx_tonestop,
	bt_reset,
	dut,
	dut_dis_scan,
	get_txpwr,
	set_txpwr,
	set_pwrofst,
} cmd_t;

typedef enum {
	tx,
	rx,
} trx_mode_t;

struct bt_test_args_t{	
	trx_mode_t trx_mode;
	cmd_t cmd;
	int type;
	int pattern;
	int hopping;
	int channel;
	uint8_t is_edr;
	int whitening;
	int tx_pwr;
	int len;
	uint8_t addr[6];
	int le_phy;
	int mod_idx;
	int8_t tx_pwrofst;
}bt_test_args={
	.pattern = 0x00,
	.whitening = 0x01,
	.addr = { 0x0A, 0x1C, 0x6B, 0xC6, 0x96, 0x7E },
	.le_phy = 0x01,
	.tx_pwr = 0x7f,
	.tx_pwrofst = 0,
};

#if 0
#define isblank(c)		((c) == ' ' || (c) == '\t')
#define isascii(c)		(((unsigned char)(c)) <= 0x7F)

static int isdigit(unsigned char c)
{
	return ((c >= '0') && (c <='9'));
}

static int isxdigit(unsigned char c)
{
	if ((c >= '0') && (c <='9'))
		return 1;
	if ((c >= 'a') && (c <='f'))
		return 1;
	if ((c >= 'A') && (c <='F'))
		return 1;
	return 0;
}

static int islower(unsigned char c)
{
	return ((c >= 'a') && (c <='z'));
}

static unsigned char toupper(unsigned char c)
{
	if (islower(c))
		c -= 'a'-'A';
	return c;
}
#endif

void helper() {
	printf(
		"\tUsage:\n"
		"\t\t-s <interface> <params>\n"
		"\t\t\tSelect the tool service:\n"

		"\t\t\t\tex. \"bt_test -s uart 1500000 /dev/ttyS0\" or \"bt_test -s usb\" or \"bt_test -s wlan wlan0\"\n"

		"\t\t-c <hci_cmd> Send an HCI command to the selected interface\n"
		"\t\t-v Display Release Date\n"
		"\n"
		"\tCommands:\n"
		"\t\tbt_start [<trx> <chan> <pkt> ...] = Start Bluetooth tx test\n"
		"\t\t\t<trx>      (man) = tx | rx\n"
		"\t\t\t<chan>     (man) = 0 ~ 78 (hopen=off) | 255 (hopen=on)\n"
		"\t\t\t<pkt>      (man) = dh1 | dh3 | dh5 | 2dh1 | 2dh3 | 2dh5 | 3dh1 | 3dh3 | 3dh5\n"
		"\t\t\t<pattern>  (opt) = pn9 (default) | pn15 | h00 | hff | hf0 | h0f | haa | h55\n"
		"\t\t\t<len>      (opt) = dec value (default: pkt's max length)\n"
		"\t\t\t<whiten>   (opt) = on | off (default)\n"
		"\t\t\t<addr>     (opt) = 0a 1c 6b c6 96 7e (default)\n"

		"\t\tbt_stop [<trx>] = Stop Bluetooth tx test\n"
		"\t\t\t<trx>      (man) = tx | rx\n"

 		"\t\tble_start [<trx> <chan> <pkt> ...] = Start BLE tx test\n"
		"\t\t\t<trx>      (man) = tx | rx\n"
		"\t\t\t<chan>     (man) = 0 ~ 39 (hopen=off) | 255 (hopen=on)\n"
		"\t\t\t<pkt>      (man) = 1m | 2m | s2 | s8\n"
		"\t\t\t<pattern>  (opt) = pn9 (default) | pn15 | h00 | hff | hf0 | h0f | haa | h55\n"
		"\t\t\t<len>      (opt) = dec value (default: 255)\n"

		"\t\tble_stop [<trx>] = Stop BLE tx test\n"
		"\t\t\t<trx>      (man) = tx | rx\n"

		"\t\ttx_tone [<chan> ...] = Start a Single-Carrier Bluetooth test\n"
		"\t\t\t<chan>     (man) = 0 ~ 78\n"
		"\t\t\t<pwr>      (opt) = 0 ~ 7 (default: 0x07)\n"
		"\t\ttx_tonestop = Stop Bluetooth Single-Carrier Test\n"

		"\t\tget_txpwr = Get  tx power\n"
		"\t\tset_txpwr [<pwr>] = Set tx power\n"
		"\t\t\t<pwr>	  (man) = 0 ~ 0x6f (default: 0x6f)\n"

		"\t\tbt_reset = reset BT\n"
		"\t\tbt_dut <1/0> = bt_dut /bt_dut 1 (dut en & scan on) | bt_dut 0 (scan off)\n"
		"\t\tset_pwrofst [<pwrofst>]= Set bt tx power offset\n"
		"\t\t\t<pwrofst>      (man) = -7 ~ 7\n"
	);
}


unsigned int command_strtoul(const char *cp, char **endp, unsigned int base)
{
	unsigned int result = 0, value, is_neg = 0;

	if (*cp == '0') {
		cp++;
		if ((*cp == 'x') && isxdigit(cp[1])) {
			base = 16;
			cp++;
		}
		if (!base) {
			base = 8;
		}
	}
	if (!base) {
		base = 10;
	}
	if (*cp == '-') {
		is_neg = 1;
		cp++;
	}
	while (isxdigit(*cp) && (value = isdigit(*cp) ? *cp - '0' : (islower(*cp) ? toupper(*cp) : *cp) - 'A' + 10) < base) {
		result = result * base + value;
		cp++;
	}
	if (is_neg)
		result = (unsigned int)((int)result * (-1));

	if (endp)
		*endp = (char *)cp;
	return result;
}

int parse_cmd_line(int argc, char **argv){
	int command_number = argc;
	int command_counter = 1;
	int hci_command_counter = 0;
	//int error_command = 0;
	int channel = 0;
	//int len = 0 ;

	while(command_counter < command_number){
		if(!strcmp(argv[command_counter], "-s")){
			if(!(command_number >= 3)){
				printf("error command \r\n");
				return ERR;
			}
			printf("bt_hci_tool server setting\r\n");
			server_or_client = SERVER;
			command_counter++;
			if(!strcmp(argv[command_counter], "usb")){
				printf("use USB\r\n");
				interface = USB;
				return OK;
			}else if(!strcmp(argv[command_counter], "uart")){
				interface = UART;
				if(command_number != 5){
					printf("error command \r\n");
					return ERR;
				}
				command_counter++;
				memcpy(s_baudrate, argv[command_counter], strlen(argv[command_counter]));
				command_counter++;
				memset(dev_path, 0, strlen(dev_path));
				memcpy(dev_path, argv[command_counter], strlen(argv[command_counter]));
				printf("use UART %d %s\r\n", atoi(s_baudrate), dev_path);
				return OK;
			}else if(!strcmp(argv[command_counter], "wlan")){
				interface = WLAN;
				if(command_number != 4){
					printf("error command \r\n");
					return ERR;
				}
				command_counter++;
				memcpy(w_ifname, argv[command_counter], strlen(argv[command_counter]));
				printf("use WLAN %s\r\n", w_ifname);
				return OK;
			}else{
				printf("interface set error\r\n");
				return ERR;
			}
		}else if(!strcmp(argv[command_counter], "-c")){
			server_or_client = CLIENT;
			command_counter++;
			hci_command_counter = 0;
			while(command_counter < command_number){
				s_hci_command[hci_command_counter++] = argv[command_counter];
				command_counter++;
			}
			return OK;
		} else if (!strcmp(argv[command_counter], "-w")) {
			int w_command_counter = 0;
			server_or_client = WLAN_CLIENT;
			if (command_number >= WLAN_CMD_MAX_CNT) {
				printf("too much args\r\n");
				return ERR;
			}
			command_counter++;
			while (command_counter < command_number) {
				w_command[w_command_counter++] = argv[command_counter];
				command_counter++;
			}
			return OK;
		}else if (ARG_IS("-m")){
			// reserved for a variety of chip models
		} else if (ARG_IS("bt_start")){
			server_or_client = CLIENT_DIRECT;
			bt_test_args.cmd = bt_start;
			if (command_number < 5) {
				printf("wrong args\r\n");
				return ERR;
			}
			//parse trx
			if (OS_STRCMP(argv[2], "tx")) {
				bt_test_args.trx_mode = tx;
			} else if (OS_STRCMP(argv[2], "rx")) {
				bt_test_args.trx_mode = rx;
			} else {
				printf("no find tx or rx\r\n");
				return ERR;
			}

			//parse chan and hopping
			channel = command_strtoul(argv[3], NULL, 10);
			if (channel < 0 || (channel > 78 && channel != 255)) {
				printf("wrong channel\r\n");
				return ERR;
			} else {
				bt_test_args.channel = channel;
				if (channel == 255)
					bt_test_args.hopping = TRUE;
				else
					bt_test_args.hopping = FALSE;
			}
			//parse type
			if (OS_STRCMP(argv[4], "dh1")){
				bt_test_args.type = _dh1;
				bt_test_args.len = maxLth_DH1;
				bt_test_args.is_edr = FALSE;
			} else if (OS_STRCMP(argv[4], "dh3")){
				bt_test_args.type = _dh3;
				bt_test_args.len = maxLth_DH3;
				bt_test_args.is_edr = FALSE;
			} else if (OS_STRCMP(argv[4], "dh5")){
				bt_test_args.type = _dh5;
				bt_test_args.len = maxLth_DH5;
				bt_test_args.is_edr = FALSE;
			} else if (OS_STRCMP(argv[4], "2dh1")){
				bt_test_args.type = _2dh1;
				bt_test_args.len = maxLth_2DH1;
				bt_test_args.is_edr = TRUE;
			} else if (OS_STRCMP(argv[4], "2dh3")){
				bt_test_args.type = _2dh3;
				bt_test_args.len = maxLth_2DH3;
				bt_test_args.is_edr = TRUE;
			} else if (OS_STRCMP(argv[4], "2dh5")){
				bt_test_args.type = _2dh5;
				bt_test_args.len = maxLth_2DH5;
				bt_test_args.is_edr = TRUE;
			} else if (OS_STRCMP(argv[4], "3dh1")){
				bt_test_args.type = _3dh1;
				bt_test_args.len = maxLth_3DH1;
				bt_test_args.is_edr = TRUE;
			} else if (OS_STRCMP(argv[4], "3dh3")){
				bt_test_args.type = _3dh3;
				bt_test_args.len = maxLth_3DH3;
				bt_test_args.is_edr = TRUE;
			} else if (OS_STRCMP(argv[4], "3dh5")){
				bt_test_args.type = _3dh5;
				bt_test_args.len = maxLth_3DH5;
				bt_test_args.is_edr = TRUE;
			} else {
				printf("wrong type\r\n");
				return ERR;
			}
			//parse pattern
			if (command_number > 5) {
				if (OS_STRCMP(argv[5], "pn9")){
					bt_test_args.pattern =  0x00;
				} else if (OS_STRCMP(argv[5], "hf0")){
					bt_test_args.pattern = 0x01;
				} else if (OS_STRCMP(argv[5], "haa")){
					bt_test_args.pattern = 0x02;
				} else if (OS_STRCMP(argv[5], "pn15")){
					bt_test_args.pattern = 0x03;
				} else if (OS_STRCMP(argv[5], "hff")){
					bt_test_args.pattern = 0x04;
				} else if (OS_STRCMP(argv[5], "h00")){
					bt_test_args.pattern = 0x05;
				} else if (OS_STRCMP(argv[5], "h0f")){
					bt_test_args.pattern = 0x06;
				} else if (OS_STRCMP(argv[5], "h55")){
					bt_test_args.pattern = 0x07;
				} else {
					printf("wrong pattern type\r\n");
					return ERR;
				}

			//parse pkt len
			if (command_number > 6) {
				bt_test_args.len = command_strtoul(argv[6], NULL, 10);
			}

			}
			//parse whitening
			if (command_number > 7) {
				if (OS_STRCMP(argv[7], "on")){
					bt_test_args.whitening =  0x00;
				} else if (OS_STRCMP(argv[7], "off")){
					bt_test_args.whitening = 0x01;
				} else {
					printf("wrong whitening args, use on/off\r\n");
					return ERR;
				}
			}
			//parse mac addr
			if (command_number > 8) {
				if (command_number < 14) {
					printf("wrong mac addr eg: 0a 1c 6b c6 96 7e\r\n");
					return ERR;
				}
			bt_test_args.addr[5] = command_strtoul(argv[8], NULL, 16);
			bt_test_args.addr[4] = command_strtoul(argv[9], NULL, 16);
			bt_test_args.addr[3] = command_strtoul(argv[10], NULL, 16);
			bt_test_args.addr[2] = command_strtoul(argv[11], NULL, 16);
			bt_test_args.addr[1] = command_strtoul(argv[12], NULL, 16);
			bt_test_args.addr[0] = command_strtoul(argv[13], NULL, 16);
			}
			return OK;
		} else if (ARG_IS("bt_stop")){
			server_or_client = CLIENT_DIRECT;
			bt_test_args.cmd = bt_stop;
			if (command_number < 3) {
				printf("wrong args\r\n");
				return ERR;
			}
			//parse trx
			if (OS_STRCMP(argv[2], "tx")) {
				bt_test_args.trx_mode = tx;
			} else if (OS_STRCMP(argv[2], "rx")) {
				bt_test_args.trx_mode = rx;
			} else {
				printf("no find tx or rx\r\n");
				return ERR;
			}
			return OK;
		} else if (ARG_IS("ble_start")){
			server_or_client = CLIENT_DIRECT;
			bt_test_args.cmd = ble_start;
			if (command_number < 5) {
				printf("wrong args\r\n");
				return ERR;
			}
			//parse trx
			if (OS_STRCMP(argv[2], "tx")) {
				bt_test_args.trx_mode = tx;
			} else if (OS_STRCMP(argv[2], "rx")) {
				bt_test_args.trx_mode = rx;
				bt_test_args.mod_idx = 0;//modulation index fix to 0
			} else {
				printf("no find tx or rx\r\n");
				return ERR;
			}

			//parse chan and hopping
			channel = command_strtoul(argv[3], NULL, 10);
			if (channel < 0 || (channel > 39 && channel != 255)) {
				printf("wrong channel\r\n");
				return ERR;
			} else {
				bt_test_args.channel = channel;
				if (channel == 255)
					bt_test_args.hopping = TRUE;
				else
					bt_test_args.hopping = FALSE;
			}
			//parse le phy
			if (OS_STRCMP(argv[4], "1m")){
				bt_test_args.le_phy = 0x01;
			} else if (OS_STRCMP(argv[4], "2m")){
				bt_test_args.le_phy = 0x02;
			} else if (OS_STRCMP(argv[4], "s8")){
				bt_test_args.le_phy = 0x03;
			} else if (OS_STRCMP(argv[4], "s2")){
				bt_test_args.le_phy = 0x04;
			} else {
				printf("wrong type\r\n");
				return ERR;
			}
			//parse pattern
			if (command_number > 5) {
				if (OS_STRCMP(argv[5], "pn9")){
					bt_test_args.pattern =  0x00;
				} else if (OS_STRCMP(argv[5], "hf0")){
					bt_test_args.pattern = 0x01;
				} else if (OS_STRCMP(argv[5], "haa")){
					bt_test_args.pattern = 0x02;
				} else if (OS_STRCMP(argv[5], "pn15")){
					bt_test_args.pattern = 0x03;
				} else if (OS_STRCMP(argv[5], "hff")){
					bt_test_args.pattern = 0x04;
				} else if (OS_STRCMP(argv[5], "h00")){
					bt_test_args.pattern = 0x05;
				} else if (OS_STRCMP(argv[5], "h0f")){
					bt_test_args.pattern = 0x06;
				} else if (OS_STRCMP(argv[5], "h55")){
					bt_test_args.pattern = 0x07;
				} else {
					printf("wrong pattern type\r\n");
					return ERR;
				}
			}
			//parse pkt len
			bt_test_args.len = 255;
			if (command_number > 6) {
				bt_test_args.len = command_strtoul(argv[6], NULL, 10);
				if (bt_test_args.len < 1 || bt_test_args.len > 255) {
					printf("len limit 1~255 (default: 255)\r\n");
					}
			}

			return OK;
		} else if (ARG_IS("ble_stop")){
			server_or_client = CLIENT_DIRECT;
			bt_test_args.cmd = ble_stop;
			if (command_number < 3) {
				printf("wrong args\r\n");
				return ERR;
			}
			//parse trx
			if (OS_STRCMP(argv[2], "tx")) {
				bt_test_args.trx_mode = tx;
			} else if (OS_STRCMP(argv[2], "rx")) {
				bt_test_args.trx_mode = rx;
			} else {
				printf("no find tx or rx\r\n");
				return ERR;
			}
			return OK;
		} else if (ARG_IS("tx_tone")){
			server_or_client = CLIENT_DIRECT;
			bt_test_args.cmd = tx_tone;
			if (command_number < 3) {
				printf("wrong args\r\n");
				return ERR;
			}
			//parse chan
			channel = command_strtoul(argv[2], NULL, 10);
			if (channel < 0 || (channel > 78)) {
				printf("wrong channel\r\n");
				return ERR;
			} else {
				bt_test_args.channel = channel;
			}
			//parse tx pwr
			bt_test_args.tx_pwr = 7;
			if (command_number > 3) {
				bt_test_args.tx_pwr = command_strtoul(argv[3], NULL, 16);
				if (bt_test_args.tx_pwr < 0 || bt_test_args.tx_pwr > 7) {
					printf("pwr limit 0~7 (default: 07)\r\n");
					bt_test_args.tx_pwr = 7;
				}
			}
			return OK;
		} else if (ARG_IS("tx_tonestop")){
			server_or_client = CLIENT_DIRECT;
			bt_test_args.cmd = tx_tonestop;
			return OK;
		} else if (ARG_IS("bt_reset")){
			server_or_client = CLIENT_DIRECT;
			bt_test_args.cmd = bt_reset;
			return OK;
		} else if (ARG_IS("bt_dut")){
			server_or_client = CLIENT_DIRECT;
			if (command_number > 2 && !(command_strtoul(argv[2], NULL, 10)))
				bt_test_args.cmd = dut_dis_scan;
			else
				bt_test_args.cmd = dut;
			return OK;
		} else if (ARG_IS("get_txpwr")){
			server_or_client = CLIENT_DIRECT;
			bt_test_args.cmd = get_txpwr;
			return OK;
		} else if (ARG_IS("set_txpwr")){
			server_or_client = CLIENT_DIRECT;
			bt_test_args.cmd = set_txpwr;
			if (command_number < 2) {
				printf("wrong args\r\n");
				return ERR;
			}
			//parse tx pwr
			bt_test_args.tx_pwr = 0x6f;
			if (command_number > 2) {
				bt_test_args.tx_pwr = command_strtoul(argv[2], NULL, 16);
				if (bt_test_args.tx_pwr < 0 || bt_test_args.tx_pwr > 0x6f) {
					printf("pwr limit 0~0x6f (default: 0x6f)\r\n");
					bt_test_args.tx_pwr = 0x6f;
				}
			}
			return OK;
		} else if (ARG_IS("set_pwrofst")){
			server_or_client = CLIENT_DIRECT;
			bt_test_args.cmd = set_pwrofst;
			if (command_number < 2) {
				printf("wrong args\r\n");
				return ERR;
			}
			//parse tx pwrofst
			bt_test_args.tx_pwrofst = 0x0;
			if (command_number > 2) {
				bt_test_args.tx_pwrofst = command_strtoul(argv[2], NULL, 10);
				printf("txpwrofst %d\n", bt_test_args.tx_pwrofst);
				if (bt_test_args.tx_pwrofst < -7 || bt_test_args.tx_pwrofst > 7) {
					printf("pwrofst limit -7 ~ 7\r\n");
					if (bt_test_args.tx_pwrofst > 7) {
						bt_test_args.tx_pwrofst = 7;
					} else if (bt_test_args.tx_pwrofst < -7) {
						bt_test_args.tx_pwrofst = -7;
					}
				}
			}
			return OK;
		} else if (ARG_IS("-v")){
			printf("bt_test %s\n", RELEASE_DATE);
			return ERR;
		} else {
			if (strcmp(argv[1], "-h") != 0) {
				printf("unknow command %s \r\n", argv[command_counter]);
			}
			helper();
			return ERR;
		}

		command_counter++;
	}

	helper();
	return ERR;

}

void uart_set_options(int baudrate, int databits,
    char parity, char stop, int softwareHandshake, int hardwareHandshake)
{
    struct termios newtio;
    if (tcgetattr(bt_dev_fd, &newtio)!=0)
    {
    printf("tcgetattr() 3 failed \n");
    }

    speed_t _baud=0;
    printf("set baudrate:%d\n", baudrate);

    switch (baudrate)
    {
    #ifdef B0
    case      0: _baud=B0;     break;
    #endif
    #ifdef B50
    case     50: _baud=B50;    break;
    #endif
    #ifdef B75
    case     75: _baud=B75;    break;
    #endif
    #ifdef B110
    case    110: _baud=B110;   break;
    #endif
    #ifdef B134
    case    134: _baud=B134;   break;
    #endif
    #ifdef B150
    case    150: _baud=B150;   break;
    #endif
    #ifdef B200
    case    200: _baud=B200;   break;
    #endif
    #ifdef B300
    case    300: _baud=B300;   break;
    #endif
    #ifdef B600
    case    600: _baud=B600;   break;
    #endif
    #ifdef B1200
    case   1200: _baud=B1200;  break;
    #endif
    #ifdef B1800
    case   1800: _baud=B1800;  break;
    #endif
    #ifdef B2400
    case   2400: _baud=B2400;  break;
    #endif
    #ifdef B4800
    case   4800: _baud=B4800;  break;
    #endif
    #ifdef B7200
    case   7200: _baud=B7200;  break;
    #endif
    #ifdef B9600
    case   9600: _baud=B9600;  break;
    #endif
    #ifdef B14400
    case  14400: _baud=B14400; break;
    #endif
    #ifdef B19200
    case  19200: _baud=B19200; break;
    #endif
    #ifdef B28800
    case  28800: _baud=B28800; break;
    #endif
    #ifdef B38400
    case  38400: _baud=B38400; break;
    #endif
    #ifdef B57600
    case  57600: _baud=B57600; break;
    #endif
    #ifdef B76800
    case  76800: _baud=B76800; break;
    #endif
    #ifdef B115200
    case 115200: _baud=B115200; break;
    #endif
    #ifdef B128000
    case 128000: _baud=B128000; break;
    #endif
    #ifdef B230400
    case 230400: _baud=B230400; break;
    #endif
    #ifdef B460800
    case 460800: _baud=B460800; break;
    #endif
    #ifdef B576000
    case 576000: _baud=B576000; break;
    #endif
    #ifdef B921600
    case 921600: _baud=B921600; break;
    #endif
    #ifdef B1500000
    case 1500000: _baud=B1500000; break;
    #endif
    #ifdef B2000000
    case 2000000: _baud=B2000000; break;
    #endif

    default:
        break;
    }
    cfsetospeed(&newtio, (speed_t)_baud);
    cfsetispeed(&newtio, (speed_t)_baud);

    /* We generate mark and space parity ourself. */
    if (databits == 7 && (parity=='M' || parity == 'S'))
    {
        databits = 8;
    }
    switch (databits)
    {
    case 5:
        newtio.c_cflag = (newtio.c_cflag & ~CSIZE) | CS5;
        break;
    case 6:
        newtio.c_cflag = (newtio.c_cflag & ~CSIZE) | CS6;
        break;
    case 7:
        newtio.c_cflag = (newtio.c_cflag & ~CSIZE) | CS7;
        break;
    case 8:
    default:
        newtio.c_cflag = (newtio.c_cflag & ~CSIZE) | CS8;
        break;
    }
    newtio.c_cflag |= CLOCAL | CREAD;

    //parity
    newtio.c_cflag &= ~(PARENB | PARODD);
    if (parity == 'E')
    {
        newtio.c_cflag |= PARENB;
    }
    else if (parity== 'O')
    {
        newtio.c_cflag |= (PARENB | PARODD);
    }

    //hardware handshake
    newtio.c_cflag &= ~CRTSCTS;

    //stopbits
    if (stop=='2')
    {
        newtio.c_cflag |= CSTOPB;
    }
    else
    {
        newtio.c_cflag &= ~CSTOPB;
    }

    newtio.c_iflag=IGNBRK;

    //software handshake
    if (softwareHandshake)
    {
        newtio.c_iflag |= IXON | IXOFF;
    }
    else
    {
        newtio.c_iflag &= ~(IXON|IXOFF|IXANY);
    }

    newtio.c_lflag=0;
    newtio.c_oflag=0;

    //   newtio.c_cc[VTIME]=1;
    //   newtio.c_cc[VMIN]=60;

    if (tcsetattr(bt_dev_fd, TCSANOW, &newtio)!=0)
    {
        printf("tcsetattr() 1 failed \n");
    }

    if (tcgetattr(bt_dev_fd, &newtio)!=0)
    {
        printf("tcsetattr() 4 failed \n");
    }

    //hardware handshake
    if (hardwareHandshake)
    {
        newtio.c_cflag |= CRTSCTS;
    }
    else
    {
        newtio.c_cflag &= ~CRTSCTS;
    }

    if (tcsetattr(bt_dev_fd, TCSANOW, &newtio)!=0)
    {
        printf("tcsetattr() 2 failed \n");
    }
}


unsigned char atoh(char* s_data){
    unsigned char ret_value;
    char temp_value = 0;

    //h
    if(s_data[0] >= 48 && s_data[0] <= 57){
        temp_value = s_data[0] - 48;
    }else if(s_data[0] >= 65 && s_data[0] <= 70){
        temp_value = (s_data[0] - 65) + 10;
    }else if(s_data[0] >= 97 && s_data[0] <= 102){
        temp_value = (s_data[0] - 97) + 10;
    }

    ret_value = temp_value;
    ret_value = ret_value << 4;
    //printf("atoh1: ret_value:%x\r\n", ret_value);
    //l
    if(s_data[1] >= 48 && s_data[1] <= 57){
        temp_value = s_data[1] - 48;
    }else if(s_data[1] >= 65 && s_data[1] <= 70){
        temp_value = (s_data[1] - 65) + 10;
    }else if(s_data[1] >= 97 && s_data[1] <= 102){
        temp_value = (s_data[1] - 97) + 10;
    }

    ret_value |= temp_value;
    //printf("atoh2: ret_value:%x\r\n", ret_value);

    return ret_value;

}

int send_to_server(unsigned char* buffer, int len){
    int client_socket;
    struct sockaddr_in serverAddr;
    //unsigned long long package_totel_counter = 0;

    if((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        return ERR;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(BT_HCI_TOOL_PORT);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(connect(client_socket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0){
        return ERR;
    }

    send(client_socket, buffer, len, 0);
    return OK;
}

void send_hci_command(){
    int i = 0;
    int command_len = 0;
    unsigned char buffer[SOCKER_BUFFER_SIZE];

//    printf("send hci command: \r\n");

    for(i = 0;i < 256; i++){
        if(s_hci_command[i] == NULL){
            command_len = i;
            break;
        }

        buffer[i] = atoh(s_hci_command[i]);
//        printf("%02X ", buffer[i]);
    }

    send_to_server(buffer, i);

//    printf("\r\n");
}

void send_raw_hci_command(){
	if (bt_test_args.cmd == bt_stop && bt_test_args.trx_mode == tx){
		r_hci_command[r_hci_command_len++] = 0x01;
		r_hci_command[r_hci_command_len++] = 0x0C;
		r_hci_command[r_hci_command_len++] = 0x18;
		r_hci_command[r_hci_command_len++] = 0x01;
		r_hci_command[r_hci_command_len++] = 0x00;
	} else if (bt_test_args.cmd == bt_stop && bt_test_args.trx_mode == rx){
		r_hci_command[r_hci_command_len++] = 0x01;
		r_hci_command[r_hci_command_len++] = 0x0C;
		r_hci_command[r_hci_command_len++] = 0x18;
		r_hci_command[r_hci_command_len++] = 0x01;
		r_hci_command[r_hci_command_len++] = 0x01;
	} else if ((bt_test_args.cmd == ble_stop && bt_test_args.trx_mode == tx)
		|| bt_test_args.cmd == bt_reset){
		r_hci_command[r_hci_command_len++] = 0x01;
		r_hci_command[r_hci_command_len++] = 0x03;
		r_hci_command[r_hci_command_len++] = 0x0C;
		r_hci_command[r_hci_command_len++] = 0x00;
	} else if (bt_test_args.cmd == ble_stop && bt_test_args.trx_mode == rx){
		r_hci_command[r_hci_command_len++] = 0x01;
		r_hci_command[r_hci_command_len++] = 0x1F;
		r_hci_command[r_hci_command_len++] = 0x20;
		r_hci_command[r_hci_command_len++] = 0x00;
	} else if (bt_test_args.cmd == tx_tonestop){
		r_hci_command[r_hci_command_len++] = 0x01;
		r_hci_command[r_hci_command_len++] = 0x0C;
		r_hci_command[r_hci_command_len++] = 0x18;
		r_hci_command[r_hci_command_len++] = 0x01;
		r_hci_command[r_hci_command_len++] = 0x02;
	} else if (bt_test_args.cmd == bt_start){
		if (bt_test_args.trx_mode == tx){
			r_hci_command[r_hci_command_len++] = 0x01;
			r_hci_command[r_hci_command_len++] = 0x06;
			r_hci_command[r_hci_command_len++] = 0x18;
			r_hci_command[r_hci_command_len++] = 0x0E;
		} else {
			r_hci_command[r_hci_command_len++] = 0x01;
			r_hci_command[r_hci_command_len++] = 0x0B;
			r_hci_command[r_hci_command_len++] = 0x18;
			r_hci_command[r_hci_command_len++] = 0x0D;
		}
		switch (bt_test_args.type){
			case _dh1:
				r_hci_command[r_hci_command_len++] = 0x04;
				break;
			case _dh3:
				r_hci_command[r_hci_command_len++] = 0x0B;
				break;
			case _dh5:
				r_hci_command[r_hci_command_len++] = 0x0F;
				break;
			case _2dh1:
				r_hci_command[r_hci_command_len++] = 0x04;
				break;
			case _2dh3:
				r_hci_command[r_hci_command_len++] = 0x0A;
				break;
			case _2dh5:
				r_hci_command[r_hci_command_len++] = 0x0E;
				break;
			case _3dh1:
				r_hci_command[r_hci_command_len++] = 0x08;
				break;
			case _3dh3:
				r_hci_command[r_hci_command_len++] = 0x0B;
				break;
			case _3dh5:
				r_hci_command[r_hci_command_len++] = 0x0F;
				break;
			default:
				break;
		}
		r_hci_command[r_hci_command_len++] = bt_test_args.pattern;
		if (bt_test_args.trx_mode == tx){
			r_hci_command[r_hci_command_len++] = bt_test_args.hopping;
		}
		r_hci_command[r_hci_command_len++] = bt_test_args.channel;
		r_hci_command[r_hci_command_len++] = bt_test_args.is_edr;
		if (bt_test_args.trx_mode == tx){
			r_hci_command[r_hci_command_len++] = bt_test_args.whitening;
			r_hci_command[r_hci_command_len++] = (bt_test_args.len & 0xFF);
			r_hci_command[r_hci_command_len++] = ((bt_test_args.len >> 8) & 0xFF);
			r_hci_command[r_hci_command_len++] = bt_test_args.addr[5];
			r_hci_command[r_hci_command_len++] = bt_test_args.addr[4];
			r_hci_command[r_hci_command_len++] = bt_test_args.addr[3];
			r_hci_command[r_hci_command_len++] = bt_test_args.addr[2];
			r_hci_command[r_hci_command_len++] = bt_test_args.addr[1];
			r_hci_command[r_hci_command_len++] = bt_test_args.addr[0];
		} else {
			r_hci_command[r_hci_command_len++] = (bt_test_args.len & 0xFF);
			r_hci_command[r_hci_command_len++] = ((bt_test_args.len >> 8) & 0xFF);
			r_hci_command[r_hci_command_len++] = bt_test_args.whitening;
			r_hci_command[r_hci_command_len++] = bt_test_args.addr[5];
			r_hci_command[r_hci_command_len++] = bt_test_args.addr[4];
			r_hci_command[r_hci_command_len++] = bt_test_args.addr[3];
			r_hci_command[r_hci_command_len++] = bt_test_args.addr[2];
			r_hci_command[r_hci_command_len++] = bt_test_args.addr[1];
			r_hci_command[r_hci_command_len++] = bt_test_args.addr[0];
		}
	}else if (bt_test_args.cmd == ble_start && bt_test_args.trx_mode == tx){
		r_hci_command[r_hci_command_len++] = 0x01;
		r_hci_command[r_hci_command_len++] = 0x34;
		r_hci_command[r_hci_command_len++] = 0x20;
		r_hci_command[r_hci_command_len++] = 0x04;
		r_hci_command[r_hci_command_len++] = bt_test_args.channel;
		r_hci_command[r_hci_command_len++] = (bt_test_args.len & 0xFF);
		r_hci_command[r_hci_command_len++] = bt_test_args.pattern;
		r_hci_command[r_hci_command_len++] = bt_test_args.le_phy;
	} else if (bt_test_args.cmd == ble_start && bt_test_args.trx_mode == rx){
		r_hci_command[r_hci_command_len++] = 0x01;
		r_hci_command[r_hci_command_len++] = 0x33;
		r_hci_command[r_hci_command_len++] = 0x20;
		r_hci_command[r_hci_command_len++] = 0x03;
		r_hci_command[r_hci_command_len++] = bt_test_args.channel;
		r_hci_command[r_hci_command_len++] = bt_test_args.le_phy;
		r_hci_command[r_hci_command_len++] = bt_test_args.mod_idx;
    } else if (bt_test_args.cmd == tx_tone){
		r_hci_command[r_hci_command_len++] = 0x01;
		r_hci_command[r_hci_command_len++] = 0x0D;
		r_hci_command[r_hci_command_len++] = 0x18;
		r_hci_command[r_hci_command_len++] = 0x06;
		r_hci_command[r_hci_command_len++] = bt_test_args.channel;
		r_hci_command[r_hci_command_len++] = (bt_test_args.tx_pwr & 0x0F);;
		r_hci_command[r_hci_command_len++] = 0x00;
		r_hci_command[r_hci_command_len++] = 0x00;
		r_hci_command[r_hci_command_len++] = 0x00;
		r_hci_command[r_hci_command_len++] = 0x00;
	} else if (bt_test_args.cmd == dut_dis_scan){
		r_hci_command[r_hci_command_len++] = 0x01;
		r_hci_command[r_hci_command_len++] = 0x1A;
		r_hci_command[r_hci_command_len++] = 0x0C;
		r_hci_command[r_hci_command_len++] = 0x01;
		r_hci_command[r_hci_command_len++] = 0x00;
	} else if (bt_test_args.cmd == get_txpwr){
		r_hci_command[r_hci_command_len++] = 0x01;
		r_hci_command[r_hci_command_len++] = 0x67;
		r_hci_command[r_hci_command_len++] = 0xFC;
		r_hci_command[r_hci_command_len++] = 0x01;
		r_hci_command[r_hci_command_len++] = 0x00;
	} else if (bt_test_args.cmd == set_txpwr){
		r_hci_command[r_hci_command_len++] = 0x01;
		r_hci_command[r_hci_command_len++] = 0x65;
		r_hci_command[r_hci_command_len++] = 0xFC;
		r_hci_command[r_hci_command_len++] = 0x02;
		r_hci_command[r_hci_command_len++] = bt_test_args.tx_pwr;
		r_hci_command[r_hci_command_len++] = 0x00;
	} else if (bt_test_args.cmd == set_pwrofst){
		r_hci_command[r_hci_command_len++] = 0x01;
		r_hci_command[r_hci_command_len++] = 0x72;
		r_hci_command[r_hci_command_len++] = 0xFC;
		r_hci_command[r_hci_command_len++] = 0x24;
		r_hci_command[r_hci_command_len++] = 0x27;
		r_hci_command[r_hci_command_len++] = 0x00;
		r_hci_command[r_hci_command_len++] = 0x02;
		r_hci_command[r_hci_command_len++] = 0x00;
		r_hci_command[r_hci_command_len++] = 0x01;
		r_hci_command[r_hci_command_len++] = (uint8_t)bt_test_args.tx_pwrofst;
		r_hci_command_len = 0x24 + 4;//payload len + hdr len
	}

	send_to_server(r_hci_command, r_hci_command_len);
}

void send_wlan_command(void)
{
    int i = 0;
    int command_len = 0;
    unsigned char buffer[SOCKER_BUFFER_SIZE] = {0,};
    while ((w_command[i] != NULL) && (i < WLAN_CMD_MAX_CNT)) {
        strcat((char*)&buffer[0], w_command[i]);
        strcat((char*)&buffer[0], " ");
        i++;
    }
    command_len = strlen((char*)&buffer[0]);
    printf("command_len:%d \r\n", command_len);
    if (command_len > 1) {
        //command_len--; // rm last space
        //buffer[command_len] = '\0';
        send_to_server(buffer, command_len);
    }
}

int hci_send(char* data, int len){
    int i = 0;
    unsigned char tmp_data[len];

    printf("COMMAND(%d): \r\n", len);
    for(i = 0; i < len; i++){
        tmp_data[i] = (unsigned char)data[i];
        printf("%02X ", tmp_data[i]);
    }
    printf("\r\n");

    len = write(bt_dev_fd, tmp_data, len);
    if (len < 0) {
        printf("write data error \n");
        return FALSE;
    }
    return TRUE;
}


int hci_recv(char* data, int len){
    int i = 0;
    unsigned char tmp_byte[64];// = 0x00;

    memset(data, 0, len);

    len = read(bt_dev_fd, data, len);
    if (len < 0) {
        printf("read error \n");
        return FALSE;
    }
    printf("EVENT(%d): \r\n", len);
    for(i = 0; i < len; i++){
        tmp_byte[i] = (unsigned char)data[i];
        printf("%02X ", tmp_byte[i]);
    }
    printf("\r\n");

    if(tmp_byte[1] == 0x0D) { //bt rx
        uint32_t rx_pkt_len = tmp_byte[4] | (tmp_byte[5] << 8) | (tmp_byte[6] << 16) | (tmp_byte[7] << 24);
        uint32_t sync_ok = tmp_byte[8] | (tmp_byte[9] << 8) | (tmp_byte[10] << 16) | (tmp_byte[11] << 24);
        uint32_t pkt_rx_ok = tmp_byte[12] | (tmp_byte[13] << 8) | (tmp_byte[14] << 16) | (tmp_byte[15] << 24);
        uint32_t rx_err_pkts = tmp_byte[16] | (tmp_byte[17] << 8) | (tmp_byte[18] << 16) | (tmp_byte[19] << 24);
        uint32_t bit_err = tmp_byte[20] | (tmp_byte[21] << 8) | (tmp_byte[22] << 16) | (tmp_byte[23] << 24);
        uint32_t bit_cnt = sync_ok * rx_pkt_len * 8;
        float per = (float) 100 * rx_err_pkts / sync_ok;
        float ber = (float) 100 * bit_err / bit_cnt;
        printf("bt_rx_result: rx_ok = %d, per = %.2f%%, ber = %.3f%%\n", pkt_rx_ok, per, ber);
    }
    if(tmp_byte[4] == 0x1F && tmp_byte[5] == 0x20) { //ble rx
        uint32_t pkt_rx_ok = (tmp_byte[8] << 8) | tmp_byte[7];
        printf("ble_rx_result: rx_ok = %d\n", pkt_rx_ok);
    }
    if(tmp_byte[1] == 0x0E && tmp_byte[4] == 0x67) { //get txpwr
        int8_t pwr_in_dbm = tmp_byte[7];
        uint8_t pwr_lvl = tmp_byte[8];
        printf("tx pwr: pwr_in_dbm = %d dBm, pwr_lvl = 0x%x\n", pwr_in_dbm, pwr_lvl);
    }

    return len;
}

int wlan_send(char *data, int len)
{
    int sock;
    struct ifreq ifr;
    int ret = 0;
    int i = 0;
    char wl_cmd[64];
    struct android_wifi_priv_cmd priv_cmd;
    char is_param_err = 0;
    int buf_len = 0;
    int argC = 0;

    sock = socket(PF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        printf("bad sock!\n");
        return ERR;
    }

    memset(&ifr, 0, sizeof(ifr));
    strcpy(ifr.ifr_name, w_ifname);

    if (ioctl(sock, SIOCGIFFLAGS, &ifr) != 0) {
        printf("%s Could not read interface %s flags: %s",__func__, w_ifname, strerror(errno));
        return ERR;
    }

    if (!(ifr.ifr_flags & IFF_UP)) {
        printf("%s is not up!\n",w_ifname);
        return ERR;
    }

    printf("ifr.ifr_name = %s\n", ifr.ifr_name);
    memset(&priv_cmd, 0, sizeof(priv_cmd));
    memset(wl_cmd, 0, sizeof(wl_cmd));
    // extract wlan command & argC
    for (i = 0; i < len; i++) {
        if (data[i] != ' ') {
            if (argC == 0) {
                wl_cmd[i] = data[i];
            }
        } else {
            argC++;
        }
    }
    printf("wl_cmd=%s, argC=%d\r\n", wl_cmd, argC);

    priv_cmd.buf = data;
    priv_cmd.used_len = len;
    priv_cmd.total_len = SOCKER_BUFFER_SIZE;
    ifr.ifr_data = (void*)&priv_cmd;

    if ((strcasecmp(wl_cmd, "BT_RESET") == 0)) {
        char bt_reset_hci_cmd[32] = "01 03 0c 00";
        if (argC == 1) {
            buf_len = priv_cmd.used_len;
            memcpy(&priv_cmd.buf[buf_len], &bt_reset_hci_cmd[0], strlen(bt_reset_hci_cmd));
            buf_len += strlen(bt_reset_hci_cmd);
            priv_cmd.used_len = buf_len;
        } else {
            is_param_err = 1;
        }
    } else if ((strcasecmp(wl_cmd, "BT_TXDH") == 0)) {
        char bt_txdh_hci_cmd[255] = "01 06 18 0e ";
        if (argC == 15) {
            buf_len = priv_cmd.used_len;
            int arg_len = strlen("BT_TXDH");
            int txdh_cmd_len = strlen(bt_txdh_hci_cmd);
            memcpy(&bt_txdh_hci_cmd[txdh_cmd_len], &priv_cmd.buf[arg_len+1], buf_len - arg_len - 1);
            memcpy(&priv_cmd.buf[arg_len+1], &bt_txdh_hci_cmd[0], strlen(bt_txdh_hci_cmd));
            buf_len += strlen(bt_txdh_hci_cmd);
            priv_cmd.used_len = buf_len;
        } else {
            is_param_err = 1;
        }
    } else if ((strcasecmp(wl_cmd, "BT_RXDH") == 0)) {
        if (argC == 14) {
            char bt_rxdh_hci_cmd[255] = "01 0b 18 0d ";
            buf_len = priv_cmd.used_len;
            int arg_len = strlen("BT_RXDH");
            int rxdh_cmd_len = strlen(bt_rxdh_hci_cmd);
            memcpy(&bt_rxdh_hci_cmd[rxdh_cmd_len], &priv_cmd.buf[arg_len+1], buf_len - arg_len - 1);
            memcpy(&priv_cmd.buf[arg_len+1], &bt_rxdh_hci_cmd[0], strlen(bt_rxdh_hci_cmd));
            buf_len += strlen(bt_rxdh_hci_cmd);
            priv_cmd.used_len = buf_len;
        } else {
            is_param_err = 1;
        }
    }  else if ((strcasecmp(wl_cmd, "BT_STOP") == 0)) {
        char bt_stop_hci_cmd[255] = "01 0C 18 01 ";
        if (argC == 2) {
            buf_len = priv_cmd.used_len;
            int arg_len = strlen("BT_STOP");
            int stop_cmd_len = strlen(bt_stop_hci_cmd);
            memcpy(&bt_stop_hci_cmd[stop_cmd_len], &priv_cmd.buf[arg_len+1], buf_len - arg_len - 1);
            memcpy(&priv_cmd.buf[arg_len+1], &bt_stop_hci_cmd[0], strlen(bt_stop_hci_cmd));
            buf_len += strlen(bt_stop_hci_cmd);
            priv_cmd.used_len = buf_len;
    } else if ((strcasecmp(wl_cmd, "BT_DATA") == 0)) {
            //char bt_raw_data_cmd[255];
            int arg_len = strlen(wl_cmd);
            buf_len = priv_cmd.used_len;
            memcpy(&priv_cmd.buf[arg_len+1], &priv_cmd.buf[arg_len+1], buf_len - arg_len - 1);
            priv_cmd.used_len = buf_len - arg_len - 1;
    } else {
            is_param_err = 1;
        }
    } else {
        is_param_err = 0;
    }
    if (is_param_err) {
        printf("param error!!!\n");
        return ERR;
    }
    if ((ret = ioctl(sock, TXRX_PARA, &ifr)) < 0) {
        printf("%s: error ioctl[TX_PARA] ret= %d\n", __func__, ret);
        return ret;
    }
    memcpy(&priv_cmd, ifr.ifr_data, sizeof(struct android_wifi_priv_cmd));
    if (strcasecmp(wl_cmd, "GET_BT_RX_RESULT") == 0) {
        printf("done: get bt rx total=%d, ok=%d, err=%d\n", *(unsigned int *)priv_cmd.buf,
            *(unsigned int *)&priv_cmd.buf[4],
            *(unsigned int *)&priv_cmd.buf[8]);
    } else if (strcasecmp(wl_cmd, "BT_DATA") == 0) {
        unsigned char *buff = (unsigned char *)&priv_cmd.buf[1];
        int  len = priv_cmd.buf[0];
	int idx = 0;
        printf("done: %d\n", len);
        for (idx = 0; idx < len; idx++) {
            printf("%02x ", buff[idx]);
        }
        printf("\n");
     } else {
        printf("done\n");
    }
    return OK;
}

int dev_open(){
    int n = 0;

    bt_dev_fd = open(dev_path, O_RDWR | O_NDELAY);
    if(bt_dev_fd < 0) {
        printf("dev_path %s fail to open \r\n", dev_path);
        return ERR;
    }
    if(interface == UART){
        tcflush(bt_dev_fd, TCIOFLUSH);
        n = fcntl(bt_dev_fd, F_GETFL, 0);
        fcntl(bt_dev_fd, F_SETFL, n & ~O_NDELAY);
        uart_set_options(/*BAUDRATE*/atoi(s_baudrate), DATABITS, PARTY, STOP, SOFTWARE_HANDSHAKE, HARDWARE_HANDSHAKE);
    }else{
        ioctl(bt_dev_fd, DOWN_FW_CFG);
        ioctl(bt_dev_fd, GET_USB_INFO);
    }
    printf("bt dev open successful\r\n");
    return OK;
}

void dev_close(){
    close(bt_dev_fd);
}


int socket_init(){
    int client;
    int server_socket;
    struct sockaddr_in server_addr;
    struct sockaddr_in clientAddr;
    int addr_len = sizeof(clientAddr);
    char buffer[SOCKER_BUFFER_SIZE];
    int recv_len = 0;

    if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        return ERR;
    }

    //bzero(&server_addr, sizeof(server_addr));
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(BT_HCI_TOOL_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    //setsockopt(server_socket ,SOL_SOCKET ,SO_REUSEADDR,(const char*)&bReuseaddr,sizeof(BOOL));

    if(bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        printf("binding error\r\n");
        return ERR;
    }

    if(listen(server_socket, 5) < 0){
        printf("listen error");
        return ERR;
    }

    while(1){
        client = accept(server_socket, (struct sockaddr*)&clientAddr, (socklen_t*)&addr_len);
        if(client == ERR){
            return 0;
        }
        recv_len = recv(client, buffer, SOCKER_BUFFER_SIZE, 0);
        if ((interface == USB) || (interface == UART)) {
            hci_send(buffer, recv_len);
        } else {
            wlan_send(buffer, recv_len);
        }
    }
}


void* hci_recv_thread( void *arg ){
    printf("hci recv thread ready %p\r\n", arg);
    while(1){
        hci_recv(hci_event, sizeof(hci_event));
//        exit(0);
    }
}

void start_server(){
    int ret = ERR;
    if ((interface == USB) || (interface == UART)) {
        pthread_t hci_recv_th;
        //init bt communication
        ret = dev_open();
        if(ret == ERR){
            printf("dev_open fail \r\n");
            return;
        }
        //test to send reset
        printf("to test hci reset for bt device\r\n");
        hci_send(hci_reset, sizeof(hci_reset));
        hci_recv(hci_event, sizeof(hci_event));
        ret = pthread_create( &hci_recv_th, NULL, hci_recv_thread, NULL);
        printf("bt device test ok \r\n");
    } else if (interface == WLAN) {
        char buf[SOCKER_BUFFER_SIZE] = {"BT_RESET "};
        wlan_send(buf, strlen(buf));
    }

        //init localhost socket
        ret = socket_init();
        if (!ret) {
            printf("socket init done\r\n");
        }
}

int main(int argc, char *argv[]){
	int ret = parse_cmd_line(argc, argv);
	if (ret) {
		return ret;
	}
	if(server_or_client == SERVER){
		start_server();
	} else if (server_or_client == CLIENT) {
		send_hci_command();
	} else if (server_or_client == CLIENT_DIRECT) {
		if (bt_test_args.cmd == ble_start) {
			send_to_server((unsigned char*)hci_reset, sizeof(hci_reset));
		} else if (bt_test_args.cmd == dut) {
			send_to_server((unsigned char*)hci_reset, sizeof(hci_reset));
			send_to_server((unsigned char*)set_filter, sizeof(set_filter));
			send_to_server((unsigned char*)scan_on, sizeof(scan_on));
			send_to_server((unsigned char*)dut_en, sizeof(dut_en));
			return 0;
		}
		send_raw_hci_command();
	} else {
		send_wlan_command();
	}
	return 0;
}
