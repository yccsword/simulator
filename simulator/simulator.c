#include <stdio.h>
#include <string.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <endian.h>
#include <sys/socket.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <errno.h>
#include "tools.h"
#include "simulator.h"
#include "threadpool.h"

AP_TABLE ap_upline_table[MAX_AP_TABLE_COUNT] = {{0}};
AP_TABLE ap_run_table[MAX_AP_TABLE_COUNT] = {{0}};
unsigned int ap_upline_count = 0;
unsigned int ap_run_count = 0;
unsigned int ap_upline_act_count = 0;
unsigned int ap_run_act_count = 0;
char mysql_addr[16] = "127.0.0.1";
char mysql_user[16] = "root";
char mysql_pwd[16] = "00000";
char mysql_database[16] = DATABASE_NAME;

int ListeningEpoll = 0;
SocketInInfo SocketIn[MAX_LINUX_SOCKET + 1] = {{0}};
int HasCheckEchoRespone = 1;

static pthread_cond_t upline_send_cond;
static pthread_mutex_t upline_send_mtx;
static pthread_cond_t upline_fill_cond;
static pthread_mutex_t upline_fill_mtx;
static pthread_cond_t run_send_cond;
static pthread_mutex_t run_send_mtx;
static pthread_cond_t run_fill_cond;
static pthread_mutex_t run_fill_mtx;

#if 1
int usleep (useconds_t useconds)
{
	struct timeval    tval;
    tval.tv_sec = useconds / 1000000;
    tval.tv_usec = useconds % 1000000;
    select(0, NULL, NULL, NULL, &tval);
}
#else
int usleep (useconds_t useconds)
{
	struct timespec ts = { .tv_sec = (long int) (useconds / 1000000),
	                     .tv_nsec = (long int) (useconds % 1000000) * 1000ul };

	/* Note the usleep() is a cancellation point.  But since we call
	 nanosleep() which itself is a cancellation point we do not have
	 to do anything here.  */
	//return __nanosleep (&ts, NULL);
	return nanosleep (&ts, NULL);
}
#endif
#if 1
int mysleep (unsigned long seconds)
{
	struct timeval    tval;
    tval.tv_sec = seconds;
    tval.tv_usec = 0;
    select(0, NULL, NULL, NULL, &tval);
}
#else
#define mysleep(seconds) sleep(seconds)
#endif

void Epoll_Add_Socket(AP_TABLE * cur_AP)
{
	//Epoll 监听cur_AP->WTPSocket
	struct epoll_event event = {0};
	setnonblocking(cur_AP->WTPSocket);
	event.data.fd = cur_AP->WTPSocket;
	event.events = EPOLLIN|EPOLLET|EPOLLONESHOT;
	if(0 != epoll_ctl(Epoll_fd[cur_AP->Epoll_fd_Index],EPOLL_CTL_ADD,cur_AP->WTPSocket,&event))
	{
		fprintf(stderr,"%sap sock:%d %s %d EPOLL_CTL_ADD error%s\n",COLOR_RED,cur_AP->WTPSocket,__func__,__LINE__,COLOR_END);
	}
}

void Epoll_Mod_Socket(AP_TABLE * cur_AP)
{
	//Epoll 重新监听cur_AP->WTPSocket
	struct epoll_event event = {0};
	event.data.fd = cur_AP->WTPSocket;
	event.events = EPOLLIN|EPOLLET|EPOLLONESHOT;
	if(0 != epoll_ctl(Epoll_fd[cur_AP->Epoll_fd_Index],EPOLL_CTL_MOD,cur_AP->WTPSocket,&event))
	{
		fprintf(stderr,"%sap sock:%d %s %d EPOLL_CTL_MOD error%s\n",COLOR_RED,cur_AP->WTPSocket,__func__,__LINE__,COLOR_END);
	}
}

void Epoll_Del_Socket(AP_TABLE * cur_AP)
{
	//Epoll 取消cur_AP->WTPSocket 监听
	struct epoll_event event = {0};
	event.data.fd = cur_AP->WTPSocket;
	event.events = EPOLLIN|EPOLLET|EPOLLONESHOT;
	if(0 != epoll_ctl(Epoll_fd[cur_AP->Epoll_fd_Index],EPOLL_CTL_DEL,cur_AP->WTPSocket,&event))
	{
		fprintf(stderr,"%sap sock:%d %s %d EPOLL_CTL_DEL error%s\n",COLOR_RED,cur_AP->WTPSocket,__func__,__LINE__,COLOR_END);
	}
}

void RunEpoll_Add_Socket(int WTPSocket)
{
	//Epoll 监听cur_AP->WTPSocket
	struct epoll_event event = {0};
	setnonblocking(WTPSocket);
	event.data.fd = WTPSocket;
	event.events = EPOLLIN|EPOLLET;
	if(0 != epoll_ctl(ListeningEpoll,EPOLL_CTL_ADD,WTPSocket,&event))
	{
		fprintf(stderr,"%sap sock:%d %s %d EPOLL_CTL_ADD error%s\n",COLOR_RED,WTPSocket,__func__,__LINE__,COLOR_END);
	}
}

void RunEpoll_Del_Socket(int WTPSocket)
{
	//Epoll 取消cur_AP->WTPSocket 监听
	struct epoll_event event = {0};
	event.data.fd = WTPSocket;
	event.events = EPOLLIN|EPOLLET;
	if(0 != epoll_ctl(ListeningEpoll,EPOLL_CTL_DEL,WTPSocket,&event))
	{
		fprintf(stderr,"%sap sock:%d %s %d EPOLL_CTL_DEL error%s\n",COLOR_RED,WTPSocket,__func__,__LINE__,COLOR_END);
	}
}

int SimulatorQuit = 0;
int MaxRetryCount = 5;
void GetMaxRetryCount()
{
	MaxRetryCount = 5;
}

// mysql操作符
MYSQL *mysql_upline = NULL;
MYSQL *mysql_run = NULL;
pthread_mutex_t mysql_init_mutex;

inline int MySqlInit(MYSQL **mysql)
{
	// 初始化
	CWThreadMutexLock(&mysql_init_mutex);
	*mysql = mysql_init(NULL);
	CWThreadMutexUnlock(&mysql_init_mutex);
	if (!*mysql) {
		fprintf(stderr,"%s%s %d mysql_errno:%d \nmysql_error:%s%s\n",
					COLOR_RED,__func__,__LINE__,mysql_errno(*mysql),mysql_error(*mysql),COLOR_END);
		return EXIT_FAILURE;
	}
	//fprintf(stderr,"%s\n", "init");
	// 链接
	*mysql = mysql_real_connect(*mysql, mysql_addr, mysql_user, mysql_pwd, mysql_database, 0, NULL, 0);
	if (!*mysql) {
		fprintf(stderr,"%s%s %d mysql_errno:%d \nmysql_error:%s%s\n",
					COLOR_RED,__func__,__LINE__,mysql_errno(*mysql),mysql_error(*mysql),COLOR_END);
		return EXIT_FAILURE;
	}
	//fprintf(stderr,"%s\n", "connect");
	mysql_set_character_set(*mysql, "utf8");
}

