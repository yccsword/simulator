#ifndef __SIMULATOR_HEADER__
#define __SIMULATOR_HEADER__

#include <netinet/in.h>

#define DATABASE_NAME "simulator"
extern char mysql_addr[16];
extern char mysql_user[16];
extern char mysql_pwd[16];
extern char mysql_database[16];

extern int MaxRetryCount;
void GetMaxRetryCount(void);

#define SQL_INDEX_DELIMIT "|"
#define CW_STATION_MODEL_MAX_LEN 20
#define CW_STATION_WIRE_MODEL_LEN 32
#define CW_WLAN_SSID_STR_MAX_LEN 32
#ifndef ETH_ALEN
#define ETH_ALEN 6
#endif
/*Station wireless config*/
typedef struct
{
    char    			staMac[ETH_ALEN];
    char    			DevType[CW_STATION_MODEL_MAX_LEN];   
    unsigned int  		ipv4Addr;
    char    			RSSI;
    char    			wireModel[CW_STATION_WIRE_MODEL_LEN];
    char    			ssid[CW_WLAN_SSID_STR_MAX_LEN+1];
    unsigned long long  onlineTime;
    
    unsigned char       radioId;
    unsigned char       wlanId;
    unsigned short      tx_data_rate;
    unsigned short      rx_data_rate;
    unsigned int      	tx_flow;
    unsigned int      	rx_flow;
} CW_STATION_WIRELESS_CONFIG;

extern int SimulatorQuit;

void* SimulatorGetApUplineInfo(void *arg);
void* SimulatorApUpline(void *arg);
void* SimulatorGetApRunInfo(void *arg);
void* SimulatorApRun(void *arg);
void* SimulatorListeningEpoll(void *arg);

#define MAX_AP_TABLE_COUNT 1024
#define MAX_RADIO_COUNT 2
#define MAX_STA_COUNT 32

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
	unsigned int  Ip;
	unsigned char Mac[6];
	unsigned char Type[CW_STATION_MODEL_MAX_LEN];
	unsigned char Rssi;
	unsigned char Ssid[CW_WLAN_SSID_STR_MAX_LEN + 1];
	unsigned char WireMode[CW_STATION_WIRE_MODEL_LEN];
	unsigned int  OnlineTime;
	unsigned char RadioId;
	unsigned char WlanId;
	unsigned short TxDataRate;
	unsigned short RxDataRate;
	unsigned int  TxFlow;
	unsigned int  RxFlow;
	
} STA_TABLE;

typedef struct {
	unsigned char PhyName[8];
	unsigned char Bssid[6];
} RADIO_TABLE;

#ifndef CWBOOL
#define CWBOOL
typedef enum {
	CW_FALSE = 0,
	CW_TRUE = 1
} CWBool;
#endif

#ifndef CWACDESCRIPTOR
#define CWACDESCRIPTOR
typedef struct {
	char *address;
	CWBool received;
	int seqNum;
} CWACDescriptor;
#endif

#ifndef CWNETWORKLEV4ADDRESS
#define CWNETWORKLEV4ADDRESS
typedef struct sockaddr_storage CWNetworkLev4Address;
#endif

#ifndef CWACVENDORINFOS
#define CWACVENDORINFOS
typedef enum {
	CW_PRESHARED = 4,
	CW_X509_CERTIFICATE = 2
} CWAuthSecurity;

typedef struct {
	CWNetworkLev4Address addr;
	struct sockaddr_in addrIPv4;

	int WTPCount;
} CWProtocolNetworkInterface;

typedef struct {
	int WTPCount;
	struct sockaddr_in addr;
} CWProtocolIPv4NetworkInterface;

typedef struct {
	int WTPCount;
	struct sockaddr_in6 addr;
} CWProtocolIPv6NetworkInterface;

typedef struct {
	int vendorIdentifier;
	enum {
		CW_AC_HARDWARE_VERSION	= 4,
		CW_AC_SOFTWARE_VERSION	= 5
	} type;
	int length;
	int *valuePtr;
} CWACVendorInfoValues;

typedef struct  {
	int vendorInfosCount;
	CWACVendorInfoValues *vendorInfos;
} CWACVendorInfos;
#endif

#ifndef CWACINFOVALUES
#define CWACINFOVALUES
typedef struct {
	int ACIPv4ListCount;
	int *ACIPv4List;	
}ACIPv4ListValues;

typedef struct {
	int ACIPv6ListCount;
	struct in6_addr *ACIPv6List;	
}ACIPv6ListValues;

typedef struct {
	int stations;
	int limit;
	int activeWTPs;
	int maxWTPs;
	CWAuthSecurity security;
	int RMACField;
//	int WirelessField;
	int DTLSPolicy;
	CWACVendorInfos vendorInfos;
	char *name;
	CWProtocolIPv4NetworkInterface *IPv4Addresses;
	int IPv4AddressesCount;
	CWProtocolIPv6NetworkInterface *IPv6Addresses;
	int IPv6AddressesCount;
	ACIPv4ListValues ACIPv4ListInfo;
	ACIPv6ListValues ACIPv6ListInfo;
	CWNetworkLev4Address preferredAddress;
	CWNetworkLev4Address incomingAddress;
} CWACInfoValues;
#endif

typedef struct {
	unsigned int  ApIndex;
	unsigned char Model[32];
	unsigned char Mac[6];
	unsigned char WTPname[16];
	unsigned char HwVersion[16];
	unsigned char SwVersion[16];
	unsigned char BootVersion[16];
	CWStateTransition State;
	unsigned int  RadioCount;
	RADIO_TABLE Radio_Info[MAX_RADIO_COUNT];
	RADIO_TABLE Connect_Radio_Info[MAX_RADIO_COUNT];
	unsigned int  StaCount;
	STA_TABLE Sta_Info[MAX_STA_COUNT];
	unsigned int  IsCPE;
	unsigned long OnlineTimeStamp;
	int WTP_PORT_CONTROL;
	int WTP_PORT_DATA;
	int CWDiscoveryCount;
	int WTPSocket;
	int WTPDataSocket;
	int CWACCount;//only one
	CWACDescriptor CWACList;//only one
	CWACInfoValues *ACInfoPtr;
	int CWDiscoveryIntervalSec;
	int CWDiscoveryIntervaluSec;
	int WTPRetransmissionCount;
	int WTPEchoRetransmissionCount;
	int Epoll_fd_Index;
} AP_TABLE;

typedef struct {
	unsigned int  SocketInFlag;
	unsigned int  SocketLife;
	unsigned int  InfoType;
} SocketInInfo;

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

extern AP_TABLE ap_upline_table[MAX_AP_TABLE_COUNT];
extern AP_TABLE ap_run_table[MAX_AP_TABLE_COUNT];

extern unsigned int ap_upline_count;
extern unsigned int ap_run_count;

extern unsigned int ap_upline_act_count;
extern unsigned int ap_run_act_count;

void Epoll_Add_Socket(AP_TABLE * cur_AP);
void Epoll_Mod_Socket(AP_TABLE * cur_AP);
void Epoll_Del_Socket(AP_TABLE * cur_AP);

extern int EchoInterval;
extern int HasWTPEvent;
extern int HasEcho;
extern int HasCheckEchoRespone;
extern int ApUplineInterval;
extern int FastSend;
extern int Max_Run_WaitCount;

#define _1MS 1000
#define _1S 1000000

#define MAX_LINUX_SOCKET 65535

typedef enum {
	UplineThreadPool = 0,
	RunThreadPool = 1
} SimulatorThreadType;

#endif
