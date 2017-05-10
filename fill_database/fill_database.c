#include <mysql.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "fill_database.h"
#include "cfg.h"

void show_usage()
{
	printf("----------------------------------------------------------------------------------------------------------------------------\n");
	printf("CreateComm ycc make, use for fill ap_simulator mysql database\n");
	printf("usage:\n");
	printf("\tfill_database [option] {[values].....}\n");
	printf("option:\n");
	printf("\tbuild \t\t\t\t\t\t\t:init ap_simulator's Tables {AP_Table Sta_Table Radio_Table}\n");
	printf("\taddap [ap_productinfo] [apcount] [stacount_per_ap] \t:insert ap info into AP_Table\n");
	printf("\taddsta [sta_productinfo] [stacount] \t\t\t:insert sta info into Sta_Table\n");
	printf("\taddradio [radio_productinfo] [radiocount] \t\t:insert radio info into Radio_Table\n");
	printf("\tclearap \t\t\t\t\t\t:free AP_Table\n");
	printf("\tclearsta \t\t\t\t\t\t:free Sta_Table\n");
	printf("\tclearradio \t\t\t\t\t\t:free Radio_Table\n");
	printf("\t0apstate \t\t\t\t\t\t:clear Ap State and WTPSocket\n");
	printf("\taddfile [sql_file] \t\t\t\t\t:insert sql info by sqlfile\n");
	printf("----------------------------------------------------------------------------------------------------------------------------\n");
}
MYSQL *mysql = NULL;
char commond[16] = {0};
inline int MySqlInit(MYSQL **mysql)
{
	char mysqladdr[16] = {0};
	char mysqluser[16] = {0};
	char mysqlpwd[16] = {0};
	char mysqldatabase[16] = {0};
	//config
	Cfg *m = CfgNew("database_config");
	strcpy(mysqladdr, GetValByKey("Mysql","addr",m));
	strcpy(mysqluser, GetValByKey("Mysql","user",m));
	strcpy(mysqlpwd, GetValByKey("Mysql","pwd",m));
	strcpy(mysqldatabase, GetValByKey("Mysql","database",m));
	CfgFree(m);
	//sql
	*mysql = mysql_init(NULL);
	if (!*mysql) {
		printf("%s %d mysql_errno:%d \nmysql_error:%s\n",
					__func__,__LINE__,mysql_errno(*mysql),mysql_error(*mysql));
		return 1;
	}
	*mysql = mysql_real_connect(*mysql, mysqladdr, mysqluser, mysqlpwd, mysqldatabase, 0, NULL, 0);
	if (!*mysql) {
		if (mysql_errno(*mysql) == 0 && strcmp(commond,"build") == 0)
		{
			*mysql = mysql_init(NULL);
			if (!*mysql) {
				printf("%s %d mysql_errno:%d \nmysql_error:%s\n",
							__func__,__LINE__,mysql_errno(*mysql),mysql_error(*mysql));
				return 1;
			}
			*mysql = mysql_real_connect(*mysql, mysqladdr, mysqluser, mysqlpwd, NULL, 0, NULL, 0);
			char sqlcmd[256] = {0};
			sprintf(sqlcmd,"create database %s",mysqldatabase);
			if (mysql_query(*mysql, sqlcmd)) 
			{
		      printf("%s %d mysql_errno %u: %s\n",__func__,__LINE__, mysql_errno(*mysql), mysql_error(*mysql));
		      return 1;
		  	}
			sprintf(sqlcmd,"use %s",mysqldatabase);
			if (mysql_query(*mysql, sqlcmd)) 
			{
		      printf("%s %d mysql_errno %u: %s\n",__func__,__LINE__, mysql_errno(*mysql), mysql_error(*mysql));
		      return 1;
		  	}
		}
		else
		{
			printf("%s %d mysql_errno:%d \nmysql_error:%s\n",
						__func__,__LINE__,mysql_errno(*mysql),mysql_error(*mysql));
			return 1;
		}
	}
	mysql_set_character_set(*mysql, "utf8");
}

inline MYSQL_RES * MySqlExecQuery(MYSQL **mysql, char *sqlcmd)
{
	MYSQL_RES *mysql_res;
	int r;
	r = mysql_real_query(*mysql, sqlcmd, strlen(sqlcmd));
	if (r) {printf("%s %d error:%d mysql_errno:%d \nmysql_error:%s\n",
					__func__,__LINE__,r,mysql_errno(*mysql),mysql_error(*mysql));return NULL;}
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
	r = mysql_real_query(*mysql, sqlcmd, strlen(sqlcmd));
	if (r) {printf("%s %d error:%d mysql_errno:%d \nmysql_error:%s\n",
					__func__,__LINE__,r,mysql_errno(*mysql),mysql_error(*mysql));return 0;}
	return mysql_affected_rows(*mysql);
}

inline int MySqlClose(MYSQL **mysql)
{
	mysql_close(*mysql);
	return 0;
}

int IsNum(const char *p) 
{
    if(p == NULL) 
        return 0;
    else {
        while(*p != '\0') {
            if(*p <= '9' && *p++ >= '0')
                continue;
            else 
                return 0;
        }
    }
    return 1;
}