inline MYSQL_RES * MySqlExecQuery(MYSQL **mysql, char *sqlcmd)
{
	// mysql结果集
	MYSQL_RES *mysql_res;
	int r;
	// 执行查询
	r = mysql_real_query(*mysql, sqlcmd, strlen(sqlcmd));
	if (r) {fprintf(stderr,"%s%s %d error:%d mysql_errno:%d \nmysql_error:%s%s\n",
					COLOR_RED,__func__,__LINE__,r,mysql_errno(*mysql),mysql_error(*mysql),COLOR_END);return NULL;}
	mysql_res = mysql_store_result(*mysql);
	if (!mysql_res) 
	{
		return NULL;
	}
	else
	{
		return mysql_res;
	}
}

inline my_ulonglong MySqlExecQuery_NoResult(MYSQL **mysql, char *sqlcmd)
{
	int r;
	// 执行查询
	r = mysql_real_query(*mysql, sqlcmd, strlen(sqlcmd));
	if (r) {fprintf(stderr,"%s%s %d error:%d mysql_errno:%d \nmysql_error:%s%s\n",
					COLOR_RED,__func__,__LINE__,r,mysql_errno(*mysql),mysql_error(*mysql),COLOR_END);return 0;}
	return mysql_affected_rows(*mysql);
}

inline int MySqlClose(MYSQL **mysql)
{
	// 结束
	mysql_close(*mysql);
	return EXIT_SUCCESS;
}

inline void ZeroAPState(MYSQL **mysql)
{
	char sqlcmd[200];
	my_ulonglong affect_line = 0;
	sprintf(sqlcmd, "update AP_Table set State = 0, WTPSocket = 0");
	fprintf(stderr,"%sSql ZeroAPState%s affect_line:%ld\n", COLOR_GREEN, COLOR_END, MySqlExecQuery_NoResult(mysql, sqlcmd));
}


