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
#include <stdarg.h>


int Epoll_fd[WORK_THREAD_NUM * THREAD_POOL_COUNT] = {0};
int Epoll_fd_State[WORK_THREAD_NUM * THREAD_POOL_COUNT] = {0};
MYSQL *mysql_thread_pool[WORK_THREAD_NUM * THREAD_POOL_COUNT] = {0};

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
/*将str1出现的所有的str2都替换为str3*/  
void replace(char *str1,char *str2,char *str3)  
{  
    while(strstr(str1,str2)!=NULL)  
    {  
        replaceFirst(str1,str2,str3);  
    }  
}  
/*截取src字符串中,从下标为start开始到end-1(end前面)的字符串保存在dest中(下标从0开始)*/  
void substring(char *dest,char *src,int start,int end)  
{  
    int i=start;  
    if(start>strlen(src))return;  
    if(end>strlen(src))  
        end=strlen(src);  
    while(i<end)  
    {     
        dest[i-start]=src[i];  
        i++;  
    }  
    dest[i-start]='\0';  
    return;  
}  
/*返回src中下标为index的字符*/  
char charAt(char *src,int index)  
{  
    char *p=src;  
    int i=0;  
    if(index<0||index>strlen(src))  
        return 0;  
    while(i<index)i++;  
    return p[i];  
}  
/*返回str2第一次出现在str1中的位置(下表索引),不存在返回-1*/  
int indexOf(char *str1,char *str2)  
{  
    char *p=str1;  
    int i=0;  
    p=strstr(str1,str2);  
    if(p==NULL)  
        return -1;  
    else{  
        while(str1!=p)  
        {  
            str1++;  
            i++;  
        }  
    }  
    return i;  
}  
/*返回str1中最后一次出现str2的位置(下标),不存在返回-1*/  
int lastIndexOf(char *str1,char *str2)  
{  
    char *p=str1;  
    int i=0,len=strlen(str2);  
    p=strstr(str1,str2);  
    if(p==NULL)return -1;  
    while(p!=NULL)  
    {  
        for(;str1!=p;str1++)i++;  
        p=p+len;  
        p=strstr(p,str2);  
    }  
    return i;  
}  
/*删除str左边第一个非空白字符前面的空白字符(空格符和横向制表符)*/  
void ltrim(char *str)  
{  
    int i=0,j,len=strlen(str);  
    while(str[i]!='\0')  
    {  
        if(str[i]!=32&&str[i]!=9)break;/*32:空格,9:横向制表符*/  
        i++;  
    }  
    if(i!=0)  
    for(j=0;j<=len-i;j++)  
    {     
        str[j]=str[j+i];/*将后面的字符顺势前移,补充删掉的空白位置*/  
    }  
}  
/*删除str最后一个非空白字符后面的所有空白字符(空格符和横向制表符)*/  
void rtrim(char *str)  
{  
    char *p=str;  
    int i=strlen(str)-1;  
    while(i>=0)  
    {  
        if(p[i]!=32&&p[i]!=9)break;  
        i--;  
    }  
    str[++i]='\0';  
}  
/*删除str两端的空白字符*/  
void trim(char *str)  
{  
    ltrim(str);  
    rtrim(str);  
}  
///*删除str两端的指定字符*/  
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

int Str2Mac(unsigned char *mac,unsigned char *in)
{
	//fprintf(stderr,"in:%s\n",in);
	replace(in,":","");
	char c1, c2;  
	int len, i;  
	len = strlen(in);  
	for (i = 0; i < (len / 2); i ++) 
	{  
		c1 = toupper(in[2*i]);  
		c2 = toupper(in[2*i + 1]);  
		if (c1 < '0' || (c1 > '9' && c1 <'A') || c1 > 'F')  
		return 0;  
		if (c2 < '0' || (c2 > '9' && c2 <'A') || c2 > 'F')  
		return 0;  
		c1 = (c1 > '9') ? (c1 - 'A' + 10) : (c1 - '0');  
		c2 = (c2 > '9') ? (c2 - 'A' + 10) : (c2 - '0');  
		mac[i] = c1 << 4 | c2;  
	}
	/*
	fprintf(stderr,"%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\n",
           (unsigned char ) mac[0], (unsigned char ) mac[1], (unsigned char ) mac[2],
           (unsigned char ) mac[3], (unsigned char ) mac[4], (unsigned char ) mac[5]);
    */
	return 0;
}

	
int Mac2Str(char *macAddr, char *str) {
   if ( macAddr == NULL ) return -1;
   if ( str == NULL ) return -1;
   sprintf(str, "%2.2x%2.2x%2.2x%2.2x%2.2x%2.2x",
           (unsigned char ) macAddr[0], (unsigned char ) macAddr[1], (unsigned char ) macAddr[2],
           (unsigned char ) macAddr[3], (unsigned char ) macAddr[4], (unsigned char ) macAddr[5]);
   return 0;
}

