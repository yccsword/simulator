/************************************************************************************************
 * Copyright (c) 2006-2009 Laboratorio di Sistemi di Elaborazione e Bioingegneria Informatica	*
 *                          Universita' Campus BioMedico - Italy								*
 *																								*
 * This program is free software; you can redistribute it and/or modify it under the terms		*
 * of the GNU General Public License as published by the Free Software Foundation; either		*
 * version 2 of the License, or (at your option) any later version.								*
 *																								*
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY				*
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A				*
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.						*
 *																								*
 * You should have received a copy of the GNU General Public License along with this			*
 * program; if not, write to the:																*
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,							*
 * MA  02111-1307, USA.											
 * 
 * In addition, as a special exception, the copyright holders give permission to link the  *
 * code of portions of this program with the OpenSSL library under certain conditions as   *
 * described in each individual source file, and distribute linked combinations including  * 
 * the two. You must obey the GNU General Public License in all respects for all of the    *
 * code used other than OpenSSL.  If you modify file(s) with this exception, you may       *
 * extend this exception to your version of the file(s), but you are not obligated to do   *
 * so.  If you do not wish to do so, delete this exception statement from your version.    *
 * If you delete this exception statement from all source files in the program, then also  *
 * delete it here.                                                                         *
 *																								*
 * -------------------------------------------------------------------------------------------- *
 * Project:  Capwap																				*
 *																								*
 * Authors : Ludovico Rossi (ludo@bluepixysw.com)												*  
 *           Del Moro Andrea (andrea_delmoro@libero.it)											*
 *           Giovannini Federica (giovannini.federica@gmail.com)								*
 *           Massimo Vellucci (m.vellucci@unicampus.it)											*
 *           Mauro Bisson (mauro.bis@gmail.com)													*
 *	         Antonio Davoli (antonio.davoli@gmail.com)		
 * 			 Elena Agostini (elena.ago@gmail.com)												*
 ************************************************************************************************/

#include "CWWTP.h"

#ifdef DMALLOC
#include "../dmalloc-5.5.0/dmalloc.h"
#endif
#include "cfg.h"

#ifdef SOFTMAC
CW_THREAD_RETURN_TYPE CWWTPThread_read_data_from_hostapd(void *arg);
#endif

CW_THREAD_RETURN_TYPE CWWTPReceiveFrame(void *arg);
CW_THREAD_RETURN_TYPE CWWTPReceiveStats(void *arg);
CW_THREAD_RETURN_TYPE CWWTPReceiveFreqStats(void *arg);
CW_THREAD_RETURN_TYPE gogo(void *arg);

int 	gEnabledLog;
int 	gMaxLogFileSize;
//Elena Agostini - 05/2014
char 	gLogFileName[512];// = WTP_LOG_FILE_NAME;

/* addresses of ACs for Discovery */
char	**gCWACAddresses;
int	gCWACCount = 0;

int gIPv4StatusDuplicate = 0;
int gIPv6StatusDuplicate = 0;

char *gWTPLocation = NULL;
char *gWTPName = NULL;
char gWTPSessionID[16];

/* if not NULL, jump Discovery and use this address for Joining */
char 		*gWTPForceACAddress = NULL;
CWAuthSecurity 	gWTPForceSecurity;

/* UDP network socket */
CWSocket 		gWTPSocket;
CWSocket 		gWTPDataSocket;
/* DTLS session vars */
CWSecurityContext	gWTPSecurityContext;
CWSecuritySession 	gWTPSession;

/* Elena Agostini - 03/2014: DTLS Data Session WTP */
CWSecuritySession gWTPSessionData;
CWSecurityContext gWTPSecurityContextData;

/* Elena Agostini - 02/2014: OpenSSL params variables */
char *gWTPCertificate=NULL;
char *gWTPKeyfile=NULL;
char *gWTPPassword=NULL;

/* Elena Agostini - 02/2014: ECN Support Msg Elem MUST be included in Join Request/Response Messages */
int gWTPECNSupport=0;

/* list used to pass frames from wireless interface to main thread */
CWSafeList 		gFrameList;

/* list used to pass CAPWAP packets from AC to main thread */
CWSafeList 		gPacketReceiveList;

/* Elena Agostini - 03/2014: Liste used to pass CAPWAP DATA packets from AC to DataThread */
CWSafeList gPacketReceiveDataList;

/* Elena Agostini - 02/2014: Port number params config.wtp */
int WTP_PORT_CONTROL;
int WTP_PORT_DATA;

//Elena Agostini - 05/2014: single log_file foreach WTP
char * wtpLogFile;

/* used to synchronize access to the lists */
CWThreadCondition    gInterfaceWait;
CWThreadMutex 		gInterfaceMutex;

//Elena Agostini: Mutex and Cond dedicated to Data Packet List
CWThreadCondition    gInterfaceWaitData;
CWThreadMutex 		gInterfaceMutexData;

/* infos about the ACs to discover */
CWACDescriptor *gCWACList = NULL;
/* infos on the better AC we discovered so far */
CWACInfoValues *gACInfoPtr = NULL;

/* WTP statistics timer */
int gWTPStatisticsTimer = CW_STATISTIC_TIMER_DEFAULT;

WTPRebootStatisticsInfo gWTPRebootStatistics;
CWWTPRadiosInfo gRadiosInfo;

/* path MTU of the current session */
int gWTPPathMTU = 0;
int gWTPRetransmissionCount;

