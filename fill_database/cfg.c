/*************************************************************************
        > File Name: Cfg.c
        > Author: ycc
        > Created Time: Sat 11 Jan 2017 17:56:46 PM CST
 ************************************************************************/

#include "cfg.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#define bufSize  1024
char *trimSp(char *buf)
{
    char tbuf[bufSize] = {'\0'};
    if(buf == NULL || strlen(buf) ==0)
    {
        return NULL;
    }
    int len = strlen(buf);
    int i = 0, j =0;
    while(i < len)
    {
        if(iscntrl(buf[i]) ==0 && isspace(buf[i]) ==0)
        {
            tbuf[j] = buf[i];
            j++;
        }
        i++;
    }
    memset(buf,'\0',len);
    strncpy(buf,tbuf,strlen(tbuf));
    return buf;
}
int getPos(char *buf)
{
    if(buf == NULL || strlen(buf) == 0)
    {
        return -1;
    }
    int len = strlen(buf);
    int i = 0;
    while(i < len)
    {
        if(buf[i] == '='){
            break;
        }
        i++;
    }
    return i++;
}
Cfg *CfgNew(const char *path)
{
    Cfg *cfg = NULL;
    if(path == NULL || strlen(path) == 0)
    {
        return NULL;
    }
    FILE *fp = fopen(path,"r");
    char buf[bufSize] = {'\0'};
    if(fp == NULL)
    {
        return NULL;
    }
    //cfg = (Cfg *)malloc(sizeof(*cfg));
    cfg = (Cfg *)calloc(1,sizeof(*cfg));
    while(fgets(buf,bufSize,fp) != NULL)
    {
        char *tbuf = trimSp(buf);
        int pos = getPos(tbuf);
        int len = strlen(tbuf);
        if(len > 0 && len == pos  && tbuf[0] == '[' && tbuf[len-1] == ']')
        {
            //Node *tmp = (Node *)malloc(sizeof(*tmp));
            Node *tmp = (Node *)calloc(1,sizeof(*tmp));
            //tmp->Base = (char *)malloc(len-1);
            tmp->Base = (char *)calloc(1,len-1);
            memset(tmp->Base,'\0',len-1);
            strncpy(tmp->Base,tbuf+1,len-2);
            if(cfg->nHead == NULL)
            {
                cfg->nHead  = tmp;
            }else{
                tmp->Next =cfg->nHead;
                cfg->nHead = tmp;
            }
            cfg->Size++;
        }else{
            if(len > 0 && pos != len)
            {
                Node *h = cfg->nHead;
                char *val = tbuf + pos+1;
                tbuf[pos] = '\0';
                char *key = tbuf;
                int k_len = strlen(key);
                int v_len = strlen(val);
                Item *t = h->iHead;
                if(t == NULL) {
                    //t = (Item *)malloc(sizeof(*t));
					t = (Item *)calloc(1,sizeof(*t));
                    h->iHead = t;
                }else {
                    //Item *cur = (Item *)malloc(sizeof(*cur));
					Item *cur = (Item *)calloc(1,sizeof(*cur));
                    cur->Next = h->iHead;
                    h->iHead = cur;
                }
                //h->iHead->K = (char *)malloc(k_len+1);
				h->iHead->K = (char *)calloc(1,k_len+1);
                //h->iHead->V = (char *)malloc(v_len+1);
				h->iHead->V = (char *)calloc(1,v_len+1);
                memset(h->iHead->K,'\0',k_len+1);
                memset(h->iHead->V,'\0',v_len+1);
                strncpy(h->iHead->K,key,k_len);
                strncpy(h->iHead->V,val,v_len);
                h->Size++;
            }
        }
    memset(buf,'\0',bufSize);
    }
    if(fp != NULL){
        fclose(fp);
    }
    if(cfg->Size == 0){
        free(cfg);
        cfg = NULL;
    }
    return cfg;
}
void prtCfg(Cfg *m)
{
    printf("cfg =%p,cfg->Size=%d\n",m,m->Size);
    Node *cur = m->nHead;
    while(cur != NULL)
    {
        printf("   -----cur node=%p,next =%p,base= %s[len=%d],size =%d\n",cur,cur->Next,\
               cur->Base,strlen(cur->Base),cur->Size);
        Item *icur = cur->iHead;
        while(icur != NULL)
        {
            printf("        *******cur item=%p,next =%p,key[len=%d] = %s,val[len=%d] = %s\n",icur,icur->Next,\
                   strlen(icur->K),icur->K,strlen(icur->V),icur->V);
            icur = icur ->Next;
        }
        cur = cur->Next;
    }
}
Node *GetNodeByBase(char *base,Cfg *cfg)
{
    Node *nv = NULL;
    if(cfg == NULL) {
        return nv;
    }
    Node *cur = cfg->nHead;
    while(cur != NULL)
    {
        if(strncmp(cur->Base,base,strlen(cur->Base)) == 0)
        {
            nv = cur;
        }
        cur = cur->Next;
    }
    return nv;
}
char  *GetValByKey(char *base,char *key,Cfg *cfg)
{
    char  *val = NULL;
    if(cfg == NULL || cfg->Size <= 0){
        return NULL;
    }
    Node *cur = cfg->nHead;
    while(cur != NULL)
    {
        if(strncmp(cur->Base,base,strlen(cur->Base)) == 0)
        {
            Item *icur = cur->iHead;
            while(icur != NULL)
            {
                if(strncmp(icur->K,key,strlen(icur->K)) == 0){
                    val = icur->V;
                    break;
                }
                icur = icur->Next;
            }
        }
        cur = cur->Next;
    }
    return val;
}
int  CfgFree(Cfg *cfg)
{
    if(cfg == NULL || cfg->Size ==0)
    {
        return -1;
    }
    Node *cur = cfg->nHead;
    while(cur != NULL)
    {
        Item *icur = cur->iHead;
        while(icur != NULL)
        {
            //printf("        ******** free icur=%p,next=%p********\n",icur,icur->Next);
            free(icur->K);
            free(icur->V);
            free(icur);
            icur->K = icur->V = NULL;
            icur = icur ->Next;
        }
        if(icur != NULL){
            icur = NULL;
        }
        //printf("******** free node=%p,next=%p********\n",cur,cur->Next);
        free(cur->Base);
        free(cur);
        cur->Base =  NULL;
		cur->iHead = NULL;
        cur = cur->Next;
    }
    if(cur != NULL){
        cur = NULL;
    }
	cfg->nHead = NULL;
	free(cfg);
	cfg = NULL;
    return 0;

}