/*将str1字符串中第一次出现的str2字符串替换成str3*/  
void replaceFirst(char *str1,char *str2,char *str3)  
{  
    char str4[strlen(str1)+1];  
    char *p;  
    strcpy(str4,str1);  
    if((p=strstr(str1,str2))!=NULL)/*p指向str2在str1中第一次出现的位置*/  
    {  
        while(str1!=p&&str1!=NULL)/*将str1指针移动到p的位置*/  
        {  
            str1++;  
        }  
        str1[0]='\0';/*将str1指针指向的值变成/0,以此来截断str1,舍弃str2及以后的内容，只保留str2以前的内容*/  
        strcat(str1,str3);/*在str1后拼接上str3,组成新str1*/  
        strcat(str1,strstr(str4,str2)+strlen(str2));/*strstr(str4,str2)是指向str2及以后的内容(包括str2),strstr(str4,str2)+strlen(str2)就是将指针向前移动strlen(str2)位，跳过str2*/  
    }  
}  

void split(char *src, const char *separator, char **dest, int *num)
{
    char *pNext;
    int count = 0;
    
    if (src == NULL || strlen(src) == 0) return;
    if (separator == NULL || strlen(separator) == 0) return; 

    //pNext = strtok(src,separator);
    pNext = strsep(&src,separator);
    
    while(pNext != NULL)
    {
        *dest++ = pNext;
        ++count;
        //pNext = strtok(NULL,separator);
        pNext = strsep(&src,separator);
    }

    *num = count;
}

void Trim(char* s, char c)
{
    char *t  = s;
    while (*s == c){s++;};
    if (*s)
    {
        char* t1 = s;
        while (*s){s++;};
        s--;
        while (*s == c){s--;};
        while (t1 <= s)
        {
            *(t++) = *(t1++);
        }
    }
    *t = 0;
}

int randseed = 0;
int Rand()
{
	srand(randseed++);
	return rand();
}

void build_database()
{
	char sqlcmd[4096];
	my_ulonglong affect_line = 0;
	MySqlInit(&mysql);
	//AP_Table
	sprintf(sqlcmd, "drop table `AP_Table`");
	MySqlExecQuery_NoResult(&mysql, sqlcmd);
	sprintf(sqlcmd, "CREATE TABLE `AP_Table` ("
					" `Index`  int(10) UNSIGNED NOT NULL AUTO_INCREMENT ,"
					" `Model`  varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NULL DEFAULT NULL ,"
					" `Mac`  varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NULL DEFAULT NULL ,"
					" `WTPname`  varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NULL DEFAULT NULL ,"
					" `Hwversion`  varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NULL DEFAULT NULL ,"
					" `Swversion`  varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NULL DEFAULT NULL ,"
					" `BootVersion`  varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NULL DEFAULT NULL ,"
					" `State`  tinyint(3) UNSIGNED NOT NULL DEFAULT 0 ,"
					" `RadioCount`  tinyint(3) UNSIGNED NOT NULL DEFAULT 0 ,"
					" `RadioInfoIndex`  varchar(32) CHARACTER SET utf8 COLLATE utf8_general_ci NULL DEFAULT NULL ,"
					" `ConnectRadioIndex`  varchar(32) CHARACTER SET utf8 COLLATE utf8_general_ci NULL DEFAULT NULL ,"
					" `StaCount`  mediumint(8) UNSIGNED NOT NULL DEFAULT 0 ,"
					" `StaIndex`  varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NULL DEFAULT '0' ,"
					" `IsCPE`  tinyint(3) UNSIGNED NOT NULL DEFAULT 0 ,"
					" `OnlineTimeStamp`  bigint(20) UNSIGNED NOT NULL DEFAULT 0 ,"
					" `WTP_PORT_CONTROL`  int(10) UNSIGNED NOT NULL DEFAULT 0 ,"
					" `WTP_PORT_DATA`  int(10) UNSIGNED NOT NULL DEFAULT 0 ,"
					" `CWDiscoveryCount`  int(10) UNSIGNED NOT NULL DEFAULT 0 ,"
					" `WTPSocket`  int(10) UNSIGNED NOT NULL DEFAULT 0 ,"
					" `WTPDataSocket`  int(10) UNSIGNED NOT NULL DEFAULT 0 ,"
					" PRIMARY KEY (`Index`),"
					" INDEX `Index` (`Index`) USING BTREE "
					" )"
					" ENGINE=InnoDB"
					" DEFAULT CHARACTER SET=utf8 COLLATE=utf8_general_ci"
					" ROW_FORMAT=DYNAMIC"
					" ;"
					" ");
	MySqlExecQuery_NoResult(&mysql, sqlcmd);
	//Radio_Table
	sprintf(sqlcmd, "drop table `Radio_Table`");
	MySqlExecQuery_NoResult(&mysql, sqlcmd);
	sprintf(sqlcmd, "CREATE TABLE `Radio_Table` ("
					" `Index`  int(10) UNSIGNED NOT NULL AUTO_INCREMENT ,"
					" `PhyName`  varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NULL DEFAULT NULL ,"
					" `Bssid`  varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NULL DEFAULT NULL ,"
					" PRIMARY KEY (`Index`),"
					" INDEX `Index` (`Index`) USING BTREE "
					" )"
					" ENGINE=InnoDB"
					" DEFAULT CHARACTER SET=utf8 COLLATE=utf8_general_ci"
					" AUTO_INCREMENT=3"
					" ROW_FORMAT=DYNAMIC"
					" ;"
					" "
					" ");
	MySqlExecQuery_NoResult(&mysql, sqlcmd);
	//Sta_Table
	sprintf(sqlcmd, "drop table `Sta_Table`");
	MySqlExecQuery_NoResult(&mysql, sqlcmd);
	sprintf(sqlcmd, "CREATE TABLE `Sta_Table` ("
					" `Index`  int(10) UNSIGNED NOT NULL AUTO_INCREMENT ,"
					" `IP`  varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NULL DEFAULT NULL ,"
					" `Mac`  varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NULL DEFAULT NULL ,"
					" `Type`  varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NULL DEFAULT NULL ,"
					" `Rssi`  char(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '0' ,"
					" `Ssid`  varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NULL DEFAULT NULL ,"
					" `WireMode`  varchar(255) CHARACTER SET utf8 COLLATE utf8_general_ci NULL DEFAULT NULL ,"
					" `OnlineTime`  bigint(20) UNSIGNED NOT NULL DEFAULT 0 ,"
					" `RadioId`  tinyint(3) UNSIGNED NOT NULL DEFAULT 0 ,"
					" `WlanId`  tinyint(3) UNSIGNED NOT NULL DEFAULT 0 ,"
					" `TxDataRate`  char(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '0' ,"
					" `RxDataRate`  char(255) CHARACTER SET utf8 COLLATE utf8_general_ci NOT NULL DEFAULT '0' ,"
					" `TxFlow`  bigint(20) UNSIGNED NOT NULL DEFAULT 0 ,"
					" `RxFlow`  bigint(20) UNSIGNED NOT NULL DEFAULT 0 ,"
					" PRIMARY KEY (`Index`),"
					" INDEX `Index` (`Index`) USING BTREE "
					" )"
					" ENGINE=InnoDB"
					" DEFAULT CHARACTER SET=utf8 COLLATE=utf8_general_ci"
					" AUTO_INCREMENT=3"
					" ROW_FORMAT=DYNAMIC"
					" ;"
					" "
					" ");
	MySqlExecQuery_NoResult(&mysql, sqlcmd);
	MySqlClose(&mysql);
}

