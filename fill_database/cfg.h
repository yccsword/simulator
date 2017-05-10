/*************************************************************************
        > File Name: Cfg.h
        > Author: ycc
        > Created Time: Sat 11 Jan 2017 17:56:46 PM CST
 ************************************************************************/
#ifndef _Cfg_H
#define WORD "="
typedef struct _Item
{
    void *K;
    void *V;
    struct _Item *Next;
}Item;
typedef struct _Node
{
    int Size;
    char *Base;
    struct _Node *Next;
    struct _Item *iHead;
}Node;
typedef struct _Cfg 
{
    int Size;
    struct _Node *nHead;
}Cfg;
Cfg *CfgNew(const char *path);
Node *GetNodeByBase(char *base,Cfg *cfg);
char  *GetValByKey(char *base,char *key,Cfg *Cfg);
int CfgFree(Cfg *cfg);
void prtCfg(Cfg *m);
#define _Cfg_H
#endif