CWPendingRequestMessage gPendingRequestMsgs[MAX_PENDING_REQUEST_MSGS];	

CWBool WTPExitOnUpdateCommit = CW_FALSE;

//Elena Agostini: nl80211 support
struct WTPglobalPhyInfo * gWTPglobalPhyInfo;
struct nl80211SocketUnit globalNLSock;

#define CW_SINGLE_THREAD

int wtpInRunState;


CWBool SimulatorReceiveMessage(CWProtocolMessage *msgPtr, char* PacketReceive, int readBytes) {
	CWList fragments = NULL;
	CWBool dataFlag = CW_FALSE;//数据隧道报文标识
	return CWProtocolParseFragment(PacketReceive, readBytes, &fragments, msgPtr, &dataFlag, NULL);
}
/* 
 * Receive a message, that can be fragmented. This is useful not only for the Join State
 */
CWBool CWReceiveMessage(CWProtocolMessage *msgPtr) {
	CWList fragments = NULL;
	int readBytes;
	char buf[CW_BUFFER_SIZE];
	CWBool dataFlag = CW_FALSE;
	
	CW_REPEAT_FOREVER {
		CW_ZERO_MEMORY(buf, CW_BUFFER_SIZE);
#ifdef CW_NO_DTLS
		char *pkt_buffer = NULL;

		CWLockSafeList(gPacketReceiveList);

		while (CWGetCountElementFromSafeList(gPacketReceiveList) == 0)
			CWWaitElementFromSafeList(gPacketReceiveList);

		pkt_buffer = (char*)CWRemoveHeadElementFromSafeListwithDataFlag(gPacketReceiveList, &readBytes,&dataFlag);

		CWUnlockSafeList(gPacketReceiveList);

		CW_COPY_MEMORY(buf, pkt_buffer, readBytes);
		CW_FREE_OBJECT(pkt_buffer);
#else
		if(!CWSecurityReceive(gWTPSession, buf, CW_BUFFER_SIZE, &readBytes)) {return CW_FALSE;}
#endif

		if(!CWProtocolParseFragment(buf, readBytes, &fragments, msgPtr, &dataFlag, NULL)) {
			if(CWErrorGetLastErrorCode() == CW_ERROR_NEED_RESOURCE) { // we need at least one more fragment
				continue;
			} else { // error
				CWErrorCode error;
				error=CWErrorGetLastErrorCode();
				switch(error)
				{
					case CW_ERROR_SUCCESS: {CWDebugLog("ERROR: Success"); break;}
					case CW_ERROR_OUT_OF_MEMORY: {CWDebugLog("ERROR: Out of Memory"); break;}
					case CW_ERROR_WRONG_ARG: {CWDebugLog("ERROR: Wrong Argument"); break;}
					case CW_ERROR_INTERRUPTED: {CWDebugLog("ERROR: Interrupted"); break;}
					case CW_ERROR_NEED_RESOURCE: {CWDebugLog("ERROR: Need Resource"); break;}
					case CW_ERROR_COMUNICATING: {CWDebugLog("ERROR: Comunicating"); break;}
					case CW_ERROR_CREATING: {CWDebugLog("ERROR: Creating"); break;}
					case CW_ERROR_GENERAL: {CWDebugLog("ERROR: General"); break;}
					case CW_ERROR_OPERATION_ABORTED: {CWDebugLog("ERROR: Operation Aborted"); break;}
					case CW_ERROR_SENDING: {CWDebugLog("ERROR: Sending"); break;}
					case CW_ERROR_RECEIVING: {CWDebugLog("ERROR: Receiving"); break;}
					case CW_ERROR_INVALID_FORMAT: {CWDebugLog("ERROR: Invalid Format"); break;}
					case CW_ERROR_TIME_EXPIRED: {CWDebugLog("ERROR: Time Expired"); break;}
					case CW_ERROR_NONE: {CWDebugLog("ERROR: None"); break;}
				}
				CWDebugLog("~~~~~~");
				return CW_FALSE;
			}
		} else break; // the message is fully reassembled
	}
	
	return CW_TRUE;
}

/*
 * Elena Agostini - 03/2014: PacketDataList + DTLS Data Session WTP
 */