inline void ZeroAPState()
{
	char sqlcmd[200];
	my_ulonglong affect_line = 0;
	MySqlInit(&mysql);
	sprintf(sqlcmd, "update AP_Table set State = 0, WTPSocket = 0");
	fprintf(stderr,"Sql ZeroAPState affect_line:%ld\n", MySqlExecQuery_NoResult(&mysql, sqlcmd));
	MySqlClose(&mysql);
}

inline char * GetMac(char * MacArray, int MacArraySize, char * MacHead, int MacIndex)
{
	//MacHead->xx:xx:xx:
	memset(MacArray, 0, MacArraySize);
	sprintf(MacArray,"%s%02hhx:%02hhx:%02hhx", MacHead, C_FF0000(MacIndex), C_00FF00(MacIndex), C_0000FF(MacIndex));
	return MacArray;
}

inline char * GetIP(char * IPArray, int IPArraySize, char * IPHead, int IPIndex)
{
	//IPHead->192.168.
	memset(IPArray, 0, IPArraySize);
	sprintf(IPArray,"%s%d.%d.%d", IPHead, C_FF0000(IPIndex), C_00FF00(IPIndex), C_0000FF(IPIndex));
	return IPArray;
}

inline char * GetPhyName(char * PhyNameArray, int PhyNameArraySize, char * PhyNameHead, int PhyNameIndex)
{
	//PhyNameHead->wlan
	memset(PhyNameArray, 0, PhyNameArraySize);
	sprintf(PhyNameArray,"%s%d", PhyNameHead,PhyNameIndex);
	return PhyNameArray;
}

int gRadioCount = 0;
inline char * GetRadioInfoIndex(char * RadioInfoArray, int RadioInfoArraySize, int RadioCount)
{
	int i = 0;
	memset(RadioInfoArray, 0, RadioInfoArraySize);
	if (RadioCount >= 1)
	{
		sprintf(RadioInfoArray,"%d",gRadioCount++);
	}
	for(i = 1; i < RadioCount; i++)
	{
		sprintf(RadioInfoArray, "%s"SQL_INDEX_DELIMIT"%d",RadioInfoArray,gRadioCount++);
	}
	return RadioInfoArray;
}

int gStaCount = 0;
inline char * GetStaIndex(char * StaArray, int StaArraySize, int StaCount)
{
	int i = 0;
	memset(StaArray, 0, StaArraySize);
	if (StaCount >= 1)
	{
		sprintf(StaArray,"%d",gStaCount++);
	}
	for(i = 1; i < StaCount; i++)
	{
		sprintf(StaArray, "%s"SQL_INDEX_DELIMIT"%d",StaArray,gStaCount++);
	}
	return StaArray;
}