int waitUplineTableFillTime = 2;//s
int UplineTableFillInterval = 300;//us//0.3ms
int ApUplineSendInterval = 700;//us//0.7ms
int FastSend = 0;
pthread_mutex_t UplineInterval_mutex;
inline void FillApUplineTable(MYSQL **mysql)
{
	char * RadioIndex[MAX_RADIO_COUNT] = {0}; 
	char * ConnectRadioIndex[MAX_RADIO_COUNT] = {0}; 
	char * StaIndex[MAX_STA_COUNT] = {0}; 
	int RadioIndexCount = 0;
	int ConnectRadioIndexCount = 0;
	int StaIndexCount = 0;
	int i = 0;
	char sqlcmd[200];
	my_ulonglong affect_line = 0;
	// mysql行操作符
	MYSQL_ROW mysql_row = NULL;
	MYSQL_ROW mysql_subrow = NULL;
	// mysql结果集
	MYSQL_RES *mysql_res = NULL;
	MYSQL_RES *mysql_subres = NULL;

	sprintf(sqlcmd, "select * from AP_Table where State = '%d'", CW_ENTER_SULKING);
	mysql_res = MySqlExecQuery(mysql, sqlcmd);
	if (mysql_res == NULL) {fprintf(stderr,"%s%s %d mysql_res==NULL%s\n",COLOR_RED,__func__,__LINE__,COLOR_END);return;}
	while((mysql_row = mysql_fetch_row(mysql_res))) 
	{
		while(ap_upline_table[(ap_upline_count + 1) % MAX_AP_TABLE_COUNT].Model[0] != '\0')
		{
			if (!FastSend)
			{
				CWThreadMutexLock(&UplineInterval_mutex);
				(ApUplineSendInterval >= 100) ? (ApUplineSendInterval-=100) : (ApUplineSendInterval = 0);
				UplineTableFillInterval += 1000;//1ms
				CWThreadMutexUnlock(&UplineInterval_mutex);
				fprintf(stderr,"%sap_upline_table Has Full, NextIndex:%d, Wait... Change UplineTableFillInterval:%dus Change ApUplineSendInterval:%dus %s\n", COLOR_YELLOW, ap_upline_count + 1, UplineTableFillInterval, ApUplineSendInterval, COLOR_END); 
				mysleep(2);
			}
			else
			{
				//fprintf(stderr,"%sap_upline_table Has Full, NextIndex:%d, Wait... Change UplineTableFillInterval:%dus Change ApUplineSendInterval:%dus %s\n", COLOR_YELLOW, ap_upline_count + 1, UplineTableFillInterval, ApUplineSendInterval, COLOR_END); 		
				pthread_mutex_lock(&upline_fill_mtx);
				pthread_cond_wait(&upline_fill_cond, &upline_fill_mtx);
				pthread_mutex_unlock(&upline_fill_mtx);
			}
		}
		ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].ApIndex = atoi(mysql_row[0]);
		memcpy(ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].Model, mysql_row[1], sizeof(ap_upline_table[0].Model));
		Str2Mac(ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].Mac, mysql_row[2]);
		memcpy(ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].WTPname, mysql_row[3], sizeof(ap_upline_table[0].WTPname));
		memcpy(ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].HwVersion, mysql_row[4], sizeof(ap_upline_table[0].HwVersion));
		memcpy(ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].SwVersion, mysql_row[5], sizeof(ap_upline_table[0].SwVersion));
		memcpy(ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].BootVersion, mysql_row[6], sizeof(ap_upline_table[0].BootVersion));
		ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].State = CW_ENTER_DISCOVERY;//atoi(mysql_row[7]);
		ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].RadioCount = atoi(mysql_row[8]);
		RadioIndexCount = 0;split(mysql_row[9], SQL_INDEX_DELIMIT, RadioIndex, &RadioIndexCount);
		if (RadioIndexCount != ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].RadioCount) 
		{fprintf(stderr,"%sError Sql RadioCount:%d RadioInfoIndexCount:%d%s\n",COLOR_YELLOW,ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].RadioCount,RadioIndexCount,COLOR_END);}
		#if 1
		for (i = 0;i < RadioIndexCount; i++)
		{
			sprintf(sqlcmd, "select * from Radio_Table where `Index` = %s", RadioIndex[i]);
			mysql_subres = MySqlExecQuery(mysql, sqlcmd);
			if (mysql_subres == NULL) {fprintf(stderr,"%s%s %d mysql_subres==NULL sqlcmd:%s%s\n",COLOR_RED,__func__,__LINE__,sqlcmd,COLOR_END);break;}
			while((mysql_subrow = mysql_fetch_row(mysql_subres))) 
			{
				memcpy(ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].Radio_Info[i].PhyName, mysql_subrow[1], sizeof(ap_upline_table[0].Radio_Info[0].PhyName));
				Str2Mac(ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].Radio_Info[i].Bssid, mysql_subrow[2]);
			}
			mysql_free_result(mysql_subres);
		}
		#endif
		ConnectRadioIndexCount = 0;split(mysql_row[10], SQL_INDEX_DELIMIT, ConnectRadioIndex, &ConnectRadioIndexCount);
		#if 1
		for (i = 0;i < ConnectRadioIndexCount; i++)
		{
			sprintf(sqlcmd, "select * from Radio_Table where `Index` = %s", ConnectRadioIndex[i]);
			mysql_subres = MySqlExecQuery(mysql, sqlcmd);
			if (mysql_subres == NULL) {fprintf(stderr,"%s%s %d mysql_subres==NULL sqlcmd:%s%s\n",COLOR_RED,__func__,__LINE__,sqlcmd,COLOR_END);break;}
			while((mysql_subrow = mysql_fetch_row(mysql_subres))) 
			{
				memcpy(ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].Connect_Radio_Info[i].PhyName, mysql_subrow[1], sizeof(ap_upline_table[0].Connect_Radio_Info[0].PhyName));
				Str2Mac(ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].Connect_Radio_Info[i].Bssid, mysql_subrow[2]);
			}
			mysql_free_result(mysql_subres);
		}
		#endif
		ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].StaCount = atoi(mysql_row[11]);
		#if 0
		StaIndexCount = 0;split(mysql_row[12], SQL_INDEX_DELIMIT, StaIndex, &StaIndexCount);
		if (StaIndexCount != ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].StaCount) 
		{fprintf(stderr,"%sError Sql StaCount:%d StaIndexCount:%d %s\n",COLOR_YELLOW,ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].StaCount,StaIndexCount,StaIndexCount,COLOR_END);}
		for (i = 0;i < StaIndexCount; i++)
		{
			sprintf(sqlcmd, "select * from Sta_Table where `Index` = %s", StaIndex[i]);
			mysql_subres = MySqlExecQuery(mysql, sqlcmd);
			if (mysql_subres == NULL) {fprintf(stderr,"%s%s %d mysql_subres==NULL sqlcmd:%s%s\n",COLOR_RED,__func__,__LINE__,sqlcmd,COLOR_END);break;}
			while((mysql_subrow = mysql_fetch_row(mysql_subres))) 
			{
				ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].Sta_Info[i].Ip = Ip2Int(mysql_subrow[1]);
				Str2Mac(ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].Sta_Info[i].Mac, mysql_subrow[2]);
				memcpy(ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].Sta_Info[i].Type, mysql_subrow[3], sizeof(ap_upline_table[0].Sta_Info[0].Type));
				ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].Sta_Info[i].Rssi = atoi(mysql_subrow[4]);
				memcpy(ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].Sta_Info[i].Ssid, mysql_subrow[5], sizeof(ap_upline_table[0].Sta_Info[0].Ssid));
				memcpy(ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].Sta_Info[i].WireMode, mysql_subrow[6], sizeof(ap_upline_table[0].Sta_Info[0].WireMode));
				ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].Sta_Info[i].OnlineTime = atoi(mysql_subrow[7]);
				ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].Sta_Info[i].RadioId = atoi(mysql_subrow[8]);
				ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].Sta_Info[i].WlanId = atoi(mysql_subrow[9]);
				ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].Sta_Info[i].TxDataRate = atoi(mysql_subrow[10]);
				ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].Sta_Info[i].RxDataRate = atoi(mysql_subrow[11]);
				ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].Sta_Info[i].TxFlow = atoi(mysql_subrow[12]);
				ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].Sta_Info[i].RxFlow = atoi(mysql_subrow[13]);
			}
			mysql_free_result(mysql_subres);
		}
		#endif
		ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].IsCPE = atoi(mysql_row[13]);
		ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].OnlineTimeStamp = atol(mysql_row[14]);
		sprintf(sqlcmd, "update AP_Table set State = '%d' where `Index` = '%d'", CW_ENTER_DISCOVERY, ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].ApIndex);
		affect_line = MySqlExecQuery_NoResult(mysql, sqlcmd);
		if (affect_line != 1) 
		{fprintf(stderr,"%sError Sql set AP:%d state DISCOVERY%s affect_line:%d\n", COLOR_YELLOW, ap_upline_table[ap_upline_count % MAX_AP_TABLE_COUNT].ApIndex, COLOR_END, affect_line);}
		ap_upline_count++;
		if (ap_upline_count / MAX_AP_TABLE_COUNT == 10000 && ap_upline_count % MAX_AP_TABLE_COUNT == 0) {ap_upline_count = 0;}
		if (FastSend)
		{
			pthread_cond_signal(&upline_send_cond); 
		}
		if (UplineTableFillInterval < _1S)
		{
			usleep(UplineTableFillInterval);
		}
		else
		{
			mysleep(UplineTableFillInterval % _1S);
		}
	}
	// 释放
	mysql_free_result(mysql_res);
}