CWBool CWReceiveDataMessage(CWProtocolMessage *msgPtr) {
	CWList fragments = NULL;
	int readBytes;
	char buf[CW_BUFFER_SIZE];
	CWBool dataFlag = CW_TRUE;
	char *pkt_buffer = NULL;
	
	CW_REPEAT_FOREVER {
		CW_ZERO_MEMORY(buf, CW_BUFFER_SIZE);
		readBytes=0;
		pkt_buffer = NULL;
		
#ifdef CW_DTLS_DATA_CHANNEL
	if(!CWSecurityReceive(gWTPSessionData, buf, CW_BUFFER_SIZE, &readBytes)) {return CW_FALSE;}
#else
		CWLockSafeList(gPacketReceiveDataList);
		while (CWGetCountElementFromSafeList(gPacketReceiveDataList) == 0)
			CWWaitElementFromSafeList(gPacketReceiveDataList);
		pkt_buffer = (char*)CWRemoveHeadElementFromSafeListwithDataFlag(gPacketReceiveDataList, &readBytes,&dataFlag);
		CWUnlockSafeList(gPacketReceiveDataList);
		
		if(pkt_buffer != NULL)
		{
			CW_COPY_MEMORY(buf, pkt_buffer, readBytes);
			CW_FREE_OBJECT(pkt_buffer);
		}
#endif

	if(!CWProtocolParseFragment(buf, readBytes, &fragments, msgPtr, &dataFlag, NULL)) {
			if(CWErrorGetLastErrorCode()){
				CWErrorCode error;
				error=CWErrorGetLastErrorCode();
				switch(error)
				{
					case CW_ERROR_SUCCESS: {CWDebugLog("ERROR: Success"); break;}
					case CW_ERROR_OUT_OF_MEMORY: {CWDebugLog("ERROR: Out of Memory"); break;}
					case CW_ERROR_WRONG_ARG: {CWDebugLog("ERROR: Wrong Argument"); break;}
					case CW_ERROR_INTERRUPTED: {CWDebugLog("ERROR: Interrupted"); break;}
					case CW_ERROR_NEED_RESOURCE: {CWDebugLog("ERROR: Need Resource"); break;}
					case CW_ERROR_COMUNICATING: {CWDebugLog("ERROR: Comunicating"); break;}
					case CW_ERROR_CREATING: {CWDebugLog("ERROR: Creating"); break;}
					case CW_ERROR_GENERAL: {CWDebugLog("ERROR: General"); break;}
					case CW_ERROR_OPERATION_ABORTED: {CWDebugLog("ERROR: Operation Aborted"); break;}
					case CW_ERROR_SENDING: {CWDebugLog("ERROR: Sending"); break;}
					case CW_ERROR_RECEIVING: {CWDebugLog("ERROR: Receiving"); break;}
					case CW_ERROR_INVALID_FORMAT: {CWDebugLog("ERROR: Invalid Format"); break;}
					case CW_ERROR_TIME_EXPIRED: {CWDebugLog("ERROR: Time Expired"); break;}
					case CW_ERROR_NONE: {CWDebugLog("ERROR: None"); break;}
				}
			}
		}
		else break; // the message is fully reassembled
	}
	
	return CW_TRUE;
}


CWBool CWWTPSendAcknowledgedPacket(int seqNum, 
				   CWList msgElemlist,
				   CWBool (assembleFunc)(CWProtocolMessage **, int *, int, int, CWList, AP_TABLE * cur_AP),
				   CWBool (parseFunc)(char*, int, int, void*), 
				   CWBool (saveFunc)(void*, AP_TABLE * cur_AP),
				   void *valuesPtr,
				   AP_TABLE * cur_AP) {

	CWProtocolMessage *messages = NULL;
	CWProtocolMessage msg;
	int fragmentsNum = 0, i;

	struct timespec timewait;
	
	int gTimeToSleep = gCWRetransmitTimer;
	int gMaxTimeToSleep = CW_ECHO_INTERVAL_DEFAULT/2;

	msg.msg = NULL;
	
	if(!(assembleFunc(&messages, 
			  &fragmentsNum, 
			  gWTPPathMTU, 
			  seqNum, 
			  msgElemlist,
			  cur_AP))) {

		goto cw_failure;
	}
	
	cur_AP->WTPRetransmissionCount= 0;
	
	while(cur_AP->WTPRetransmissionCount < gCWMaxRetransmit) 
	{
//		CWLog("Transmission Num:%d", gWTPRetransmissionCount);
		for(i = 0; i < fragmentsNum; i++) 
		{
#ifdef CW_NO_DTLS
			if(!CWNetworkSendUnsafeConnected(cur_AP->WTPSocket, 
							 messages[i].msg,
							 messages[i].offset))
#else
			if(!CWSecuritySend(gWTPSession,
					   messages[i].msg, 
					   messages[i].offset))
#endif
			{
				CWLog("Failure sending Request");
				goto cw_failure;
			}
		}
		
		timewait.tv_sec = time(0) + gTimeToSleep;
		timewait.tv_nsec = 0;

		CW_REPEAT_FOREVER 
		{
			#if 0
			CWThreadMutexLock(&gInterfaceMutex);

			if (CWGetCountElementFromSafeList(gPacketReceiveList) > 0)
				CWErrorRaise(CW_ERROR_SUCCESS, NULL);
			else {
				if (CWErr(CWWaitThreadConditionTimeout(&gInterfaceWait, &gInterfaceMutex, &timewait)))
					CWErrorRaise(CW_ERROR_SUCCESS, NULL);
			}

			CWThreadMutexUnlock(&gInterfaceMutex);
			#endif

			switch(SimulatorEPollRead(cur_AP->WTPSocket, Epoll_fd[cur_AP->Epoll_fd_Index], cur_AP->CWDiscoveryIntervaluSec)) {

				case CW_ERROR_TIME_EXPIRED:
				{
					cur_AP->WTPRetransmissionCount++;
					goto cw_continue_external_loop;
					break;
				}

				case CW_ERROR_SUCCESS:
				{
					char buf[CW_BUFFER_SIZE];
					CWNetworkLev4Address addr;
					int readBytes;
					// ycc fix mutli_thread
					if(!CWErr(CWNetworkReceiveUnsafe(cur_AP->WTPSocket,
									 buf,
									 CW_BUFFER_SIZE-1,
									 0,
									 &addr,
									 &readBytes))) {
						return CW_FALSE;
					}
					#if 0
					/* there's something to read */
					if(!(CWReceiveMessage(&msg))) 
					{
						CW_FREE_PROTOCOL_MESSAGE(msg);
						CWLog("Failure Receiving Response");
						goto cw_failure;
					}
					#else
					//MyPrint(DEBUG_INFO,"size of buf is:%d\n",readBytes);
					//MyPrint(DATA_INFO,"Recv:",buf,readBytes);
					//exit(0);
					/* there's something to read */
					if(!(SimulatorReceiveMessage(&msg,buf,readBytes))) 
					{
						CW_FREE_PROTOCOL_MESSAGE(msg);
						CWLog("Failure Receiving Response");
						goto cw_failure;
					}
					#endif
					
					if(!(parseFunc(msg.msg, msg.offset, seqNum, valuesPtr))) 
					{
						if(CWErrorGetLastErrorCode() != CW_ERROR_INVALID_FORMAT) {

							CW_FREE_PROTOCOL_MESSAGE(msg);
							CWLog("Failure Parsing Response");
							goto cw_failure;
						}
						else {
							CWErrorHandleLast();
							{ 
								gWTPRetransmissionCount++;
								goto cw_continue_external_loop;
							}
							break;
						}
					}
					
					if((saveFunc(valuesPtr, cur_AP))) {

						goto cw_success;
					} 
					else {
						if(CWErrorGetLastErrorCode() != CW_ERROR_INVALID_FORMAT) {
							CW_FREE_PROTOCOL_MESSAGE(msg);
							CWLog("Failure Saving Response");
							goto cw_failure;
						} 
					}
					break;
				}

				case CW_ERROR_INTERRUPTED: 
				{
					cur_AP->WTPRetransmissionCount++;
					goto cw_continue_external_loop;
					break;
				}	
				default:
				{
					CWErrorHandleLast();
					CWDebugLog("Failure");
					goto cw_failure;
					break;
				}
			}
		}
		
		cw_continue_external_loop:
			CWDebugLog("Retransmission time is over");
			
			gTimeToSleep<<=1;
			if ( gTimeToSleep > gMaxTimeToSleep ) gTimeToSleep = gMaxTimeToSleep;
	}

	/* too many retransmissions */
	return CWErrorRaise(CW_ERROR_NEED_RESOURCE, "Peer Dead");
	
cw_success:	
	for(i = 0; i < fragmentsNum; i++) {
		CW_FREE_PROTOCOL_MESSAGE(messages[i]);
	}
	
	CW_FREE_OBJECT(messages);
	CW_FREE_PROTOCOL_MESSAGE(msg);
	
	return CW_TRUE;
	
cw_failure:
	if(messages != NULL) {
		for(i = 0; i < fragmentsNum; i++) {
			CW_FREE_PROTOCOL_MESSAGE(messages[i]);
		}
		CW_FREE_OBJECT(messages);
	}
	CWDebugLog("Failure");
	return CW_FALSE;
}

