/*******************************************************************************************
 * Copyright (c) 2006-7 Laboratorio di Sistemi di Elaborazione e Bioingegneria Informatica *
 *                      Universita' Campus BioMedico - Italy                               *
 *                                                                                         *
 * This program is free software; you can redistribute it and/or modify it under the terms *
 * of the GNU General Public License as published by the Free Software Foundation; either  *
 * version 2 of the License, or (at your option) any later version.                        *
 *                                                                                         *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY         *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 	   *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.                *
 *                                                                                         *
 * You should have received a copy of the GNU General Public License along with this       *
 * program; if not, write to the:                                                          *
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston,                    *
 * MA  02111-1307, USA.                                                                    *
 *                                                                                         *
 * In addition, as a special exception, the copyright holders give permission to link the  *
 * code of portions of this program with the OpenSSL library under certain conditions as   *
 * described in each individual source file, and distribute linked combinations including  * 
 * the two. You must obey the GNU General Public License in all respects for all of the    *
 * code used other than OpenSSL.  If you modify file(s) with this exception, you may       *
 * extend this exception to your version of the file(s), but you are not obligated to do   *
 * so.  If you do not wish to do so, delete this exception statement from your version.    *
 * If you delete this exception statement from all source files in the program, then also  *
 * delete it here.                                                                         *
 * 
 * --------------------------------------------------------------------------------------- *
 * Project:  Capwap                                                                        *
 *                                                                                         *
 * Author :  Ludovico Rossi (ludo@bluepixysw.com)                                          *  
 *           Del Moro Andrea (andrea_delmoro@libero.it)                                    *
 *           Giovannini Federica (giovannini.federica@gmail.com)                           *
 *           Massimo Vellucci (m.vellucci@unicampus.it)                                    *
 *           Mauro Bisson (mauro.bis@gmail.com)                                            *
 *******************************************************************************************/


#include "CWWTP.h"
 
#ifdef DMALLOC
#include "../dmalloc-5.5.0/dmalloc.h"
#endif

/*________________________________________________________________*/
/*  *******************___CAPWAP VARIABLES___*******************  */
int gCWMaxDiscoveries = 3;//10 ycc fix//ycc config

/*_________________________________________________________*/
/*  *******************___VARIABLES___*******************  */
int gCWDiscoveryCount;

#ifdef CW_DEBUGGING
	int gCWDiscoveryInterval = 3; //5;
	int gCWMaxDiscoveryInterval = 4; //20;
#else
	int gCWDiscoveryInterval = 5;
	int gCWMaxDiscoveryInterval = 20;
#endif

/*_____________________________________________________*/
/*  *******************___MACRO___*******************  */
#define CWWTPFoundAnAC(cur_AP)	(cur_AP->ACInfoPtr != NULL /*&& gACInfoPtr->preferredAddress.ss_family != AF_UNSPEC*/)

/*__________________________________________________________*/
/*  *******************___PROTOTYPES___*******************  */
CWBool CWReceiveDiscoveryResponse();
void CWWTPEvaluateAC(CWACInfoValues *ACInfoPtr, AP_TABLE * cur_AP);// ycc fix mutli_thread
CWBool CWReadResponses();
CWBool CWAssembleDiscoveryRequest(CWProtocolMessage **messagesPtr, int seqNum, AP_TABLE * cur_AP);// ycc fix mutli_thread
CWBool CWParseDiscoveryResponseMessage(char *msg,
				       int len,
				       int *seqNumPtr,
				       CWACInfoValues *ACInfoPtr);

/*_________________________________________________________*/
/*  *******************___FUNCTIONS___*******************  */

/* 
 * Manage Discovery State
 */