int gConnectRadioCount = 0;
#define INSERT_AP_TABLE_PARAM_COUNT 13
int NoCPE = 0;
inline void ChangeCPEAPStaIndexInSql(char* complain_sqlcmd, char* CPESTAindex, int CurrentApCount, int tempConnectRadioIndex)
{
	char * szsqlcmd = strdup(complain_sqlcmd);
	if (szsqlcmd == NULL) return;
	char ** apsql = (char **)alloca(CurrentApCount * sizeof(char *));
	char * oldapsql = NULL;
	char newapsql[512] = {0};
	char ** sql_param = (char **)alloca(INSERT_AP_TABLE_PARAM_COUNT * sizeof(char *));
	char ** index_param = (char **)alloca(MAX_PARAM_COUNT * sizeof(char *));
	int apcount_insql = 0;
	int paramcount_insql = 0;
	int indexcount_insql = 0;
	char replace_oldseq[256] = {0};
	char replace_newseq[256] = {0};
	char NewStaIndex[192] = {0};
	int i = 0,j = 0;
	split(szsqlcmd, "(", apsql, &apcount_insql);
	for (i = 0; i < apcount_insql; i++)
	{
		//printf("apsql[%d]:%s sizeof:%d\n",i,apsql[i],strlen(apsql[i]));
		oldapsql = strdup(apsql[i]);
		memcpy(newapsql, oldapsql, strlen(oldapsql));
		split(apsql[i], ",", sql_param, &paramcount_insql);
		if(paramcount_insql == INSERT_AP_TABLE_PARAM_COUNT + 1)
		{
			Trim(sql_param[8],'\'');
			//printf("\napsql[%d].sql_param[8]:%s\n",i,sql_param[8]);
			split(sql_param[8], SQL_INDEX_DELIMIT, index_param, &indexcount_insql);//RadioInfoIndex		
			for (j = 0; j < indexcount_insql; j++)
			{
				if (index_param[j] == NULL) continue;
				//printf("index_param[%d]:%s\n",j,index_param[j]);
				if (atoi(index_param[j]) == tempConnectRadioIndex) 
				{
					Trim(sql_param[10],'\'');Trim(sql_param[11],'\'');Trim(sql_param[12],'\'');
					sprintf(replace_oldseq, ",'%s','%s','%s,", sql_param[10], sql_param[11], sql_param[12]);//,StaCount,StaIndex,IsCPE)
					if (1 == atoi(sql_param[12]))//IsCPE == 1
					{
						free(oldapsql);free(szsqlcmd);
						oldapsql = NULL;szsqlcmd = NULL;
						NoCPE = 1;
						return;//Get CPE set this CPE to AP
					}
					if (0 == atoi(sql_param[10]))//StaCount == 0
					{
						sprintf(NewStaIndex, "%s", CPESTAindex);
					}
					else
					{
						sprintf(NewStaIndex, "%s|%s",sql_param[11], CPESTAindex);
					}
					sprintf(replace_newseq, ",'%d','%s','%s,", atoi(sql_param[10]) + 1, NewStaIndex, sql_param[12]);//,StaCount,StaIndex,IsCPE) 
					//printf("replace_oldseq:%s\n",replace_oldseq);
					//printf("replace_newseq:%s\n",replace_newseq);
					//printf("complain_sqlcmd:%s\n",complain_sqlcmd);
					//printf("oldapsql:%s\n",oldapsql);		
					replaceFirst(newapsql, replace_oldseq, replace_newseq);
					//printf("newapsql:%s sizeof:%d\n",newapsql,strlen(newapsql));
					replaceFirst(complain_sqlcmd, oldapsql, newapsql);
					//printf("complain_sqlcmd:%s\n",complain_sqlcmd);
					free(oldapsql);free(szsqlcmd);
					oldapsql = NULL;szsqlcmd = NULL;
					return;//Get
				}
			}
		}
		memset(oldapsql,0,strlen(oldapsql));
		memset(newapsql,0,strlen(newapsql));
		free(oldapsql);oldapsql = NULL;
	}
	free(szsqlcmd);szsqlcmd = NULL;
}

inline void InsertCPEIntoStaTableAndGetCPESTAIndex(char * CPEMac, char * CPESTAindex)
{
	char sqlcmd[1024] = {0};
	MYSQL_RES *mysql_res = NULL;
	MYSQL_ROW mysql_row = NULL;
	unsigned char tempIP[18] = {0};
	//insert StaTable
	sprintf(sqlcmd, "insert into Sta_Table (IP,Mac,Type,Rssi,Ssid,WireMode,OnlineTime,"
					"RadioId,WlanId,TxDataRate,RxDataRate,TxFlow,RxFlow) values"
					"('%s','%s','%s','%s','%s','%s','%d','%d','%d','%s','%s','%d','%d')",
					GetIP(tempIP, sizeof(tempIP), "177.177.", Rand() % FFFF),
					CPEMac,"CPE","40","CPE_Simulator","11ac",
					Rand(),1,1,"150","72",Rand(),Rand());
	//fprintf(stderr,"Sql insert_cpe_info affect_line:%ld\n", MySqlExecQuery_NoResult(&mysql, sqlcmd));
	MySqlExecQuery_NoResult(&mysql, sqlcmd);
	//get CpeIndex in StaTable
	sprintf(sqlcmd,"select `Index` from Sta_Table where Mac='%s' limit 0,1",CPEMac);
	mysql_res = MySqlExecQuery(&mysql, sqlcmd);
	if (mysql_res == NULL) {fprintf(stderr,"%s %d mysql_res==NULL\n",__func__,__LINE__);return;}
	mysql_row = mysql_fetch_row(mysql_res);
	strcpy(CPESTAindex, mysql_row[0]);
	mysql_free_result(mysql_res);
}