/* Elena Agostini - 03/2014: Retransmission Request Messages with custom interval */
CWBool CWWTPRequestPacketRetransmissionCustomTimeInterval(int retransmissionTimeInterval, 
				int seqNum, 
				CWProtocolMessage *messages,
				CWBool (parseFunc)(char*, int, int, void*), 
				CWBool (saveFunc)(void*),
				void *valuesPtr) {

	CWProtocolMessage msg;
	int fragmentsNum = 0, i;

	struct timespec timewait;
	
	int gTimeToSleep = retransmissionTimeInterval;
	int gMaxTimeToSleep = CW_ECHO_INTERVAL_DEFAULT/2;

	gWTPRetransmissionCount= 0;
	
	while(gWTPRetransmissionCount < gCWMaxRetransmit) 
	{
//		CWDebugLog("Transmission Num:%d", gWTPRetransmissionCount);
		for(i = 0; i < fragmentsNum; i++) 
		{
#ifdef CW_NO_DTLS
			if(!CWNetworkSendUnsafeConnected(gWTPSocket, 
							 messages[i].msg,
							 messages[i].offset))
#else
			if(!CWSecuritySend(gWTPSession,
					   messages[i].msg, 
					   messages[i].offset))
#endif
			{
				CWDebugLog("Failure sending Request");
				goto cw_failure;
			}
		}
		
		timewait.tv_sec = time(0) + gTimeToSleep;
		timewait.tv_nsec = 0;

		CW_REPEAT_FOREVER 
		{
			CWThreadMutexLock(&gInterfaceMutex);

			if (CWGetCountElementFromSafeList(gPacketReceiveList) > 0)
				CWErrorRaise(CW_ERROR_SUCCESS, NULL);
			else {
				if (CWErr(CWWaitThreadConditionTimeout(&gInterfaceWait, &gInterfaceMutex, &timewait)))
					CWErrorRaise(CW_ERROR_SUCCESS, NULL);
			}

			CWThreadMutexUnlock(&gInterfaceMutex);

			switch(CWErrorGetLastErrorCode()) {

				case CW_ERROR_TIME_EXPIRED:
				{
					gWTPRetransmissionCount++;
					goto cw_continue_external_loop;
					break;
				}

				case CW_ERROR_SUCCESS:
				{
					/* there's something to read */
					if(!(CWReceiveMessage(&msg))) 
					{
						CW_FREE_PROTOCOL_MESSAGE(msg);
						CWDebugLog("Failure Receiving Response");
						goto cw_failure;
					}
					
					if(!(parseFunc(msg.msg, msg.offset, seqNum, valuesPtr))) 
					{
						if(CWErrorGetLastErrorCode() != CW_ERROR_INVALID_FORMAT) {

							CW_FREE_PROTOCOL_MESSAGE(msg);
							CWDebugLog("Failure Parsing Response");
							goto cw_failure;
						}
						else {
							CWErrorHandleLast();
							{ 
								gWTPRetransmissionCount++;
								goto cw_continue_external_loop;
							}
							break;
						}
					}
					
					if((saveFunc(valuesPtr))) {

						goto cw_success;
					} 
					else {
						if(CWErrorGetLastErrorCode() != CW_ERROR_INVALID_FORMAT) {
							CW_FREE_PROTOCOL_MESSAGE(msg);
							CWDebugLog("Failure Saving Response");
							goto cw_failure;
						} 
					}
					break;
				}

				case CW_ERROR_INTERRUPTED: 
				{
					gWTPRetransmissionCount++;
					goto cw_continue_external_loop;
					break;
				}	
				default:
				{
					CWErrorHandleLast();
					CWDebugLog("Failure");
					goto cw_failure;
					break;
				}
			}
		}
		
		cw_continue_external_loop:
			CWDebugLog("Retransmission time is over");
			
			gTimeToSleep<<=1;
			if ( gTimeToSleep > gMaxTimeToSleep ) gTimeToSleep = gMaxTimeToSleep;
	}

	/* too many retransmissions */
	return CWErrorRaise(CW_ERROR_NEED_RESOURCE, "Peer Dead");
	
cw_success:	
	for(i = 0; i < fragmentsNum; i++) {
		CW_FREE_PROTOCOL_MESSAGE(messages[i]);
	}
	
	CW_FREE_OBJECT(messages);
	CW_FREE_PROTOCOL_MESSAGE(msg);
	
	return CW_TRUE;
	
cw_failure:
	if(messages != NULL) {
		for(i = 0; i < fragmentsNum; i++) {
			CW_FREE_PROTOCOL_MESSAGE(messages[i]);
		}
		CW_FREE_OBJECT(messages);
	}
	CWDebugLog("Failure");
	return CW_FALSE;
}