pthread_mutex_t get_capwap_control_port_mutex;
pthread_mutex_t init_capwap_control_port_mutex;
int WTP_RestartPort = 5001;//ycc config
int DiscoverIntervalSec = 0;//ycc config
int DiscoverIntervalmSec = 250;//ycc config
CWStateTransition CWWTPEnterDiscovery(AP_TABLE * cur_AP) {
	int i;
	CWBool j;
	int ret= -10;
	
	CWLog("\n");	
	CWLog("######### Discovery State #########");
	
	/* reset Discovery state */
	CWThreadMutexLock(&get_capwap_control_port_mutex);
	cur_AP->WTP_PORT_CONTROL = (WTP_PORT_CONTROL++ % 65536);// ycc fix mutli_thread
	if (WTP_PORT_CONTROL >= 65536) WTP_PORT_CONTROL = WTP_RestartPort;// ycc fix mutli_thread
	cur_AP->WTP_PORT_DATA = WTP_PORT_DATA;
	CWThreadMutexUnlock(&get_capwap_control_port_mutex);
	cur_AP->CWDiscoveryCount = 0;// ycc fix mutli_thread
	if(cur_AP->WTPSocket)
	{
		Epoll_Del_Socket(cur_AP);
		CWNetworkCloseSocket(cur_AP->WTPSocket);// ycc fix mutli_thread
	}
	
	/* Elena Agostini - 06/2014: close WTP Data Socket before binding in Join state */
	if(cur_AP->WTPDataSocket)// ycc fix mutli_thread
		CWNetworkCloseSocket(cur_AP->WTPDataSocket);// ycc fix mutli_thread
	/* Elena Agostini - 04/2014: make control port always the same inside each WTP */
	CWThreadMutexLock(&init_capwap_control_port_mutex);
	if(!CWErr(CWNetworkInitSocketClientWithPort(&cur_AP->WTPSocket, NULL, cur_AP->WTP_PORT_CONTROL))) {// ycc fix mutli_thread
		CWThreadMutexUnlock(&init_capwap_control_port_mutex);
		//return CW_QUIT;
		return CW_ENTER_SULKING;
	}
	else
	{
		CWThreadMutexUnlock(&init_capwap_control_port_mutex);
	}
	Epoll_Add_Socket(cur_AP);
	//fprintf(stderr,"%s %d cur_AP->WTP_PORT_CONTROL:%d cur_AP->WTPSocket:%d\n",__func__,__LINE__,cur_AP->WTP_PORT_CONTROL,cur_AP->WTPSocket);//ycc test

	/* 
	 * note: gCWACList can be freed and reallocated (reading from config file)
	 * at each transition to the discovery state to save memory space
	 */
	cur_AP->CWACCount = gCWACCount = 1;// ycc fix mutli_thread only one //ycc config
	memcpy(&(cur_AP->CWACList), &gCWACList[0], sizeof(CWACDescriptor));// ycc fix mutli_thread//ycc config
	cur_AP->CWDiscoveryIntervalSec = DiscoverIntervalSec;//ycc config
	cur_AP->CWDiscoveryIntervaluSec = cur_AP->CWDiscoveryIntervalSec * 1000 + DiscoverIntervalmSec;//ycc config
	//for(i = 0; i < gCWACCount; i++) // ycc fix mutli_thread
		//gCWACList[i].received = CW_FALSE;// ycc fix mutli_thread gCWACList[i] replace to cur_AP->CWACList
		cur_AP->CWACList.received = CW_FALSE;

	/* wait a random time */
	//sleep(CWRandomIntInRange(gCWDiscoveryInterval, gCWMaxDiscoveryInterval));//fix ycc

	CW_REPEAT_FOREVER {
		CWBool sentSomething = CW_FALSE;
	
		/* we get no responses for a very long time */
		if(cur_AP->CWDiscoveryCount == gCWMaxDiscoveries)
		{
			return CW_ENTER_SULKING;
		}

		/* send Requests to one or more ACs */
		//for(i = 0; i < gCWACCount; i++) {

			/* if this AC hasn't responded to us... */
			if(!(cur_AP->CWACList.received)) {
				/* ...send a Discovery Request */

				CWProtocolMessage *msgPtr = NULL;
				
				/* get sequence number (and increase it) */
				cur_AP->CWACList.seqNum = CWGetSeqNum();
				
				if(!CWErr(CWAssembleDiscoveryRequest(&msgPtr,
								     cur_AP->CWACList.seqNum, 
								     cur_AP))) {// ycc fix mutli_thread
					fprintf(stderr,"%s %d\n",__func__,__LINE__);//ycc care
					exit(1);
				}
				// ycc fix mutli_thread gACInfoPtr replace to cur_AP->ACInfoPtr
                CW_CREATE_OBJECT_ERR(cur_AP->ACInfoPtr, CWACInfoValues, return CW_QUIT;);
				
				CWNetworkGetAddressForHost(cur_AP->CWACList.address, &(cur_AP->ACInfoPtr->preferredAddress));
				CWUseSockNtop(&(cur_AP->ACInfoPtr->preferredAddress), CWDebugLog(str););
				// ycc fix mutli_thread
				j = CWErr(CWNetworkSendUnsafeUnconnected(cur_AP->WTPSocket,
									 &(cur_AP->ACInfoPtr->preferredAddress),
									 (*msgPtr).msg,
									 (*msgPtr).offset)); 
				/* 
				 * log eventual error and continue
				 * CWUseSockNtop(&(gACInfoPtr->preferredAddress),
				 * 		 CWLog("WTP sends Discovery Request to: %s", str););
				 */
								
				CW_FREE_PROTOCOL_MESSAGE(*msgPtr);
				CW_FREE_OBJECT(msgPtr);
				CW_FREE_OBJECT(cur_AP->ACInfoPtr);
				
				/*
				 * we sent at least one Request in this loop
				 * (even if we got an error sending it) 
				 */
				sentSomething = CW_TRUE; 
			}
		//}
		
		/* All AC sent the response (so we didn't send any request) */
		if(!sentSomething && CWWTPFoundAnAC(cur_AP)) break;
		
		cur_AP->CWDiscoveryCount++;

		/* wait for Responses */
		//if(CWErr(CWReadResponses(cur_AP)) && CWWTPFoundAnAC(cur_AP)) {// ycc fix mutli_thread
		if(CWReadResponses(cur_AP) && CWWTPFoundAnAC(cur_AP)) {// ycc fix mutli_thread
			/* we read at least one valid Discovery Response */
			break;
		}
		
		CWLog("WTP Discovery-To-Discovery (%d)", cur_AP->CWDiscoveryCount);// ycc fix mutli_thread
	}
	
	CWLog("WTP Picks an AC");
	
	/* crit error: we should have received at least one Discovery Response */
	if(!CWWTPFoundAnAC(cur_AP)) {
		CWLog("No Discovery response Received");
		return CW_ENTER_DISCOVERY;
	}
	
	/* if the AC is multi homed, we select our favorite AC's interface */
	CWWTPPickACInterface(cur_AP);
		
	CWUseSockNtop(&(cur_AP->ACInfoPtr->preferredAddress),
			CWLog("Preferred AC: \"%s\", at address: %s", cur_AP->ACInfoPtr->name, str););
	return CW_ENTER_JOIN;
}