inline char * GetConnectRadioInfoIndex(char* complain_sqlcmd, int CurrentApCount, char * CPEMac, char * ConnectRadioInfoArray, int ConnectRadioInfoArraySize, int ConnectRadioCount)
{
	int i = 0;
	int tempConnectRadioIndex = 0;
	unsigned char CPESTAindex[16] = {0};
	memset(ConnectRadioInfoArray, 0, ConnectRadioInfoArraySize);
	if (ConnectRadioCount >= 1)
	{
		tempConnectRadioIndex = gConnectRadioCount++;
		sprintf(ConnectRadioInfoArray, "%d", tempConnectRadioIndex);
		//insert StaTable and Get StaIndex
		InsertCPEIntoStaTableAndGetCPESTAIndex(CPEMac, CPESTAindex);
		//change org sqlcmd
		ChangeCPEAPStaIndexInSql(complain_sqlcmd, CPESTAindex, CurrentApCount, tempConnectRadioIndex);
	}
	for(i = 1; i < ConnectRadioCount; i++)
	{
		tempConnectRadioIndex = gConnectRadioCount++;
		sprintf(ConnectRadioInfoArray, "%s"SQL_INDEX_DELIMIT"%d", ConnectRadioInfoArray, tempConnectRadioIndex);
		InsertCPEIntoStaTableAndGetCPESTAIndex(CPEMac, CPESTAindex);
		ChangeCPEAPStaIndexInSql(complain_sqlcmd, CPESTAindex, CurrentApCount, tempConnectRadioIndex);
	}
	return ConnectRadioInfoArray;
}

void write_sql_to_file(char * sql,char * productfilename,long count,int stacount_per_ap)
{
	assert(sql);
	FILE * fp = NULL;
	char sqlfilename[128] = {0};
	sprintf(sqlfilename,"%s-%ld-%d",productfilename,count,stacount_per_ap);
	fp = fopen(sqlfilename,"wb+");
	(fp == NULL) ? printf("open %s fail\n",sqlfilename) : fwrite(sql,strlen(sql),1,fp);
	fclose(fp);
}

void insert_sql_by_file(char * sqlfile)
{
	assert(sqlfile);
	FILE * fp = NULL;
	int filesize = 0;
	char * sqlcmd = (char *)calloc(102400000, sizeof(char));
	fp = fopen(sqlfile,"rb+");
	if (fp == NULL){printf("open %s fail\n",sqlfile);return;}
	fseek (fp , 0 , SEEK_END);  
    filesize = ftell(fp);  
    rewind(fp);  
	fread(sqlcmd,filesize,1,fp);
	fclose(fp);
	//printf("sqlcmd:%s\n",sqlcmd);
	MySqlInit(&mysql);
	fprintf(stderr,"Sql %s affect_line:%ld\n", sqlfile, MySqlExecQuery_NoResult(&mysql, sqlcmd));
	MySqlClose(&mysql);
	free(sqlcmd);
	sqlcmd = NULL;
}

