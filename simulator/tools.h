
#ifndef __SIMULATOR_TOOLS_HEADER__
#define __SIMULATOR_TOOLS_HEADER__
#include <sys/epoll.h>
#include <mysql.h>
int Str2Mac(unsigned char *mac,unsigned char *in);
int Mac2Str(char *macAddr, char *str);


void replaceFirst(char *str1,char *str2,char *str3);  
void replace(char *str1,char *str2,char *str3);  
void substring(char *dest,char *src,int start,int end);  
char charAt(char *src,int index);  
int indexOf(char *str1,char *str2);  
int lastIndexOf(char *str1,char *str2);  
void ltrim(char *str);  
void rtrim(char *str);  
void trim(char *str);  

int Ip2Int( const char *ip );
void Int2Ip( int ip_num, char *ip );

time_t get_system_boot_time(void);

void split(char *src, const char *separator, char **dest, int *num);

#define WORK_THREAD_NUM 4
#define THREAD_POOL_COUNT 2

int Epoll_fd[WORK_THREAD_NUM * THREAD_POOL_COUNT];
int Epoll_fd_State[WORK_THREAD_NUM * THREAD_POOL_COUNT];
extern MYSQL *mysql_thread_pool[WORK_THREAD_NUM * THREAD_POOL_COUNT];

void setnonblocking(int sock);

typedef enum
{
	DEBUG_INFO,
	DATA_INFO
} MyPrintArg;

int IsNum(const char *p);
void Trim(char* s, char c);

//ANSI Color
#define COLOR_END "\033[0m"
#define COLOR_RED "\033[;31m"
#define COLOR_GREEN "\033[;32m"
#define COLOR_YELLOW "\033[;33m"
#define COLOR_BLUE "\033[;34m"
#define COLOR_PURPLE "\033[;35m"
#define COLOR_DARKGREEN "\033[;36m"

#endif

