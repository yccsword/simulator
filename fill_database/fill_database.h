#ifndef __FILL_DATABASE_HEADER__
#define __FILL_DATABASE_HEADER__

#define FFFFFF 16777216
#define FFFF 65536
#define FF 256

#define C_FF0000(x) x>>16&0xFF
#define C_00FF00(x) x>>8&0xFF
#define C_0000FF(x) x&0xFF

#define SQL_INDEX_DELIMIT "|"
#define MAX_PARAM_COUNT 32

typedef enum {
	BUILD_DATABASE,
	INSERT_AP_INFO,
	INSERT_STA_INFO,
	INSERT_RADIO_INFO,
	CLEAR_AP_TABLE,
	CLEAR_STA_TABLE,
	CLEAR_RADIO_TABLE,
	CLEAR_APSTATE,
	INSERT_BY_FILE,
	UNKONW_PARAM,
} FillDataBaseParam;

#ifndef CWSTATETRANSITION
#define CWSTATETRANSITION
typedef enum {
	CW_ENTER_SULKING,
	CW_ENTER_DISCOVERY,
	CW_ENTER_JOIN,
	CW_ENTER_CONFIGURE,
	CW_ENTER_DATA_CHECK,
	CW_ENTER_RUN,
	CW_ENTER_RESET,
	CW_QUIT,
} CWStateTransition;
#endif

typedef struct {
	unsigned char Model[32];
	unsigned char MacHead[18];
	unsigned char WTPname[16];
	unsigned char HwVersion[16];
	unsigned char SwVersion[16];
	unsigned char BootVersion[16];
	unsigned int  RadioCount;
	unsigned int  IsCPE;
} APProductInfo;

typedef struct {
	unsigned char IPHead[10];
	unsigned char MacHead[18];
	unsigned char Type[16];
	unsigned char Rssi[16];
	unsigned char Ssid[32];
	unsigned char WireMode[16];
	unsigned char TxDataRate[16];
	unsigned char RxDataRate[16];
} STAProductInfo;

typedef struct {
	unsigned char PhyHead[32];
	unsigned char BssidHead[18];
} RADIOProductInfo;

#endif