void insert_ap_info(char * ap_productinfo_filename, char * apcount, char *stacount_per_ap)
{
	assert(ap_productinfo_filename);assert(apcount);assert(stacount_per_ap);
	if (!IsNum(apcount) || !IsNum(stacount_per_ap))
	{
		printf("param error apcount and stacount_per_ap must be num!\n");
		return;
	}
	long ApCount = atol(apcount);
	long StaCountPerAp = atol(stacount_per_ap);
	long i = 0;
	char * sqlcmd = (char *)calloc(102400000, sizeof(char));
	char cursql[512] = {0};
	my_ulonglong affect_line = 0;
	unsigned char tempMac[18] = {0};
	unsigned char tempRadioInfoIndex[16] = {0};//MaxRadio=8
	unsigned char tempStaIndex[256] = {0};//MaxSta=128
	unsigned char tempConnectRadioInfoIndex[16] = {0};//MaxConnectRadio=8
	//config
	int ProductCount = 0;
	APProductInfo * Products = NULL;
	char ProductCfgName[16] = {0};
	long sqllen = 0;
	long cursqllen = 0;
	MYSQL_RES *mysql_res = NULL;
	MYSQL_ROW mysql_row = NULL;
	Cfg *m = CfgNew(ap_productinfo_filename);
	if (!IsNum(GetValByKey("GolbalSetting","ProductCount",m))){printf("GolbalSetting->ProductCount param error\n");return;}
	ProductCount = atoi(GetValByKey("GolbalSetting","ProductCount",m));
	Products = (APProductInfo *)calloc(ProductCount, sizeof(APProductInfo));
	//memset(Products, 0, ProductCount * sizeof(APProductInfo));
	for (i = 0; i < ProductCount; i++)
	{
		memset(ProductCfgName,0,sizeof(ProductCfgName));
		sprintf(ProductCfgName,"Product%d",i);
		strcpy(Products[i].Model,GetValByKey(ProductCfgName,"Model",m));
		strcpy(Products[i].MacHead,GetValByKey(ProductCfgName,"MacHead",m));
		strcpy(Products[i].WTPname,GetValByKey(ProductCfgName,"WTPname",m));
		strcpy(Products[i].HwVersion,GetValByKey(ProductCfgName,"HwVersion",m));
		strcpy(Products[i].SwVersion,GetValByKey(ProductCfgName,"SwVersion",m));
		strcpy(Products[i].BootVersion,GetValByKey(ProductCfgName,"BootVersion",m));
		Products[i].RadioCount = atoi(GetValByKey(ProductCfgName,"RadioCount",m));
		Products[i].IsCPE = atoi(GetValByKey(ProductCfgName,"IsCPE",m));
	}
	CfgFree(m);
	//sql
	MySqlInit(&mysql);
	//get min Sta Index
	sprintf(sqlcmd,"select min(`Index`) from Sta_Table limit 0,1");
	mysql_res = MySqlExecQuery(&mysql, sqlcmd);
	if (mysql_res == NULL) {fprintf(stderr,"%s %d mysql_res==NULL\n",__func__,__LINE__);return;}
	mysql_row = mysql_fetch_row(mysql_res);
	(NULL == mysql_row[0]) ? (gStaCount = 0) : (gStaCount = atoi(mysql_row[0]));
	mysql_free_result(mysql_res);
	//get min Radio Index
	sprintf(sqlcmd,"select min(`Index`) from Radio_Table limit 0,1");
	mysql_res = MySqlExecQuery(&mysql, sqlcmd);
	if (mysql_res == NULL) {fprintf(stderr,"%s %d mysql_res==NULL\n",__func__,__LINE__);return;}
	mysql_row = mysql_fetch_row(mysql_res);
	(NULL == mysql_row[0]) ? (gRadioCount = gConnectRadioCount = 0) : (gRadioCount = gConnectRadioCount = atoi(mysql_row[0]));
	mysql_free_result(mysql_res);
	//insert AP_Table
	sprintf(sqlcmd, "insert into AP_Table (Model,Mac,WTPname,Hwversion,Swversion,BootVersion,State,"
					"RadioCount,RadioInfoIndex,ConnectRadioIndex,StaCount,StaIndex,IsCPE) values");
	sqllen = strlen(sqlcmd);
	for (i = 0; i < ApCount; i++)
	{
		//sprintf(sqlcmd, "%s ('%s','%s','%s','%s','%s','%s','%d','%d','%s','%s','%d','%s','%d'),",sqlcmd,
		sprintf(cursql, " ('%s','%s','%s','%s','%s','%s','%d','%d','%s','%s','%d','%s','%d'),",
						Products[i % ProductCount].Model,
						GetMac(tempMac, sizeof(tempMac), Products[i % ProductCount].MacHead, i % FFFFFF),
						Products[i % ProductCount].WTPname,
						Products[i % ProductCount].HwVersion,
						Products[i % ProductCount].SwVersion,
						Products[i % ProductCount].BootVersion,
						CW_ENTER_SULKING,
						Products[i % ProductCount].RadioCount,
						GetRadioInfoIndex(tempRadioInfoIndex, sizeof(tempRadioInfoIndex), Products[i % ProductCount].RadioCount),
						Products[i % ProductCount].IsCPE ? GetConnectRadioInfoIndex(sqlcmd, i * 2, tempMac, tempConnectRadioInfoIndex, sizeof(tempConnectRadioInfoIndex), 1) : "",
						Products[i % ProductCount].IsCPE ? 0 : StaCountPerAp,
						GetStaIndex(tempStaIndex, sizeof(tempStaIndex), Products[i % ProductCount].IsCPE ? 0 : StaCountPerAp),
						Products[i % ProductCount].IsCPE
						);
		cursqllen = strlen(cursql);
		memcpy(sqlcmd + sqllen, cursql, cursqllen);
		sqllen += cursqllen;
		if (NoCPE)
		{
			//printf("sqlcmd1:%s\n",&sqlcmd[strlen(sqlcmd) -4]);
			//sqlcmd[strlen(sqlcmd) - 4] = '0';
			sqlcmd[sqllen - 4] = '0';
			//printf("sqlcmd2:%s\n",&sqlcmd[strlen(sqlcmd) -4]);
			NoCPE = 0;
		}
	}
	//sqlcmd[strlen(sqlcmd) - 1] = 0;
	sqlcmd[sqllen - 1] = 0;
	write_sql_to_file(sqlcmd,ap_productinfo_filename,ApCount,StaCountPerAp);
	fprintf(stderr,"Sql insert_ap_info affect_line:%ld\n", MySqlExecQuery_NoResult(&mysql, sqlcmd));
	MySqlClose(&mysql);
	free(Products);
	Products = NULL;
	free(sqlcmd);
	sqlcmd = NULL;
}
void insert_sta_info(char * sta_productinfo_filename, char * stacount)
{
	assert(sta_productinfo_filename);assert(stacount);
	if (!IsNum(stacount))
	{
		printf("param error stacount must be num!\n");
		return;
	}
	long StaCount = atol(stacount);
	long i = 0;
	char * sqlcmd = (char *)calloc(307200000, sizeof(char));
	char cursql[512] = {0};
	my_ulonglong affect_line = 0;
	unsigned char tempMac[18] = {0};
	unsigned char tempIP[18] = {0};
	//config
	int ProductCount = 0;
	STAProductInfo * Products = NULL;
	char ProductCfgName[16] = {0};
	long sqllen = 0;
	long cursqllen = 0;
	Cfg *m = CfgNew(sta_productinfo_filename);
	if (!IsNum(GetValByKey("GolbalSetting","ProductCount",m))){printf("GolbalSetting->ProductCount param error\n");return;}
	ProductCount = atoi(GetValByKey("GolbalSetting","ProductCount",m));
	Products = (STAProductInfo *)calloc(ProductCount, sizeof(STAProductInfo));
	//memset(Products, 0, ProductCount * sizeof(APProductInfo));
	for (i = 0; i < ProductCount; i++)
	{
		memset(ProductCfgName,0,sizeof(ProductCfgName));
		sprintf(ProductCfgName,"Product%ld",i);
		strcpy(Products[i].IPHead,GetValByKey(ProductCfgName,"IPHead",m));
		strcpy(Products[i].MacHead,GetValByKey(ProductCfgName,"MacHead",m));
		strcpy(Products[i].Type,GetValByKey(ProductCfgName,"Type",m));
		strcpy(Products[i].Rssi,GetValByKey(ProductCfgName,"Rssi",m));
		strcpy(Products[i].Ssid,GetValByKey(ProductCfgName,"Ssid",m));
		strcpy(Products[i].WireMode,GetValByKey(ProductCfgName,"WireMode",m));
		strcpy(Products[i].TxDataRate,GetValByKey(ProductCfgName,"TxDataRate",m));
		strcpy(Products[i].RxDataRate,GetValByKey(ProductCfgName,"RxDataRate",m));
	}
	CfgFree(m);
	//sql
	sprintf(sqlcmd, "insert into Sta_Table (IP,Mac,Type,Rssi,Ssid,WireMode,OnlineTime,"
					"RadioId,WlanId,TxDataRate,RxDataRate,TxFlow,RxFlow) values");
	sqllen = strlen(sqlcmd);
	for (i = 0; i < StaCount; i++)
	{
		//sprintf(sqlcmd, "%s ('%s','%s','%s','%s','%s','%s','%d','%d','%d','%s','%s','%d','%d'),",sqlcmd,
		sprintf(cursql, " ('%s','%s','%s','%s','%s','%s','%d','%d','%d','%s','%s','%d','%d'),",
						GetIP(tempIP, sizeof(tempIP), Products[i % ProductCount].IPHead, i % FFFFFF),
						GetMac(tempMac, sizeof(tempMac), Products[i % ProductCount].MacHead, i % FFFFFF),
						Products[i % ProductCount].Type,
						Products[i % ProductCount].Rssi,
						Products[i % ProductCount].Ssid,
						Products[i % ProductCount].WireMode,
						Rand(),
						(i % 2) + 1,
						(i % 2) + 1,
						Products[i % ProductCount].TxDataRate,
						Products[i % ProductCount].RxDataRate,
						Rand(),
						Rand()
						);
		cursqllen = strlen(cursql);
		memcpy(sqlcmd + sqllen, cursql, cursqllen);
		sqllen += cursqllen;
	}
	//sqlcmd[strlen(sqlcmd) - 1] = 0;
	sqlcmd[sqllen - 1] = 0;
	write_sql_to_file(sqlcmd,sta_productinfo_filename,StaCount,0);
	MySqlInit(&mysql);
	fprintf(stderr,"Sql insert_sta_info affect_line:%ld\n", MySqlExecQuery_NoResult(&mysql, sqlcmd));
	MySqlClose(&mysql);
	free(Products);
	Products = NULL;
	free(sqlcmd);
	sqlcmd = NULL;
}
void insert_radio_info(char * radio_productinfo_filename, char * radiocount)
{
	assert(radio_productinfo_filename);assert(radiocount);
	if (!IsNum(radiocount))
	{
		printf("param error radiocount must be num!\n");
		return;
	}
	long RadioCount = atol(radiocount);
	long i = 0;
	char * sqlcmd = (char *)calloc(102400000, sizeof(char));
	char cursql[512] = {0};
	my_ulonglong affect_line = 0;
	unsigned char tempPhyName[32] = {0};
	unsigned char tempBssid[18] = {0};
	//config
	int ProductCount = 0;
	RADIOProductInfo * Products = NULL;
	char ProductCfgName[16] = {0};
	long sqllen = 0;
	long cursqllen = 0;
	Cfg *m = CfgNew(radio_productinfo_filename);
	if (!IsNum(GetValByKey("GolbalSetting","ProductCount",m))){printf("GolbalSetting->ProductCount param error\n");return;}
	ProductCount = atoi(GetValByKey("GolbalSetting","ProductCount",m));
	Products = (RADIOProductInfo *)calloc(ProductCount, sizeof(RADIOProductInfo));
	//memset(Products, 0, ProductCount * sizeof(APProductInfo));
	for (i = 0; i < ProductCount; i++)
	{
		memset(ProductCfgName,0,sizeof(ProductCfgName));
		sprintf(ProductCfgName,"Product%d",i);
		strcpy(Products[i].PhyHead, GetValByKey(ProductCfgName,"PhyHead",m));
		strcpy(Products[i].BssidHead, GetValByKey(ProductCfgName,"BssidHead",m));
	}
	CfgFree(m);
	//sql
	sprintf(sqlcmd, "insert into Radio_Table (PhyName,Bssid) values");
	sqllen = strlen(sqlcmd);
	for (i = 0; i < RadioCount; i++)
	{
		//sprintf(sqlcmd, "%s ('%s','%s'),",sqlcmd,
		sprintf(cursql, " ('%s','%s'),",
						GetPhyName(tempPhyName, sizeof(tempPhyName), Products[i % ProductCount].PhyHead, i % 2),
						GetMac(tempBssid, sizeof(tempBssid), Products[i % ProductCount].BssidHead, i % FFFFFF)
						);
		cursqllen = strlen(cursql);
		memcpy(sqlcmd + sqllen, cursql, cursqllen);
		sqllen += cursqllen;
	}
	//sqlcmd[strlen(sqlcmd) - 1] = 0;
	sqlcmd[sqllen - 1] = 0;
	write_sql_to_file(sqlcmd,radio_productinfo_filename,RadioCount,0);
	MySqlInit(&mysql);
	fprintf(stderr,"Sql insert_radio_info affect_line:%ld\n", MySqlExecQuery_NoResult(&mysql, sqlcmd));
	MySqlClose(&mysql);
	free(Products);
	Products = NULL;
	free(sqlcmd);
	sqlcmd = NULL;
}
void clear_ap_info()
{
	char sqlcmd[200];
	my_ulonglong affect_line = 0;
	MySqlInit(&mysql);
	sprintf(sqlcmd, "delete from AP_Table");
	fprintf(stderr,"Sql clear_ap_info affect_line:%ld\n", MySqlExecQuery_NoResult(&mysql, sqlcmd));
	sprintf(sqlcmd, "truncate AP_Table");
	fprintf(stderr,"Sql clear_ap_info affect_line:%ld\n", MySqlExecQuery_NoResult(&mysql, sqlcmd));
	MySqlClose(&mysql);
}
void clear_sta_info()
{
	char sqlcmd[200];
	my_ulonglong affect_line = 0;
	MySqlInit(&mysql);
	sprintf(sqlcmd, "delete from Sta_Table");
	fprintf(stderr,"Sql clear_sta_info affect_line:%ld\n", MySqlExecQuery_NoResult(&mysql, sqlcmd));
	sprintf(sqlcmd, "truncate Sta_Table");
	fprintf(stderr,"Sql clear_sta_info affect_line:%ld\n", MySqlExecQuery_NoResult(&mysql, sqlcmd));	
	MySqlClose(&mysql);
}
void clear_radio_info()
{
	char sqlcmd[200];
	my_ulonglong affect_line = 0;
	MySqlInit(&mysql);
	sprintf(sqlcmd, "delete from Radio_Table");
	fprintf(stderr,"Sql clear_radio_info affect_line:%ld\n", MySqlExecQuery_NoResult(&mysql, sqlcmd));
	sprintf(sqlcmd, "truncate Radio_Table");
	fprintf(stderr,"Sql clear_radio_info affect_line:%ld\n", MySqlExecQuery_NoResult(&mysql, sqlcmd));
	MySqlClose(&mysql);
}