int EchoInterval = 30;//ycc config Get Run Ap Half AC EchoInterval Detective
inline void FillApRunTable(MYSQL **mysql)
{
	char * RadioIndex[MAX_RADIO_COUNT] = {0}; 
	char * ConnectRadioIndex[MAX_RADIO_COUNT] = {0}; 
	char * StaIndex[MAX_STA_COUNT] = {0}; 
	int RadioIndexCount = 0;
	int ConnectRadioIndexCount = 0;
	int StaIndexCount = 0;
	int i = 0;
	char sqlcmd[200];
	my_ulonglong affect_line = 0;
	// mysql行操作符
	MYSQL_ROW mysql_row = NULL;
	MYSQL_ROW mysql_subrow = NULL;
	// mysql结果集
	MYSQL_RES *mysql_res = NULL;
	MYSQL_RES *mysql_subres = NULL;
	time_t time_stamp_for_getRun;
	time(&time_stamp_for_getRun);
	//升序查询asc(降序desc)，AP量大的时候，让时间较早的先进入队列
	sprintf(sqlcmd, "select * from AP_Table where State = '%d' and OnlineTimeStamp < '%ld' order by OnlineTimeStamp asc", CW_ENTER_RUN, time_stamp_for_getRun - EchoInterval/2);
	mysql_res = MySqlExecQuery(mysql, sqlcmd);
	if (mysql_res == NULL) {fprintf(stderr,"%s%s %d mysql_res==NULL%s\n",COLOR_RED,__func__,__LINE__,COLOR_END);return;}
	while((mysql_row = mysql_fetch_row(mysql_res))) 
	{
 		while(ap_run_table[(ap_run_count + 1) % MAX_AP_TABLE_COUNT].Model[0] != '\0')
		{
			if (!FastSend)
			{
				CWThreadMutexLock(&UplineInterval_mutex);
				UplineTableFillInterval+=100;//+0.1ms
				(ApUplineSendInterval < _1S - 200) ? (ApUplineSendInterval+=200) : (ApUplineSendInterval = _1S - 100);//+0.2ms
				CWThreadMutexUnlock(&UplineInterval_mutex);
				fprintf(stderr,"%sap_run_table Has Full, NextIndex:%d, Wait... Change UplineTableFillInterval:%dus Change ApUplineSendInterval:%dus %s\n", COLOR_YELLOW, ap_run_count + 1, UplineTableFillInterval, ApUplineSendInterval, COLOR_END); 
				mysleep(1);
			}
			else
			{
				//fprintf(stderr,"%sap_run_table Has Full, NextIndex:%d, Wait... Change UplineTableFillInterval:%dus Change ApUplineSendInterval:%dus %s\n", COLOR_YELLOW, ap_run_count + 1, UplineTableFillInterval, ApUplineSendInterval, COLOR_END); 
				pthread_mutex_lock(&run_fill_mtx);
				pthread_cond_wait(&run_fill_cond, &run_fill_mtx);
				pthread_mutex_unlock(&run_fill_mtx);
			}
		}
		ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].ApIndex = atoi(mysql_row[0]);
		memcpy(ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].Model, mysql_row[1], sizeof(ap_run_table[0].Model));
		Str2Mac(ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].Mac, mysql_row[2]);
		memcpy(ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].WTPname, mysql_row[3], sizeof(ap_run_table[0].WTPname));
		memcpy(ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].HwVersion, mysql_row[4], sizeof(ap_run_table[0].HwVersion));
		memcpy(ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].SwVersion, mysql_row[5], sizeof(ap_run_table[0].SwVersion));
		memcpy(ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].BootVersion, mysql_row[6], sizeof(ap_run_table[0].BootVersion));
		ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].State = atoi(mysql_row[7]);
		ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].RadioCount = atoi(mysql_row[8]);
		#if 0
		RadioIndexCount = 0;split(mysql_row[9], SQL_INDEX_DELIMIT, RadioIndex, &RadioIndexCount);
		if (RadioIndexCount != ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].RadioCount) 
		{fprintf(stderr,"%sError Sql RadioCount:%d RadioInfoIndexCount:%d%s\n",COLOR_YELLOW,ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].RadioCount,RadioIndexCount,COLOR_END);}
		for (i = 0;i < RadioIndexCount; i++)
		{
			sprintf(sqlcmd, "select * from Radio_Table where `Index` = %s", RadioIndex[i]);
			mysql_subres = MySqlExecQuery(mysql, sqlcmd);
			if (mysql_subres == NULL) {fprintf(stderr,"%s%s %d mysql_subres==NULL sqlcmd:%s%s\n",COLOR_RED,__func__,__LINE__,sqlcmd,COLOR_END);break;}
			while((mysql_subrow = mysql_fetch_row(mysql_subres))) 
			{
				memcpy(ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].Radio_Info[i].PhyName, mysql_subrow[1], sizeof(ap_run_table[0].Radio_Info[0].PhyName));
				Str2Mac(ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].Radio_Info[i].Bssid, mysql_subrow[2]);
			}
			mysql_free_result(mysql_subres);
		}
		#endif
		#if 0
		ConnectRadioIndexCount = 0;split(mysql_row[10], SQL_INDEX_DELIMIT, ConnectRadioIndex, &ConnectRadioIndexCount);
		for (i = 0;i < ConnectRadioIndexCount; i++)
		{
			sprintf(sqlcmd, "select * from Radio_Table where `Index` = %s", ConnectRadioIndex[i]);
			mysql_subres = MySqlExecQuery(mysql, sqlcmd);
			if (mysql_subres == NULL) {fprintf(stderr,"%s%s %d mysql_subres==NULL sqlcmd:%s%s\n",COLOR_RED,__func__,__LINE__,sqlcmd,COLOR_END);break;}
			while((mysql_subrow = mysql_fetch_row(mysql_subres))) 
			{
				memcpy(ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].Connect_Radio_Info[i].PhyName, mysql_subrow[1], sizeof(ap_run_table[0].Connect_Radio_Info[0].PhyName));
				Str2Mac(ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].Connect_Radio_Info[i].Bssid, mysql_subrow[2]);
			}
			mysql_free_result(mysql_subres);
		}
		#endif
		ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].StaCount = atoi(mysql_row[11]);
		StaIndexCount = 0;split(mysql_row[12], SQL_INDEX_DELIMIT, StaIndex, &StaIndexCount);
		if (StaIndexCount != ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].StaCount) 
		{fprintf(stderr,"%sError Sql StaCount:%d StaIndexCount:%d %s\n",COLOR_YELLOW,ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].StaCount,StaIndexCount,StaIndexCount,COLOR_END);}
		#if 1
		for (i = 0;i < StaIndexCount; i++)
		{
			sprintf(sqlcmd, "select * from Sta_Table where `Index` = %s", StaIndex[i]);
			mysql_subres = MySqlExecQuery(mysql, sqlcmd);
			if (mysql_subres == NULL) {fprintf(stderr,"%s%s %d mysql_subres==NULL sqlcmd:%s%s\n",COLOR_RED,__func__,__LINE__,sqlcmd,COLOR_END);break;}
			while((mysql_subrow = mysql_fetch_row(mysql_subres))) 
			{
				ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].Sta_Info[i].Ip = Ip2Int(mysql_subrow[1]);
				Str2Mac(ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].Sta_Info[i].Mac, mysql_subrow[2]);
				memcpy(ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].Sta_Info[i].Type, mysql_subrow[3], sizeof(ap_run_table[0].Sta_Info[0].Type));
				ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].Sta_Info[i].Rssi = atoi(mysql_subrow[4]);
				memcpy(ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].Sta_Info[i].Ssid, mysql_subrow[5], sizeof(ap_run_table[0].Sta_Info[0].Ssid));
				memcpy(ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].Sta_Info[i].WireMode, mysql_subrow[6], sizeof(ap_run_table[0].Sta_Info[0].WireMode));
				ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].Sta_Info[i].OnlineTime = atoi(mysql_subrow[7]);
				ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].Sta_Info[i].RadioId = atoi(mysql_subrow[8]);
				ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].Sta_Info[i].WlanId = atoi(mysql_subrow[9]);
				ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].Sta_Info[i].TxDataRate = atoi(mysql_subrow[10]);
				ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].Sta_Info[i].RxDataRate = atoi(mysql_subrow[11]);
				ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].Sta_Info[i].TxFlow = atoi(mysql_subrow[12]);
				ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].Sta_Info[i].RxFlow = atoi(mysql_subrow[13]);
			}
			mysql_free_result(mysql_subres);
		}
		#endif
		ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].IsCPE = atoi(mysql_row[13]);
		ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].OnlineTimeStamp = atol(mysql_row[14]);
		ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].WTP_PORT_CONTROL = atoi(mysql_row[15]);
		ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].WTP_PORT_DATA = atoi(mysql_row[16]);
		ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].CWDiscoveryCount = atoi(mysql_row[17]);
		ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].WTPSocket = atoi(mysql_row[18]);
		ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].WTPDataSocket = atoi(mysql_row[19]);
		sprintf(sqlcmd, "update AP_Table set OnlineTimeStamp = '%ld' where `Index` = '%d'", time_stamp_for_getRun, ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].ApIndex);
		affect_line = MySqlExecQuery_NoResult(mysql, sqlcmd);
		if (affect_line != 1) 
		{fprintf(stderr,"%sError Sql set AP:%d OnlineTimeStamp%s affect_line:%d\n", COLOR_YELLOW, ap_run_table[ap_run_count % MAX_AP_TABLE_COUNT].ApIndex, COLOR_END, affect_line);}
		ap_run_count++;
		if (ap_run_count / MAX_AP_TABLE_COUNT == 10000 && ap_run_count % MAX_AP_TABLE_COUNT == 0) {ap_run_count = 0;}
 		//usleep(30);//0.03ms
 		if (FastSend)
		{
			pthread_cond_signal(&run_send_cond); 
		}
 		sched_yield();
	}
	// 释放
	mysql_free_result(mysql_res);
}