/* 
 * Wait DiscoveryInterval time while receiving Discovery Responses.
 */
CWBool CWReadResponses(AP_TABLE * cur_AP) {

	CWBool result = CW_FALSE;
	
	struct timeval timeout, before, after, delta, newTimeout;
	cur_AP->CWDiscoveryIntervaluSec += 100;//fix ycc
	//if (cur_AP->CWDiscoveryIntervaluSec >= 1000){cur_AP->CWDiscoveryIntervalSec++;cur_AP->CWDiscoveryIntervaluSec -= 1000;}
	timeout.tv_sec = newTimeout.tv_sec = cur_AP->CWDiscoveryIntervalSec;//gCWDiscoveryInterval;//fix ycc
	timeout.tv_usec = newTimeout.tv_usec = cur_AP->CWDiscoveryIntervaluSec;//0;//fix ycc
	CWDebugLog("cur_AP->ApIndex:%d cur_AP->CWDiscoveryInterval:%dus",cur_AP->ApIndex,cur_AP->CWDiscoveryIntervaluSec);
	
	gettimeofday(&before, NULL);

	CW_REPEAT_FOREVER {
		/* check if something is available to read until newTimeout */
		//if(CWNetworkTimedPollRead(cur_AP->WTPSocket, &newTimeout)) { // ycc fix mutli_thread
			/* success
			 * if there was no error, raise a "success error", so we can easily handle
			 * all the cases in the switch
			 */
			//CWErrorRaise(CW_ERROR_SUCCESS, NULL);
		//}

		//switch(CWErrorGetLastErrorCode()) {
		switch(SimulatorEPollRead(cur_AP->WTPSocket, Epoll_fd[cur_AP->Epoll_fd_Index], cur_AP->CWDiscoveryIntervaluSec)) {
			case CW_ERROR_TIME_EXPIRED:
				goto cw_time_over;
				break;
				
			case CW_ERROR_SUCCESS:
				result = CWReceiveDiscoveryResponse(cur_AP);// ycc fix mutli_thread
				goto cw_error;// ycc fix mutli_thread
			case CW_ERROR_INTERRUPTED: 
				/*
				 * something to read OR interrupted by the system
				 * wait for the remaining time (NetworkPoll will be recalled with the remaining time)
				 */
				gettimeofday(&after, NULL);

				CWTimevalSubtract(&delta, &after, &before);
				if(CWTimevalSubtract(&newTimeout, &timeout, &delta) == 1) { 
					/* negative delta: time is over */
					goto cw_time_over;
				}
				break;
			default:
				CWErrorHandleLast();
				goto cw_error;
				break;	
		}
	}
	cw_time_over:
		/* time is over */
		CWDebugLog("Timer expired during receive");	
	cw_error:
		return result;
}