void GetConfig(char * CfgName)
{
	Cfg *m = CfgNew(CfgName);
	char * tempValue = NULL;
	if (m)
	{
		//DatabaseConfig
		(tempValue = GetValByKey("DatabaseConfig","mysql_addr",m))?strcpy(mysql_addr, tempValue):fprintf(stderr,"%s[DatabaseConfig]=>mysql_addr key error%s\n", COLOR_RED, COLOR_END);tempValue = NULL;
		(tempValue = GetValByKey("DatabaseConfig","mysql_user",m))?strcpy(mysql_user, tempValue):fprintf(stderr,"%s[DatabaseConfig]=>mysql_user key error%s\n", COLOR_RED, COLOR_END);tempValue = NULL;
		(tempValue = GetValByKey("DatabaseConfig","mysql_pwd",m))?strcpy(mysql_pwd, tempValue):fprintf(stderr,"%s[DatabaseConfig]=>mysql_pwd key error%s\n", COLOR_RED, COLOR_END);tempValue = NULL;
		(tempValue = GetValByKey("DatabaseConfig","mysql_database",m))?strcpy(mysql_database, tempValue):fprintf(stderr,"%s[DatabaseConfig]=>mysql_database key error%s\n", COLOR_RED, COLOR_END);tempValue = NULL;
		//NormalConfig
		(tempValue = GetValByKey("NormalConfig","EchoInterval",m))?EchoInterval = atoi(tempValue):fprintf(stderr,"%s[NormalConfig]=>EchoInterval key error%s\n", COLOR_RED, COLOR_END);tempValue = NULL;
		(tempValue = GetValByKey("NormalConfig","EchoRetryCount",m))?Max_Run_WaitCount = atoi(tempValue):fprintf(stderr,"%s[NormalConfig]=>EchoRetryCount key error%s\n", COLOR_RED, COLOR_END);tempValue = NULL;
		(tempValue = GetValByKey("NormalConfig","HasEcho",m))?HasEcho = atoi(tempValue):fprintf(stderr,"%s[NormalConfig]=>HasEcho key error%s\n", COLOR_RED, COLOR_END);tempValue = NULL;
		(tempValue = GetValByKey("NormalConfig","HasWTPEvent",m))?HasWTPEvent = atoi(tempValue):fprintf(stderr,"%s[NormalConfig]=>HasWTPEvent key error%s\n", COLOR_RED, COLOR_END);tempValue = NULL;
		(tempValue = GetValByKey("NormalConfig","HasCheckEchoRespone",m))?HasCheckEchoRespone = atoi(tempValue):fprintf(stderr,"%s[NormalConfig]=>HasCheckEchoRespone key error%s\n", COLOR_RED, COLOR_END);tempValue = NULL;
		//KeyConfig
		(tempValue = GetValByKey("KeyConfig","ApUplineInterval",m))?ApUplineInterval = atoi(tempValue):fprintf(stderr,"%s[KeyConfig]=>ApUplineInterval key error%s\n", COLOR_RED, COLOR_END);tempValue = NULL;
		(tempValue = GetValByKey("KeyConfig","DiscoverIntervalSec",m))?DiscoverIntervalSec = atoi(tempValue):fprintf(stderr,"%s[KeyConfig]=>DiscoverIntervalSec key error%s\n", COLOR_RED, COLOR_END);tempValue = NULL;
		(tempValue = GetValByKey("KeyConfig","DiscoverIntervalmSec",m))?DiscoverIntervalmSec = atoi(tempValue):fprintf(stderr,"%s[KeyConfig]=>DiscoverIntervalmSec key error%s\n", COLOR_RED, COLOR_END);tempValue = NULL;
		(tempValue = GetValByKey("KeyConfig","gCWMaxDiscoveries",m))?gCWMaxDiscoveries = atoi(tempValue):fprintf(stderr,"%s[KeyConfig]=>gCWMaxDiscoveries key error%s\n", COLOR_RED, COLOR_END);tempValue = NULL;
		(tempValue = GetValByKey("KeyConfig","WTP_RestartPort",m))?WTP_RestartPort = atoi(tempValue):fprintf(stderr,"%s[KeyConfig]=>WTP_RestartPort key error%s\n", COLOR_RED, COLOR_END);tempValue = NULL;
		(tempValue = GetValByKey("KeyConfig","gCWSilentInterval",m))?gCWSilentInterval = atoi(tempValue):fprintf(stderr,"%s[KeyConfig]=>gCWSilentInterval key error%s\n", COLOR_RED, COLOR_END);tempValue = NULL;
		(tempValue = GetValByKey("KeyConfig","FastSend",m))?FastSend = atoi(tempValue):fprintf(stderr,"%s[KeyConfig]=>FastSend key error%s\n", COLOR_RED, COLOR_END);tempValue = NULL;
	}
	CfgFree(m);
	fprintf(stderr,"%sConnect Mysql:%s/%s@%s use %s%s\n", COLOR_GREEN, mysql_user, mysql_pwd, mysql_addr, mysql_database, COLOR_END);
	fprintf(stderr,"%sEcho&WTP Event Interval:%d Echo&WTP RetryCount:%d Send Echo Enable:%d Report WTPEvent Enable:%d Check Echo Response Enable:%d%s\n", COLOR_GREEN, EchoInterval, Max_Run_WaitCount, HasEcho, HasWTPEvent, HasCheckEchoRespone, COLOR_END);
	fprintf(stderr,"%sDiscovery Interval:%ds Discovery ReSend Interval:%ds%dms DiscoveryRetryTime:%d%s\n", COLOR_GREEN, ApUplineInterval, DiscoverIntervalSec, DiscoverIntervalmSec, gCWMaxDiscoveries, COLOR_END);
	fprintf(stderr,"%sWTP Control Port restart from:%d Sulking Sleep time:%dms FastSend Enable:%d%s\n", COLOR_GREEN, WTP_RestartPort, gCWSilentInterval, FastSend, COLOR_END);
}