inline void SimulatorWTPEnterRun(AP_TABLE * cur_AP)
{
	char sqlcmd[200];
	time_t time_stamp_for_enterRun;
	my_ulonglong affect_line = 0;
	time(&time_stamp_for_enterRun);
	sprintf(sqlcmd, "update AP_Table set State = '%d',OnlineTimeStamp = '%ld',WTP_PORT_CONTROL = '%d',WTP_PORT_DATA = '%d',WTPSocket = '%d',WTPDataSocket = '%d' where `Index` = '%d'", 
					CW_ENTER_RUN,
					time_stamp_for_enterRun,
					cur_AP->WTP_PORT_CONTROL,
					cur_AP->WTP_PORT_DATA,
					cur_AP->WTPSocket,
					cur_AP->WTPDataSocket,
					cur_AP->ApIndex);
	affect_line = MySqlExecQuery_NoResult(&mysql_thread_pool[cur_AP->Epoll_fd_Index], sqlcmd);
	if (affect_line != 1) 
	{fprintf(stderr,"%sError Sql set AP:%d state RUN%s affect_line:%d\n", COLOR_YELLOW, cur_AP->ApIndex, COLOR_END, affect_line);}
	Epoll_Del_Socket(cur_AP);
	Epoll_fd_State[cur_AP->Epoll_fd_Index] = 0;
	memset(cur_AP, 0, sizeof(AP_TABLE));
	if (FastSend)
	{
		pthread_cond_signal(&upline_fill_cond); 
	}
}

inline void SimulatorWTPQuit(AP_TABLE * cur_AP)
{
	char sqlcmd[200];
	my_ulonglong affect_line = 0;
	sprintf(sqlcmd, "update AP_Table set State = '%d',WTP_PORT_CONTROL = '%d',WTP_PORT_DATA = '%d',WTPSocket = '%d',WTPDataSocket = '%d' where `Index` = '%d'", 
					CW_ENTER_SULKING,
					cur_AP->WTP_PORT_CONTROL,
					cur_AP->WTP_PORT_DATA,
					cur_AP->WTPSocket,
					cur_AP->WTPDataSocket,
					cur_AP->ApIndex);
	affect_line = MySqlExecQuery_NoResult(&mysql_thread_pool[cur_AP->Epoll_fd_Index], sqlcmd);
	if (affect_line != 1) 
	{fprintf(stderr,"%sError Sql set AP:%d state QUIT%s affect_line:%d\n", COLOR_YELLOW, cur_AP->ApIndex, COLOR_END, affect_line);}
	Epoll_Del_Socket(cur_AP);
	Epoll_fd_State[cur_AP->Epoll_fd_Index] = 0;
	memset(cur_AP, 0, sizeof(AP_TABLE));
	if (FastSend)
	{
		pthread_cond_signal(&upline_fill_cond); 
	}
}

inline void SimulatorWTPReupline(AP_TABLE * cur_AP)
{
	char sqlcmd[200];
	my_ulonglong affect_line = 0;
	sprintf(sqlcmd, "update AP_Table set State = '%d' where `Index` = '%d'", 
					CW_ENTER_SULKING,
					cur_AP->ApIndex);
	affect_line = MySqlExecQuery_NoResult(&mysql_thread_pool[cur_AP->Epoll_fd_Index], sqlcmd);
	if (affect_line != 1) 
	{fprintf(stderr,"%sError Sql set AP:%d state Reupline%s affect_line:%d\n", COLOR_YELLOW, cur_AP->ApIndex, COLOR_END, affect_line);}
	else
	{fprintf(stderr,"%sApIndex:%d WTPSocket:%d "MACSTR" Reupline%s\n",COLOR_PURPLE,cur_AP->ApIndex,cur_AP->WTPSocket,MAC2STR(cur_AP->Mac),COLOR_END);}
	RunEpoll_Del_Socket(cur_AP->WTPSocket);
	Epoll_fd_State[cur_AP->Epoll_fd_Index] = 0;
	memset(&SocketIn[cur_AP->WTPSocket], 0, sizeof(SocketInInfo));
	memset(cur_AP, 0, sizeof(AP_TABLE));
	if (FastSend)
	{
		pthread_cond_signal(&run_fill_cond); 
	}
}