/*
 * Gets a datagram from network that should be a Discovery Response.
 */
CWBool CWReceiveDiscoveryResponse(AP_TABLE * cur_AP) {
	char buf[CW_BUFFER_SIZE];
	int i;
	CWNetworkLev4Address addr;
	CWACInfoValues *ACInfoPtr;
	int seqNum;
	int readBytes;
	
	/* receive the datagram */
	// ycc fix mutli_thread
	if(!CWErr(CWNetworkReceiveUnsafe(cur_AP->WTPSocket,
					 buf,
					 CW_BUFFER_SIZE-1,
					 0,
					 &addr,
					 &readBytes))) {
		return CW_FALSE;
	}
	
        CW_CREATE_OBJECT_ERR(ACInfoPtr,
			     CWACInfoValues,
			     return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
	
	/* check if it is a valid Discovery Response */
	if(!CWErr(CWParseDiscoveryResponseMessage(buf, readBytes, &seqNum, ACInfoPtr))) {

		CW_FREE_OBJECT(ACInfoPtr);
		return CWErrorRaise(CW_ERROR_INVALID_FORMAT, 
				    "Received something different from a\
				     Discovery Response while in Discovery State");
	}

	CW_COPY_NET_ADDR_PTR(&(ACInfoPtr->incomingAddress), &(addr));

	/* see if this AC is better than the one we have stored */
	CWWTPEvaluateAC(ACInfoPtr, cur_AP);

	CWLog("WTP Receives Discovery Response");

	/* check if the sequence number we got is correct */
	// ycc fix mutli_thread gCWACList[i] replace to cur_AP->CWACList only one AC
	//for(i = 0; i < gCWACCount; i++) {
	
		if(cur_AP->CWACList.seqNum == seqNum) {
		
			CWUseSockNtop(&addr,
				      CWLog("Discovery Response from:%s", str););
			/* we received response from this address */
			cur_AP->CWACList.received = CW_TRUE;
	
			return CW_TRUE;
		}
	//}
	
	return CWErrorRaise(CW_ERROR_INVALID_FORMAT, 
			    "Sequence Number of Response doesn't macth Request");
}


void CWWTPEvaluateAC(CWACInfoValues *ACInfoPtr, AP_TABLE * cur_AP) {
	// ycc fix mutli_thread gACInfoPtr replace to cur_AP->ACInfoPtr

	if(ACInfoPtr == NULL) return;
	
	if(cur_AP->ACInfoPtr == NULL) { 
		/* 
		 * this is the first AC we evaluate: so
		 *  it's the best AC we examined so far
		 */
		cur_AP->ACInfoPtr = ACInfoPtr;

	} else {
		
		CW_FREE_OBJECT(ACInfoPtr);
	}
	/* 
	 * ... note: we can add our favourite algorithm to pick the best AC.
	 * We can also consider to remember all the Discovery Responses we 
	 * received and not just the best.
	 */
}

/*
 * Pick one interface of the AC (easy if there is just one interface). The 
 * current algorithm just pick the Ac with less WTP communicating with it. If
 * the addresses returned by the AC in the Discovery Response don't include the
 * address of the sender of the Discovery Response, we ignore the address in 
 * the Response and use the one of the sender (maybe the AC sees garbage 
 * address, i.e. it is behind a NAT).
 */
void CWWTPPickACInterface(AP_TABLE * cur_AP) {
// ycc fix mutli_thread gACInfoPtr replace to cur_AP->ACInfoPtr
	int i, min;
	CWBool foundIncoming = CW_FALSE;
	if(cur_AP->ACInfoPtr == NULL) return;
	
	cur_AP->ACInfoPtr->preferredAddress.ss_family = AF_UNSPEC;
	
	if(gNetworkPreferredFamily == CW_IPv6) {
		goto cw_pick_IPv6;
	}
	
cw_pick_IPv4:
	if(cur_AP->ACInfoPtr->IPv4Addresses == NULL || cur_AP->ACInfoPtr->IPv4AddressesCount <= 0) return;
		
	min = cur_AP->ACInfoPtr->IPv4Addresses[0].WTPCount;

	CW_COPY_NET_ADDR_PTR(&(cur_AP->ACInfoPtr->preferredAddress),
			     &(cur_AP->ACInfoPtr->IPv4Addresses[0].addr));
		
	for(i = 1; i < cur_AP->ACInfoPtr->IPv4AddressesCount; i++) {

		if(!sock_cmp_addr((struct sockaddr*)&(cur_AP->ACInfoPtr->IPv4Addresses[i]),
				  (struct sockaddr*)&(cur_AP->ACInfoPtr->incomingAddress),
				  sizeof(struct sockaddr_in))) foundIncoming = CW_TRUE;

		if(cur_AP->ACInfoPtr->IPv4Addresses[i].WTPCount < min) {

			min = cur_AP->ACInfoPtr->IPv4Addresses[i].WTPCount;
			CW_COPY_NET_ADDR_PTR(&(cur_AP->ACInfoPtr->preferredAddress), 
					     &(cur_AP->ACInfoPtr->IPv4Addresses[i].addr));
		}
	}
		
	if(!foundIncoming) {
		/* 
		 * If the addresses returned by the AC in the Discovery
		 * Response don't include the address of the sender of the
		 * Discovery Response, we ignore the address in the Response
		 * and use the one of the sender (maybe the AC sees garbage
		 * address, i.e. it is behind a NAT).
		 */
		CW_COPY_NET_ADDR_PTR(&(cur_AP->ACInfoPtr->preferredAddress),
				     &(cur_AP->ACInfoPtr->incomingAddress));
	}
	return;
		
cw_pick_IPv6:
	/* CWDebugLog("Pick IPv6"); */
	if(cur_AP->ACInfoPtr->IPv6Addresses == NULL ||\
	   cur_AP->ACInfoPtr->IPv6AddressesCount <= 0) goto cw_pick_IPv4;
		
	min = cur_AP->ACInfoPtr->IPv6Addresses[0].WTPCount;
	CW_COPY_NET_ADDR_PTR(&(cur_AP->ACInfoPtr->preferredAddress),
			     &(cur_AP->ACInfoPtr->IPv6Addresses[0].addr));
		
	for(i = 1; i < cur_AP->ACInfoPtr->IPv6AddressesCount; i++) {

		/*
		 * if(!sock_cmp_addr(&(gACInfoPtr->IPv6Addresses[i]),
		 * 		     &(gACInfoPtr->incomingAddress),
		 * 		     sizeof(struct sockaddr_in6))) 
		 *
		 * 	foundIncoming = CW_TRUE;
		 */
			
		if(cur_AP->ACInfoPtr->IPv6Addresses[i].WTPCount < min) {
			min = cur_AP->ACInfoPtr->IPv6Addresses[i].WTPCount;
			CW_COPY_NET_ADDR_PTR(&(cur_AP->ACInfoPtr->preferredAddress),
					     &(cur_AP->ACInfoPtr->IPv6Addresses[i].addr));
		}
	}
	/*
	if(!foundIncoming) {
		CW_COPY_NET_ADDR_PTR(&(gACInfoPtr->preferredAddress), 
				     &(gACInfoPtr->incomingAddress));
	}
	*/
	return;
}

CWBool CWAssembleDiscoveryRequest(CWProtocolMessage **messagesPtr, int seqNum, AP_TABLE * cur_AP) {

	CWProtocolMessage *msgElems= NULL;
	const int msgElemCount = 6 + cur_AP->RadioCount;// ycc fix mutli_thread
	CWProtocolMessage *msgElemsBinding= NULL;
	const int msgElemBindingCount=0;
	int k = -1;
	int fragmentsNum;

	if(messagesPtr == NULL) return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);

	CW_CREATE_PROTOCOL_MSG_ARRAY_ERR(msgElems, msgElemCount, return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););	
	
	/* Assemble Message Elements */
	// ycc fix mutli_thread
	if(
	   (!(CWAssembleMsgElemDiscoveryType(&(msgElems[++k])))) ||
	   (!(CWAssembleMsgElemWTPBoardData(&(msgElems[++k]), cur_AP)))	 ||
	   (!(CWAssembleMsgElemWTPDescriptor(&(msgElems[++k]), cur_AP))) ||
	   (!(CWAssembleMsgElemWTPFrameTunnelMode(&(msgElems[++k])))) ||
	   (!(CWAssembleMsgElemWTPMACType(&(msgElems[++k])))) ||
	   (!(CWAssembleMsgElemAPType(&(msgElems[++k]), cur_AP)))//ycc fix
	   )
	{
		int i;
		for(i = 0; i <= k; i++) { CW_FREE_PROTOCOL_MESSAGE(msgElems[i]);}
		CW_FREE_OBJECT(msgElems);
		/* error will be handled by the caller */
		return CW_FALSE;
	}
	
	//Elena Agostini - 07/2014: nl80211 support. 
	int indexWTPRadioInfo=0;
	#if 1
	// ycc fix mutli_thread
	for(indexWTPRadioInfo=0; indexWTPRadioInfo< cur_AP->RadioCount; indexWTPRadioInfo++)
	{
		if(!(CWAssembleMsgElemWTPRadioInformation( &(msgElems[++k]), gRadiosInfo.radiosInfo[indexWTPRadioInfo].gWTPPhyInfo.radioID, gRadiosInfo.radiosInfo[indexWTPRadioInfo].gWTPPhyInfo.phyStandardValue)))
		{
			int i;
			for(i = 0; i <= k; i++) { CW_FREE_PROTOCOL_MESSAGE(msgElems[i]);}
			CW_FREE_OBJECT(msgElems);
			/* error will be handled by the caller */
			return CW_FALSE;	
		}
	}
	#endif
	
	return CWAssembleMessage(messagesPtr, 
				 &fragmentsNum,
				 0,
				 seqNum,
				 CW_MSG_TYPE_VALUE_DISCOVERY_REQUEST,
				 msgElems,
				 msgElemCount,
				 msgElemsBinding,
				 msgElemBindingCount,
				 CW_PACKET_PLAIN,
				 cur_AP);
}