void SystemErrorHandler(int signum)  
{  
    const int len=1024;  
    void *func[len];  
    size_t size;  
    int i;  
    char **funs;  
  
    signal(signum,SIG_DFL);  
    size=backtrace(func,len);  
    funs=(char**)backtrace_symbols(func,size);  
    fprintf(stderr,"System error, Stack trace:\n");  
    for(i=0;i<size;++i) fprintf(stderr,"%d %s \n",i,funs[i]);  
    free(funs);  
	fprintf(stderr,"%s %d\n",__func__,__LINE__);//ycc care
    exit(1);  
}  

void getThreadSignal()  
{  
	sigset_t mask;  
	int rc;  
	int sig; 
	sigfillset(&mask);  
	rc = sigwait(&mask, &sig);  
	if (rc != 0)  
		fprintf(stderr,"%s,%d err\n", "sigwait",rc);  
	fprintf(stderr,"[%s] Signal handling thread got signal %d\n",__func__, sig); 
}  


void maskThreadSignal()  
{  
    sigset_t mask;  
    int rc;  
    sigfillset(&mask);  
    //这组线程阻塞所有的信号  
    rc = pthread_sigmask(SIG_BLOCK, &mask, NULL);  
}  

int main (int argc, const char * argv[]) {
	

	/* Daemon Mode */

	pid_t pid;
	
	if (argc <= 1)
		printf("Usage: WTP working_path\n");

	//config
	GetConfig("simulator_config");
	//end config

	if ((pid = fork()) < 0)
		exit(1);
	else if (pid != 0)
		exit(0);
	else {
		setsid();
		if (chdir(argv[1]) != 0){
			printf("chdir Faile\n");
			exit(1);
		}
		fclose(stdout);
	}	

	
	CWStateTransition nextState = CW_ENTER_DISCOVERY;
	//Elena to move line 611
	//CWLogInitFile(WTP_LOG_FILE_NAME);

//Elena: This is useless
/*
#ifndef CW_SINGLE_THREAD
	CWDebugLog("Use Threads");
#else
	CWDebugLog("Don't Use Threads");
#endif
*/
	CWErrorHandlingInitLib();
	if(!CWParseSettingsFile()){
		//Elena: fprintf
		fprintf(stderr, "Can't start WTP");
		exit(1);
	}
	
	//Elena Agostini - 05/2014
	CWLogInitFile(wtpLogFile);
	strncpy(gLogFileName, wtpLogFile, strlen(wtpLogFile));
	
	/* Capwap receive packets list */
	if (!CWErr(CWCreateSafeList(&gPacketReceiveList)))
	{
		CWLog("Can't start WTP");
		fprintf(stderr,"%s %d\n",__func__,__LINE__);//ycc care
		exit(1);
	}

	/* Capwap receive packets list */
	if (!CWErr(CWCreateSafeList(&gPacketReceiveDataList)))
	{
		CWLog("Can't start WTP");
		fprintf(stderr,"%s %d\n",__func__,__LINE__);//ycc care
		exit(1);
	}
	
	/* Capwap receive frame list */
	if (!CWErr(CWCreateSafeList(&gFrameList)))
	{
		CWLog("Can't start WTP");
		fprintf(stderr,"%s %d\n",__func__,__LINE__);//ycc care
		exit(1);
	}

	CWCreateThreadMutex(&gInterfaceMutex);
	CWSetMutexSafeList(gPacketReceiveList, &gInterfaceMutex);
	CWSetMutexSafeList(gFrameList, &gInterfaceMutex);
	CWCreateThreadCondition(&gInterfaceWait);
	CWSetConditionSafeList(gPacketReceiveList, &gInterfaceWait);
	CWSetConditionSafeList(gFrameList, &gInterfaceWait);

	//Elena Agostini: Mutex and Cond dedicated to Data Packet List
	CWCreateThreadMutex(&gInterfaceMutexData);
	CWCreateThreadCondition(&gInterfaceWaitData);
	CWSetMutexSafeList(gPacketReceiveDataList, &gInterfaceMutexData);
	CWSetConditionSafeList(gPacketReceiveDataList, &gInterfaceWaitData);


	CWLog("Starting WTP...");
	
	CWRandomInitLib();

	CWThreadSetSignals(SIG_BLOCK, 1, SIGALRM);
	maskThreadSignal();//ycc fix signal

	if (timer_init() == 0) {
		CWLog("Can't init timer module");
		fprintf(stderr,"%s %d\n",__func__,__LINE__);//ycc care
		exit(1);
	}


/* Elena Agostini - 04/2014: DTLS Data Channel || DTLS Control Channel */
#if defined(CW_NO_DTLS) && !defined(CW_DTLS_DATA_CHANNEL)
	if( !CWErr(CWWTPLoadConfiguration()) ) {
#else
	if( !CWErr(CWSecurityInitLib())	|| !CWErr(CWWTPLoadConfiguration()) ) {
#endif
		CWLog("Can't start WTP");
		fprintf(stderr,"%s %d\n",__func__,__LINE__);//ycc care
		exit(1);
	}

	CWDebugLog("Init WTP Radio Info");
	if(!CWWTPInitConfiguration())
	{
		CWLog("Error Init Configuration");
		fprintf(stderr,"%s %d\n",__func__,__LINE__);//ycc care
		exit(1);
	}

#ifdef SPLIT_MAC
	//We need monitor interface only in SPLIT_MAC mode with tunnel
	CWThread thread_receiveFrame;
	if(!CWErr(CWCreateThread(&thread_receiveFrame, CWWTPReceiveFrame, NULL))) {
		CWLog("Error starting Thread that receive binding frame");
		fprintf(stderr,"%s %d\n",__func__,__LINE__);//ycc care
		exit(1);
	}
#endif

/*
	CWThread thread_receiveStats;
	if(!CWErr(CWCreateThread(&thread_receiveStats, CWWTPReceiveStats, NULL))) {
		CWLog("Error starting Thread that receive stats on monitoring interface");
		exit(1);
	}
*/
	/****************************************
	 * 2009 Update:							*
	 *				Spawn Frequency Stats	*
	 *				Receiver Thread			*
	 ****************************************/
/*
	CWThread thread_receiveFreqStats;
	if(!CWErr(CWCreateThread(&thread_receiveFreqStats, CWWTPReceiveFreqStats, NULL))) {
		CWLog("Error starting Thread that receive frequency stats on monitoring interface");
		exit(1);
	}
	*/
	//ycc thread 初始化线程池
	{
		int i = 0;
		for (i = 0; i < THREAD_POOL_COUNT; i++)
		{
			threadpool[i] = pool_init (WORK_THREAD_NUM);/*线程池中最多四个活动线程*/ 
		}
	}
	//ycc epoll 初始化epoll//需要close，但是目前不会主动退出
	//ycc thread 初始化线程池mysql连接
	{
		int i = 0;
		for (i = 0; i < WORK_THREAD_NUM * THREAD_POOL_COUNT; i++)
		{
			Epoll_fd[i] = epoll_create(1);
			MySqlInit(&mysql_thread_pool[i]);
			if(Epoll_fd[i] < 0)
			{
				fprintf(stderr,"%s %d epoll_create:%d fail!\n",__func__,__LINE__,i);
				return 1;
			}
		}
	}
	//ycc thread 上线AP读取线程
	CWThread thread_getAPneedUpLine;
	if(!CWErr(CWCreateThread(&thread_getAPneedUpLine, SimulatorGetApUplineInfo, NULL))) {
		CWLog("Error starting Thread that get AP need upline info from mysql");
		fprintf(stderr,"%s %d\n",__func__,__LINE__);//ycc care
		exit(1);
	}
	//ycc thread 上线AP执行线程
	CWThread thread_APUpLine;
	if(!CWErr(CWCreateThread(&thread_APUpLine, SimulatorApUpline, NULL))) {
		CWLog("Error starting Thread that action AP upline");
		fprintf(stderr,"%s %d\n",__func__,__LINE__);//ycc care
		exit(1);
	}
	//ycc thread RUN AP读取线程
	CWThread thread_getAPneedRUN;
	if(!CWErr(CWCreateThread(&thread_getAPneedRUN, SimulatorGetApRunInfo, NULL))) {
		CWLog("Error starting Thread that get AP need run info from mysql");
		fprintf(stderr,"%s %d\n",__func__,__LINE__);//ycc care
		exit(1);
	}
	//ycc thread RUN AP执行线程
	CWThread thread_APRUN;
	if(!CWErr(CWCreateThread(&thread_APRUN, SimulatorApRun, NULL))) {
		CWLog("Error starting Thread that action AP run");
		fprintf(stderr,"%s %d\n",__func__,__LINE__);//ycc care
		exit(1);
	}
	//ycc thread RUN AP收包线程
	CWThread thread_APRUN_recv;
	if(!CWErr(CWCreateThread(&thread_APRUN_recv, SimulatorListeningEpoll, NULL))) {
		CWLog("Error starting Thread that recv AP run packet response");
		fprintf(stderr,"%s %d\n",__func__,__LINE__);//ycc care
		exit(1);
	}

	signal(SIGPIPE,SystemErrorHandler);
	signal(SIGFPE,SystemErrorHandler);
	signal(SIGSEGV,SystemErrorHandler); //Invaild memory address  
    signal(SIGABRT,SystemErrorHandler); // Abort signal  
	//ycc keep simulator running
	CW_REPEAT_FOREVER 
	{
		if (SimulatorQuit)
		{
			CWWTPDestroy(NULL);
			/*销毁线程池*/  
			int i = 0;
			for (i = 0; i < THREAD_POOL_COUNT; i++)
			{
	    		pool_destroy (&threadpool[i]);  
			}
			return 0;
		}
		getThreadSignal();
		sleep(120);
	}
	
	/* if AC address is given jump Discovery and use this address for Joining */
	if(gWTPForceACAddress != NULL)	nextState = CW_ENTER_JOIN;

	/* start CAPWAP state machine */	
	CW_REPEAT_FOREVER {
		switch(nextState) {
			case CW_ENTER_DISCOVERY:
				nextState = CWWTPEnterDiscovery(NULL);
				break;
			case CW_ENTER_SULKING:
				nextState = CWWTPEnterSulking(NULL);
				break;
			case CW_ENTER_JOIN:
				nextState = CWWTPEnterJoin(NULL);
				break;
			case CW_ENTER_CONFIGURE:
				nextState = CWWTPEnterConfigure(NULL);
				break;
			case CW_ENTER_DATA_CHECK:
				nextState = CWWTPEnterDataCheck(NULL);
				break;	
			case CW_ENTER_RUN:
				nextState = CWWTPEnterRun(NULL);
				break;
			case CW_ENTER_RESET:
				CWLog("------ Enter Reset State ------");
				nextState = CW_ENTER_DISCOVERY;
				break;
			case CW_QUIT:
				CWWTPDestroy(NULL);
				return 0;
		}
	}
}

__inline__ unsigned int CWGetSeqNum() {
	static unsigned int seqNum = 0;
	
	if (seqNum==CW_MAX_SEQ_NUM) seqNum=0;
	else seqNum++;
	return seqNum;
}

__inline__ int CWGetFragmentID() {
	static int fragID = 0;
	return fragID++;
}


/* 
 * Parses config file and inits WTP configuration.
 */
CWBool CWWTPLoadConfiguration() {
	int i;
	
	CWLog("WTP Loads Configuration");
	
	/* get saved preferences */
	if(!CWErr(CWParseConfigFile())) {
		CWLog("Can't Read Config File");
		fprintf(stderr,"%s %d\n",__func__,__LINE__);//ycc care
		exit(1);
	}
	
	if(gCWACCount == 0) 
		return CWErrorRaise(CW_ERROR_NEED_RESOURCE, "No AC Configured");
	
	CW_CREATE_ARRAY_ERR(gCWACList, 
			    gCWACCount,
			    CWACDescriptor,
			    return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););

	for(i = 0; i < gCWACCount; i++) {

		CWDebugLog("Init Configuration for AC at %s", gCWACAddresses[i]);
		CW_CREATE_STRING_FROM_STRING_ERR(gCWACList[i].address, gCWACAddresses[i],
						 return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
	}
	
	CW_FREE_OBJECTS_ARRAY(gCWACAddresses, gCWACCount);
	return CW_TRUE;
}

void CWWTPDestroy(AP_TABLE * cur_AP) {
	int i;
	
	CWLog("Destroy WTP");
	
	/*
	 * Elena Agostini - 07/2014: Memory leak
	 */
#ifndef CW_NO_DTLS
	if(gWTPSession)
		CWSecurityDestroySession(gWTPSession);
	if(gWTPSecurityContext)
		CWSecurityDestroyContext(gWTPSecurityContext);
	
	gWTPSecurityContext = NULL;
	gWTPSession = NULL;
#endif

	for(i = 0; i < gCWACCount; i++) {
		CW_FREE_OBJECT(gCWACList[i].address);
	}
	
	timer_destroy();
//Elena
#ifdef SPLIT_MAC
	close(rawInjectSocket);
#endif
	CW_FREE_OBJECT(gCWACList);
	CW_FREE_OBJECT(gRadiosInfo.radiosInfo);
}

CWBool CWWTPInitConfiguration() {
	int i, err;

	GetMaxRetryCount();//ycc fix

	//Generate 128-bit Session ID,
	initWTPSessionID(gWTPSessionID);
	
	CWWTPResetRebootStatistics(&gWTPRebootStatistics);
	
	//Elena Agostini - 07/2014: nl80211 support
	if(CWWTPGetRadioGlobalInfo() == CW_FALSE)
		//return CW_FALSE;//ycc fix simulator no radio
	fprintf(stderr,"Wtp simulator No Radio\n");
	return CW_TRUE;
}