int Ip2Int( const char *ip )
{
    return ntohl( inet_addr( ip ) );
}

void Int2Ip( int ip_num, char *ip )
{
    strcpy( ip, (char*)inet_ntoa( htonl( ip_num ) ) );
}

time_t get_system_boot_time(void)
{
    struct sysinfo info;
    time_t cur_time = 0;
    time_t boot_time = 0;
    //struct tm *ptm = NULL;
    if (sysinfo(&info)) {
    fprintf(stderr, "Failed to get sysinfo, errno:%u, reason:%s\n",
        errno, strerror(errno));
    return -1;
    }
    time(&cur_time);
    if (cur_time > info.uptime) {
    boot_time = cur_time - info.uptime;
    }
    else {
    boot_time = info.uptime - cur_time;
    }
	return boot_time;
    //ptm = localtime(&boot_time);
    //printf("System boot time: %d-%-d-%d %d:%d:%d\n", ptm->tm_year + 1900,
        //ptm->tm_mon + 1, ptm->tm_mday, ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
   	//return 0; 
}

#if 0
void split(char *src, const char *separator, char **dest, int *num)
{
    char *pSeparator, *pStart, *pEnd;
    unsigned int sep_len;
    int count = 0;
    
    if (src == NULL || strlen(src) == 0) return;
    
    pSeparator = (char *)malloc(16);
    if (pSeparator == NULL) return;
    
    if (separator == NULL || strlen(separator) == 0) strcpy(pSeparator," ");/* one blank by default */
    else strcpy(pSeparator,separator);

    sep_len = strlen(pSeparator);
        
    pStart = src;
    
    while(1)
    {
        pEnd = strstr(pStart, pSeparator);
        if (pEnd != NULL)
        {
            memset(pEnd,'\0',sep_len);
            *dest++ = pStart;
            pEnd = pEnd + sep_len;
            pStart = pEnd;
            ++count;
        }
        else
        {
            *dest = pStart;
            ++count;
            break;
        }
    }

    *num = count;

    if (pSeparator != NULL) free(pSeparator);
}
#else
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
#endif

void setnonblocking(int sock)  
{  
    int opts;     
    opts=fcntl(sock,F_GETFL);  
    if(opts<0)     
    {     
        perror("fcntl(sock,GETFL)");  
        fprintf(stderr,"End at: %d",__LINE__);  
		fprintf(stderr,"%s %d\n",__func__,__LINE__);//ycc care
        exit(1);  
    }  
      
    opts = opts|O_NONBLOCK;   
    if(fcntl(sock,F_SETFL,opts)<0)     
    {     
        perror("fcntl(sock,SETFL,opts)");  
        fprintf(stderr,"End at: %d",__LINE__);  
		fprintf(stderr,"%s %d\n",__func__,__LINE__);//ycc care
        exit(1);      
    }      
  
}  

void MyPrint(MyPrintArg type,const char *format,...)
{
	va_list args;
	unsigned char* data;
	int datalen;
	va_start(args,format);
	switch(type)
	{
		case DEBUG_INFO:
			vfprintf(stderr,format,args);
			break;
		case DATA_INFO:
			fprintf(stderr,"%s",format);
			data=va_arg(args,unsigned char*);
			datalen=va_arg(args,int);
			while(datalen--)
				fprintf(stderr,"%02x ",*data++);
			break;
		default:
			break;
	}
	va_end(args);
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

