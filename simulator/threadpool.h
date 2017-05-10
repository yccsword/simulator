#ifndef __SIMULATOR_THREADPOOL_HEADER__
#define __SIMULATOR_THREADPOOL_HEADER__
#include "tools.h"
/*
*�̳߳����������к͵ȴ���������һ��CThread_worker
*�������������������������һ������ṹ
*/
typedef struct worker
{
    /*�ص���������������ʱ����ô˺�����ע��Ҳ��������������ʽ*/
    void *(*process) (void *arg);
    void *arg;/*�ص������Ĳ���*/
    struct worker *next;

} CThread_worker;

/*�̳߳ؽṹ*/
typedef struct
{
    pthread_mutex_t queue_lock;
    pthread_cond_t queue_ready;

    /*����ṹ���̳߳������еȴ�����*/
    CThread_worker *queue_head;

    /*�Ƿ������̳߳�*/
    int shutdown;
    pthread_t *threadid;
    /*�̳߳�������Ļ�߳���Ŀ*/
    int max_thread_num;
    /*��ǰ�ȴ����е�������Ŀ*/
    int cur_queue_size;

} CThread_pool;
CThread_pool * pool_init (int max_thread_num);
int pool_add_worker (void *(*process) (void *arg), void *arg);
int pool_destroy (CThread_pool *pool);
extern CThread_pool *threadpool[THREAD_POOL_COUNT];
//�̲߳���
typedef struct
{
	void *arg1;
    void *arg2;
}ThreadArgs;

#endif