pthread_mutex_t get_Epoll_fd_mutex;
void* uplineprocess (void *arg)  
{
	unsigned int APIndex = (unsigned int *) arg;
	AP_TABLE * cur_AP = &ap_upline_table[APIndex];	
    //fprintf(stderr,"threadid is 0x%x, AP %d Start Upline\n", pthread_self (),cur_AP->ApIndex); 
	//分配空闲的Epoll_fd和mysql连接
	int i = 0;
	retry_get_Epoll_fd:
	CWThreadMutexLock(&get_Epoll_fd_mutex);
	for (i = 0; i < WORK_THREAD_NUM * THREAD_POOL_COUNT; i++)
	{	
		if (Epoll_fd_State[i] == 0)
		{
			cur_AP->Epoll_fd_Index = i;
			Epoll_fd_State[i] = 1;
			break;
		}	
	}
	CWThreadMutexUnlock(&get_Epoll_fd_mutex);
	if (i == WORK_THREAD_NUM * THREAD_POOL_COUNT) { fprintf(stderr,"%sthreadid is 0x%x, AP %d Get Epoll_fd Fail%s\n", COLOR_YELLOW, pthread_self (),cur_AP->ApIndex, COLOR_END); mysleep(1);goto retry_get_Epoll_fd;}	

	
	while(1) 
	{
		switch(cur_AP->State) {
			case CW_ENTER_DISCOVERY:
				//fprintf(stderr,"threadid is 0x%x, AP %d Enter Discovery\n", pthread_self (),cur_AP->ApIndex); 
				cur_AP->State = CWWTPEnterDiscovery(cur_AP);
				break;
			case CW_ENTER_SULKING:
				//fprintf(stderr,"threadid is 0x%x, AP %d Enter Sulking\n", pthread_self (),cur_AP->ApIndex); 
				cur_AP->State = CWWTPEnterSulking(cur_AP);
				break;
			case CW_ENTER_JOIN:
				//fprintf(stderr,"threadid is 0x%x, AP %d Enter Join\n", pthread_self (),cur_AP->ApIndex);
				cur_AP->State = CWWTPEnterJoin(cur_AP);
				break;
			case CW_ENTER_CONFIGURE:
				//fprintf(stderr,"threadid is 0x%x, AP %d Enter Configure\n", pthread_self (),cur_AP->ApIndex); 
				cur_AP->State = CWWTPEnterConfigure(cur_AP);
				break;
			case CW_ENTER_DATA_CHECK:
				//fprintf(stderr,"threadid is 0x%x, AP %d Enter Data_Check\n", pthread_self (),cur_AP->ApIndex); 
				cur_AP->State = CWWTPEnterDataCheck(cur_AP);
				break;	
			case CW_ENTER_RUN:
				//fprintf(stderr,"threadid is 0x%x, AP %d Enter Run\n", pthread_self (),cur_AP->ApIndex); 
				//cur_AP->State = CWWTPEnterRun();
				//Write Run State and Quit Thread
				SimulatorWTPEnterRun(cur_AP);
				return;
			case CW_ENTER_RESET:
				//fprintf(stderr,"threadid is 0x%x, AP %d Enter Reset\n", pthread_self (),cur_AP->ApIndex); 
				//CWLog("------ Enter Reset State ------");
				cur_AP->State = CW_ENTER_DISCOVERY;
				break;
			case CW_QUIT:
				SimulatorWTPQuit(cur_AP);
				//fprintf(stderr,"threadid is 0x%x, AP %d Enter Quit\n", pthread_self (),cur_AP->ApIndex); 
				//CWWTPDestroy(cur_AP);
				return 0;
		}
	}
    return NULL;  
} 

int waitSocketReadTime = 100;
pthread_mutex_t Set_waitSocketReadTime_mutex;
int HasWTPEvent = 1;//ycc config
int HasEcho = 1;//ycc config
#define DEFAULT_MAX_RUN_WAITCOUNT 5
int Max_Run_WaitCount = DEFAULT_MAX_RUN_WAITCOUNT; //ycc config
void* runprocess (void *arg)  
{
	unsigned int APIndex = (unsigned int *) arg;
	AP_TABLE * cur_AP = &ap_run_table[APIndex];
	int WaitCount = 0;
	//fprintf(stderr,"threadid is 0x%x, AP %d Start Run\n", pthread_self (),cur_AP->ApIndex); 
	if(HasCheckEchoRespone)
	{
		#if 1
		//分配空闲的Epoll_fd和mysql连接
		int i = 0;
		retry_get_Epoll_fd:
		CWThreadMutexLock(&get_Epoll_fd_mutex);
		for (i = 0; i < WORK_THREAD_NUM * THREAD_POOL_COUNT; i++)
		{	
			if (Epoll_fd_State[i] == 0)
			{
				cur_AP->Epoll_fd_Index = i;
				Epoll_fd_State[i] = 1;
				break;
			}	
		}
		CWThreadMutexUnlock(&get_Epoll_fd_mutex);
		if (i == WORK_THREAD_NUM * THREAD_POOL_COUNT) { fprintf(stderr,"%sthreadid is 0x%x, AP %d Get Epoll_fd Fail%s\n", COLOR_YELLOW, pthread_self (),cur_AP->ApIndex, COLOR_END); mysleep(1);goto retry_get_Epoll_fd;}	
		#endif
	}
	while (cur_AP->WTPSocket == 0) 
	{
		WaitCount++;
		usleep(waitSocketReadTime);
		if (cur_AP->WTPSocket == 0)
		{
			fprintf(stderr,"%sApIndex:%d WTPSocket:%d waitSocketReadTime:%dus%s\n",COLOR_GREEN,cur_AP->ApIndex,cur_AP->WTPSocket,waitSocketReadTime,COLOR_END);
			CWThreadMutexLock(&Set_waitSocketReadTime_mutex);
			waitSocketReadTime += 100;
			CWThreadMutexUnlock(&Set_waitSocketReadTime_mutex);
		}
		if (WaitCount >= Max_Run_WaitCount) break;
	}
	if(HasCheckEchoRespone)
	{
		if(cur_AP->WTPSocket != 0 && SocketIn[cur_AP->WTPSocket].SocketInFlag == 0)
		{
			if(SocketIn[cur_AP->WTPSocket].SocketLife == 0)//first in
			{
				RunEpoll_Add_Socket(cur_AP->WTPSocket);
				SocketIn[cur_AP->WTPSocket].SocketLife++;
			}
			else if(SocketIn[cur_AP->WTPSocket].SocketLife < Max_Run_WaitCount)//not recv echo response one more chance
			{
				SocketIn[cur_AP->WTPSocket].SocketLife++;
			}
			else//not recv echo response twice set sulk for reupline
			{
				SimulatorWTPReupline(cur_AP);
				return;
			}
		}
		else//recv echo response
		{
			SocketIn[cur_AP->WTPSocket].SocketInFlag = 0;
			SocketIn[cur_AP->WTPSocket].SocketLife = 1;
		}
	}
	if (HasEcho)
	{
		CWSimulatorSendEchoRequest(cur_AP);
	}
	if (HasWTPEvent)
	{
		CWSimulatorSendSTAWIFIInfo(cur_AP);
	}
	if(HasCheckEchoRespone)
	{
		#if 1
		Epoll_fd_State[cur_AP->Epoll_fd_Index] = 0;
		#endif
	}
	memset(cur_AP, 0, sizeof(AP_TABLE));
	if (FastSend)
	{
		pthread_cond_signal(&run_fill_cond);
	}
}