/*
 *  Parse Discovery Response and return informations in *ACInfoPtr.
 */
CWBool CWParseDiscoveryResponseMessage(char *msg, 
				       int len,
				       int *seqNumPtr,
				       CWACInfoValues *ACInfoPtr) {


	CWControlHeaderValues controlVal;
	CWProtocolTransportHeaderValues transportVal;
	int offsetTillMessages, i, j;
	char tmp_ABGNTypes;
	CWProtocolMessage completeMsg;
	
	if(msg == NULL || seqNumPtr == NULL || ACInfoPtr == NULL) 
		return CWErrorRaise(CW_ERROR_WRONG_ARG, NULL);
	
	CWDebugLog("Parse Discovery Response");
	
	completeMsg.msg = msg;
	completeMsg.offset = 0;
	
	CWBool dataFlag = CW_FALSE;
	/* will be handled by the caller */
	if(!(CWParseTransportHeader(&completeMsg, &transportVal, &dataFlag, NULL))) return CW_FALSE; 
	/* will be handled by the caller */
	if(!(CWParseControlHeader(&completeMsg, &controlVal))) return CW_FALSE;
	
	/* different type */
	if(controlVal.messageTypeValue != CW_MSG_TYPE_VALUE_DISCOVERY_RESPONSE)
		return CWErrorRaise(CW_ERROR_INVALID_FORMAT, "Message is not Discovery Response as Expected");
	
	
	*seqNumPtr = controlVal.seqNum;
	
	/* skip timestamp */
	controlVal.msgElemsLen -= CW_CONTROL_HEADER_OFFSET_FOR_MSG_ELEMS;

	offsetTillMessages = completeMsg.offset;
	
	ACInfoPtr->IPv4AddressesCount = 0;
	ACInfoPtr->IPv6AddressesCount = 0;
	/* parse message elements */
	while((completeMsg.offset-offsetTillMessages) < controlVal.msgElemsLen) {
		unsigned short int type=0;	/* = CWProtocolRetrieve32(&completeMsg); */
		unsigned short int len=0;	/* = CWProtocolRetrieve16(&completeMsg); */
		
		CWParseFormatMsgElem(&completeMsg,&type,&len);
	//	CWDebugLog("Parsing Message Element: %u, len: %u", type, len);
		
		switch(type) {
			case CW_MSG_ELEMENT_AC_DESCRIPTOR_CW_TYPE:
				/* will be handled by the caller */
				if(!(CWParseACDescriptor(&completeMsg, len, ACInfoPtr))) return CW_FALSE;
				break;
			case CW_MSG_ELEMENT_IEEE80211_WTP_RADIO_INFORMATION_CW_TYPE:
				/* will be handled by the caller */
				if(!(CWParseWTPRadioInformation_FromAC(&completeMsg, len, &tmp_ABGNTypes))) return CW_FALSE;
				break;
			case CW_MSG_ELEMENT_AC_NAME_CW_TYPE:
				/* will be handled by the caller */
				if(!(CWParseACName(&completeMsg, len, &(ACInfoPtr->name)))) return CW_FALSE;
				break;
			case CW_MSG_ELEMENT_CW_CONTROL_IPV4_ADDRESS_CW_TYPE:
				/* 
				 * just count how many interfacess we have, 
				 * so we can allocate the array 
				 */
				ACInfoPtr->IPv4AddressesCount++;
				completeMsg.offset += len;
				break;
			case CW_MSG_ELEMENT_CW_CONTROL_IPV6_ADDRESS_CW_TYPE:
				/* 
				 * just count how many interfacess we have, 
				 * so we can allocate the array 
				 */
				ACInfoPtr->IPv6AddressesCount++;
				completeMsg.offset += len;
				break;
			default:
				return CWErrorRaise(CW_ERROR_INVALID_FORMAT,
					"Unrecognized Message Element");
		}

		/* CWDebugLog("bytes: %d/%d",
		 * 	      (completeMsg.offset-offsetTillMessages),
		 * 	      controlVal.msgElemsLen); 
		 */
	}
	
	if (completeMsg.offset != len) 
		return CWErrorRaise(CW_ERROR_INVALID_FORMAT,
				    "Garbage at the End of the Message");
	
	/* actually read each interface info */
	CW_CREATE_ARRAY_ERR(ACInfoPtr->IPv4Addresses,
			    ACInfoPtr->IPv4AddressesCount,
			    CWProtocolIPv4NetworkInterface,
			    return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
	
	if(ACInfoPtr->IPv6AddressesCount > 0) {

		CW_CREATE_ARRAY_ERR(ACInfoPtr->IPv6Addresses,
				    ACInfoPtr->IPv6AddressesCount,
				    CWProtocolIPv6NetworkInterface,
				    return CWErrorRaise(CW_ERROR_OUT_OF_MEMORY, NULL););
	}

	i = 0, j = 0;
	
	completeMsg.offset = offsetTillMessages;
	while((completeMsg.offset-offsetTillMessages) < controlVal.msgElemsLen) {

		unsigned short int type=0;	/* = CWProtocolRetrieve32(&completeMsg); */
		unsigned short int len=0;	/* = CWProtocolRetrieve16(&completeMsg); */
		
		CWParseFormatMsgElem(&completeMsg,&type,&len);		
		
		switch(type) {
			case CW_MSG_ELEMENT_CW_CONTROL_IPV4_ADDRESS_CW_TYPE:
				/* will be handled by the caller */
				if(!(CWParseCWControlIPv4Addresses(&completeMsg,
								   len,
								   &(ACInfoPtr->IPv4Addresses[i]))))
					return CW_FALSE; 
				i++;
				break;
			case CW_MSG_ELEMENT_CW_CONTROL_IPV6_ADDRESS_CW_TYPE:
				/* will be handled by the caller */
				if(!(CWParseCWControlIPv6Addresses(&completeMsg,
								   len,
								   &(ACInfoPtr->IPv6Addresses[j])))) 
					return CW_FALSE;				
				j++;
				break;
			default:
				completeMsg.offset += len;
				break;
		}
	}
	return CW_TRUE;
}