FillDataBaseParam TransportParam(char * param)
{
	if (strcmp(param,"build") == 0) return BUILD_DATABASE;
	if (strcmp(param,"addap") == 0) return INSERT_AP_INFO;
	if (strcmp(param,"addsta") == 0) return INSERT_STA_INFO;
	if (strcmp(param,"addradio") == 0) return INSERT_RADIO_INFO;
	if (strcmp(param,"clearap") == 0) return CLEAR_AP_TABLE;
	if (strcmp(param,"clearsta") == 0) return CLEAR_STA_TABLE;
	if (strcmp(param,"clearradio") == 0) return CLEAR_RADIO_TABLE;
	if (strcmp(param,"0apstate") == 0) return CLEAR_APSTATE;
	if (strcmp(param,"addfile") == 0) return INSERT_BY_FILE;
	return UNKONW_PARAM;
}

void main (int argv, char * args[])
{
	printf("argv:%d args:%s\n",argv,args[1]);
	if (argv <= 1)
	{
		show_usage();
		return;
	}
	strcpy(commond,args[1]);

	switch(TransportParam(args[1]))
	{
		case BUILD_DATABASE: build_database();return;
		case INSERT_AP_INFO: 
			if (argv != 5) {show_usage();return;}
			insert_ap_info(args[2], args[3], args[4]);
			return;
		case INSERT_STA_INFO: 
			if (argv != 4) {show_usage();return;}
			insert_sta_info(args[2], args[3]);
			return;
		case INSERT_RADIO_INFO: 
			if (argv != 4) {show_usage();return;}
			insert_radio_info(args[2], args[3]);
			return;
		case CLEAR_AP_TABLE: clear_ap_info();return;
		case CLEAR_STA_TABLE: clear_sta_info();return;
		case CLEAR_RADIO_TABLE: clear_radio_info();return;
		case CLEAR_APSTATE: ZeroAPState();return;
		case INSERT_BY_FILE: 
			if (argv != 3) {show_usage();return;}
			insert_sql_by_file(args[2]);return;
		default:show_usage();return;
	}
}