void* SimulatorGetApUplineInfo(void *arg)
{
	MySqlInit(&mysql_upline);
	ZeroAPState(&mysql_upline);
    pthread_mutex_init(&upline_fill_mtx, NULL);
    pthread_cond_init(&upline_fill_cond, NULL);
	while(1) 
	{
		FillApUplineTable(&mysql_upline);
		if (SimulatorQuit)
		{
			//break;
		}
		mysleep(4);
	}
	MySqlClose(&mysql_upline);
#if 0
	char tempMac[3][18];
	char tempIp[1][16];
	int i,j,k;
	for (i = 0; i < ap_upline_count; i++)
	{
		Mac2Str(ap_upline_table[i % MAX_AP_TABLE_COUNT].Mac, tempMac[0]);
		Mac2Str(ap_upline_table[i % MAX_AP_TABLE_COUNT].ConnectRadioMac, tempMac[1]);
		fprintf(stderr,"Ap%d_Info:\n",i);
		fprintf(stderr,"%d|%s|%s|%s|%s|%s|%s|%d|%d|%s|%d\n", 
								ap_upline_table[i % MAX_AP_TABLE_COUNT].ApIndex,
								ap_upline_table[i % MAX_AP_TABLE_COUNT].Model,
								tempMac[0],
								ap_upline_table[i % MAX_AP_TABLE_COUNT].WTPname,
								ap_upline_table[i % MAX_AP_TABLE_COUNT].HwVersion,
								ap_upline_table[i % MAX_AP_TABLE_COUNT].SwVersion,
								ap_upline_table[i % MAX_AP_TABLE_COUNT].BootVersion,
								ap_upline_table[i % MAX_AP_TABLE_COUNT].State,
								ap_upline_table[i % MAX_AP_TABLE_COUNT].RadioCount,
								tempMac[1],
								ap_upline_table[i % MAX_AP_TABLE_COUNT].StaCount);
		for (j = 0; j < ap_upline_table[i % MAX_AP_TABLE_COUNT].RadioCount; j++)
		{
			Mac2Str(ap_upline_table[i % MAX_AP_TABLE_COUNT].Radio_Info[j].Bssid, tempMac[2]);
			fprintf(stderr,"Ap%d_Radio%dInfo:\n",i,j);
			fprintf(stderr,"%s|%s|%d\n",
				ap_upline_table[i % MAX_AP_TABLE_COUNT].Radio_Info[j].PhyName,
				tempMac[2],
				ap_upline_table[i % MAX_AP_TABLE_COUNT].Radio_Info[j].IsCPE);
		}
		for (k = 0; k < ap_upline_table[i % MAX_AP_TABLE_COUNT].StaCount; k++)
		{
			Int2Ip(ap_upline_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].Ip, tempIp[0]);
			Mac2Str(ap_upline_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].Mac, tempMac[2]);
			fprintf(stderr,"Ap%d_Sta%dInfo:\n",i,k);
			fprintf(stderr,"%s|%s|%s|%d|%s|%s|%d|%d|%d|%d|%d|%d|%d\n",
				tempIp[0],
				tempMac[2],
				ap_upline_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].Type,
				ap_upline_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].Rssi,
				ap_upline_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].Ssid,
				ap_upline_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].WireMode,
				ap_upline_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].OnlineTime,
				ap_upline_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].RadioId,
				ap_upline_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].WlanId,
				ap_upline_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].TxDataRate,
				ap_upline_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].RxDataRate,
				ap_upline_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].TxFlow,
				ap_upline_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].RxFlow);
		}
	}
#endif
}

int ApUplineInterval = 0;
void* SimulatorApUpline(void *arg)
{
    pthread_mutex_init(&upline_send_mtx, NULL);
    pthread_cond_init(&upline_send_cond, NULL);
    while(1)
    {  
		if (ap_upline_table[ap_upline_act_count % MAX_AP_TABLE_COUNT].Model[0] == '\0')
		{
			//fprintf(stderr,"No Ap Start Upline\n"); 
			if (!FastSend)
			{
				usleep(30000);//30ms
			}
			else
			{
				pthread_mutex_lock(&upline_send_mtx);
				pthread_cond_wait(&upline_send_cond, &upline_send_mtx);
				pthread_mutex_unlock(&upline_send_mtx);				
			}
		}
		else
		{
			if (!(ap_upline_act_count % 1024))
			{
				fprintf(stderr,"%sAp%d Start Upline%s\n", COLOR_GREEN, ap_upline_act_count, COLOR_END);
			}
			ThreadArgs pool_args = {0};
			int uplineindex = ap_upline_act_count % MAX_AP_TABLE_COUNT;
			pool_args.arg1 = threadpool[UplineThreadPool];
			pool_args.arg2 = uplineindex;
        	pool_add_worker(uplineprocess, &pool_args);
			//break;//test ycc add only 1 for test
			ap_upline_act_count++;
			if (ap_upline_act_count / MAX_AP_TABLE_COUNT == 100 && ap_upline_act_count % MAX_AP_TABLE_COUNT == 0) {ap_upline_act_count = 0;}
			if (ApUplineSendInterval != 0)
			{
				usleep(ApUplineSendInterval);
			}
			else
			{
				sched_yield();
			}
			if (ApUplineInterval != 0)
			{
				usleep(ApUplineInterval * 1000);//ms
			}
		}
    }  
}

void* SimulatorGetApRunInfo(void *arg)
{
	mysleep(EchoInterval/2);//Wait Ap upline
 	MySqlInit(&mysql_run);
    pthread_mutex_init(&run_fill_mtx, NULL);
    pthread_cond_init(&run_fill_cond, NULL);
 	while(1) 
	{
 		FillApRunTable(&mysql_run);
 		if (SimulatorQuit)
		{
			//break;
		}
		mysleep(1);
 	}
	MySqlClose(&mysql_run);
#if 0
	char tempMac[3][18];
	char tempIp[1][16];
	int i,j,k;
	for (i = 0; i < ap_run_count; i++)
	{
		Mac2Str(ap_run_table[i % MAX_AP_TABLE_COUNT].Mac, tempMac[0]);
		Mac2Str(ap_run_table[i % MAX_AP_TABLE_COUNT].ConnectRadioMac, tempMac[1]);
		fprintf(stderr,"Ap%d_Info:\n",i);
		fprintf(stderr,"%d|%s|%s|%s|%s|%s|%s|%d|%d|%s|%d\n", 
								ap_run_table[i % MAX_AP_TABLE_COUNT].ApIndex,
								ap_run_table[i % MAX_AP_TABLE_COUNT].Model,
								tempMac[0],
								ap_run_table[i % MAX_AP_TABLE_COUNT].WTPname,
								ap_run_table[i % MAX_AP_TABLE_COUNT].HwVersion,
								ap_run_table[i % MAX_AP_TABLE_COUNT].SwVersion,
								ap_run_table[i % MAX_AP_TABLE_COUNT].BootVersion,
								ap_run_table[i % MAX_AP_TABLE_COUNT].State,
								ap_run_table[i % MAX_AP_TABLE_COUNT].RadioCount,
								tempMac[1],
								ap_run_table[i % MAX_AP_TABLE_COUNT].StaCount);
		for (j = 0; j < ap_run_table[i % MAX_AP_TABLE_COUNT].RadioCount; j++)
		{
			Mac2Str(ap_run_table[i % MAX_AP_TABLE_COUNT].Radio_Info[j].Bssid, tempMac[2]);
			fprintf(stderr,"Ap%d_Radio%dInfo:\n",i,j);
			fprintf(stderr,"%s|%s|%d\n",
				ap_run_table[i % MAX_AP_TABLE_COUNT].Radio_Info[j].PhyName,
				tempMac[2],
				ap_run_table[i % MAX_AP_TABLE_COUNT].Radio_Info[j].IsCPE);
		}
		for (k = 0; k < ap_run_table[i % MAX_AP_TABLE_COUNT].StaCount; k++)
		{
			Int2Ip(ap_run_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].Ip, tempIp[0]);
			Mac2Str(ap_run_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].Mac, tempMac[2]);
			fprintf(stderr,"Ap%d_Sta%dInfo:\n",i,k);
			fprintf(stderr,"%s|%s|%s|%d|%s|%s|%d|%d|%d|%d|%d|%d|%d\n",
				tempIp[0],
				tempMac[2],
				ap_run_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].Type,
				ap_run_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].Rssi,
				ap_run_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].Ssid,
				ap_run_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].WireMode,
				ap_run_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].OnlineTime,
				ap_run_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].RadioId,
				ap_run_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].WlanId,
				ap_run_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].TxDataRate,
				ap_run_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].RxDataRate,
				ap_run_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].TxFlow,
				ap_run_table[i % MAX_AP_TABLE_COUNT].Sta_Info[k].RxFlow);
		}
	}
#endif
}
void* SimulatorApRun(void *arg)
{
	mysleep(EchoInterval/2);//Wait Ap upline
    pthread_mutex_init(&run_send_mtx, NULL);
    pthread_cond_init(&run_send_cond, NULL);
	while(1)
    {  
		if (ap_run_table[ap_run_act_count % MAX_AP_TABLE_COUNT].Model[0] == '\0')
		{
			//fprintf(stderr,"No Ap Start Run\n"); 
			if (!FastSend)
			{
				usleep(10000);//10ms
			}
			else
			{
				pthread_mutex_lock(&run_send_mtx);
				pthread_cond_wait(&run_send_cond, &run_send_mtx);
				pthread_mutex_unlock(&run_send_mtx);		
			}
		}
		else
		{
			if (!(ap_run_act_count % 1024))
			{
				fprintf(stderr,"%sAp%d Start Run%s\n", COLOR_GREEN, ap_run_act_count, COLOR_END);
			}
			if (!FastSend)
			{
				ThreadArgs pool_args = {0};
				int runindex = ap_run_act_count % MAX_AP_TABLE_COUNT;
				pool_args.arg1 = threadpool[UplineThreadPool];
				pool_args.arg2 = runindex;
        		pool_add_worker(runprocess, &pool_args);
			}
			else
			{
				ThreadArgs pool_args = {0};
				int runindex = ap_run_act_count % MAX_AP_TABLE_COUNT;
				pool_args.arg1 = threadpool[RunThreadPool];
				pool_args.arg2 = runindex;
        		pool_add_worker(runprocess, &pool_args);
			}
			//break;//test ycc add only 1 for test
			ap_run_act_count++;
			if (ap_run_act_count / MAX_AP_TABLE_COUNT == 100 && ap_run_act_count % MAX_AP_TABLE_COUNT == 0) {ap_run_act_count = 0;}
			//usleep(30);//0.03ms
			sched_yield();
		}
    }  
}

void* recvpacket (void *arg)
{
	int buflen = 0;
	int recvlen = 0;
	int fd = (int *) arg;
	socklen_t getnumlen = 4;
	if(0 != getsockopt(fd,SOL_SOCKET,SO_RCVBUF,&buflen,&getnumlen))
	{
		fprintf(stderr,"%s%s:%d %s%s\n",COLOR_RED,__func__,__LINE__,strerror(errno),COLOR_END);
		return;
	}
	char * recvBuf = (char *)alloca(buflen);
	memset(recvBuf, 0, buflen);
	recvlen = recv(fd, (char *)recvBuf, buflen, 0);//need addr use recvfrom
	//fprintf(stderr,"recvBuf:%s recvlen:%d \n",recvBuf,recvlen);
}

void* SimulatorListeningEpoll(void *arg)
{
	if(!HasCheckEchoRespone) return;
	int nfds, i;
	struct epoll_event listen_event[MAX_LINUX_SOCKET] = {0};
	ListeningEpoll = epoll_create(MAX_LINUX_SOCKET + 1);
	if(ListeningEpoll < 0)
	{
		fprintf(stderr,"%s%s %d epoll_create fail!%s\n",COLOR_RED,__func__,__LINE__,COLOR_END);
		return;
	}
	mysleep(EchoInterval/2);//Wait Ap run
	while(1)
	{
		//fprintf(stderr,"%s %d cur_AP->sock:%d Epoll_fd:%d\n",__func__,__LINE__,sock,Epoll_fd);//ycc test
		nfds = epoll_wait(ListeningEpoll,listen_event,1,-1);
		if(nfds < 0)
		{
			fprintf(stderr,"Epoll_fd:%d %s %d %sCW_ERROR_GENERAL%s\n",ListeningEpoll,__func__,__LINE__,COLOR_YELLOW,COLOR_END);
			continue;//return -1;//CW_ERROR_GENERAL;
		}
		else if(nfds == 0)
		{
			fprintf(stderr,"Epoll_fd:%d %s %d %sCW_ERROR_TIME_EXPIRED%s\n",ListeningEpoll,__func__,__LINE__,COLOR_YELLOW,COLOR_END);
			continue;//return 1;//CW_ERROR_TIME_EXPIRED;
		}
		else
		{
			for (i = 0; i < nfds; i++)
			{
				if(listen_event[i].events & EPOLLIN)
				{
					//fprintf(stderr,"%s %d listen_event[i].data.fd:%d\n",__func__,__LINE__,listen_event[i].data.fd);
					SocketIn[listen_event[i].data.fd].SocketInFlag = 1;
					ThreadArgs pool_args = {0};
					pool_args.arg1 = threadpool[UplineThreadPool];
					pool_args.arg2 = listen_event[i].data.fd;
					pool_add_worker(recvpacket, &pool_args);//recv
					continue;//return 0;//CW_ERROR_SUCCESS;
				}
			}
			//fprintf(stderr,"ap sock:%d Epoll_fd:%d %s %d %scontinue%s\n",sock,ListeningEpoll,__func__,__LINE__,COLOR_YELLOW,COLOR_END);
			continue;
		}
	}
}
