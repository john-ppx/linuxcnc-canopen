/*
 * CAN module object for Linux SocketCAN.
 *
 * @file        CO_driver.c
 * @author      Janez Paternoster
 * @copyright   2015 Janez Paternoster
 *
 * This file is part of CANopenNode, an opensource CANopen Stack.
 * Project home page is <https://github.com/CANopenNode/CANopenNode>.
 * For more information on CANopen see <http://www.can-cia.org/>.
 *
 * CANopenNode is free and open source software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include "CO_driver.h"
#include "CO_Emergency.h"
#include <stdio.h>
#include <string.h> /* for memcpy */
#include <stdlib.h> /* for malloc, free */
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

//#define SERVER_ADDR   "127.0.0.1"
#define SERVER_ADDR   "192.168.89.48"
#define SERVER_PORT   19048

/******************************************************************************/
#ifndef CO_SINGLE_THREAD
    pthread_mutex_t CO_EMCY_mtx = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t CO_OD_mtx = PTHREAD_MUTEX_INITIALIZER;
#endif


/** Set socketCAN filters *****************************************************/
static CO_ReturnError_t setFilters(CO_CANmodule_t *CANmodule){
    CO_ReturnError_t ret = CO_ERROR_NO;
    return ret;
}


/******************************************************************************/
void CO_CANsetConfigurationMode(int32_t CANbaseAddress){
}


/******************************************************************************/
void CO_CANsetNormalMode(CO_CANmodule_t *CANmodule){
    /* set CAN filters */
    if(CANmodule == NULL || setFilters(CANmodule) != CO_ERROR_NO){
        CO_errExit("CO_CANsetNormalMode failed");
    }
    CANmodule->CANnormal = true;
}


/******************************************************************************/
CO_ReturnError_t CO_CANmodule_init(
        CO_CANmodule_t         *CANmodule,
        int32_t                 CANbaseAddress,
        CO_CANrx_t              rxArray[],
        uint16_t                rxSize,
        CO_CANtx_t              txArray[],
        uint16_t                txSize,
        uint16_t                CANbitRate)
{
    CO_ReturnError_t ret = CO_ERROR_NO;
    uint16_t i;

    /* verify arguments */
    if(CANmodule==NULL || CANbaseAddress < 0 || rxArray==NULL || txArray==NULL){
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
    }

    /* Configure object variables */
    if(ret == CO_ERROR_NO){
        CANmodule->CANbaseAddress = CANbaseAddress;
        CANmodule->rxArray = rxArray;
        CANmodule->rxSize = rxSize;
        CANmodule->txArray = txArray;
        CANmodule->txSize = txSize;
        CANmodule->CANnormal = false;
        CANmodule->useCANrxFilters = true;
        CANmodule->bufferInhibitFlag = false;
        CANmodule->firstCANtxMessage = true;
        CANmodule->error = 0;
        CANmodule->CANtxCount = 0U;
        CANmodule->errOld = 0U;
        CANmodule->em = NULL;

#ifdef CO_LOG_CAN_MESSAGES
        CANmodule->useCANrxFilters = false;
#endif

        for(i=0U; i<rxSize; i++){
            rxArray[i].ident = 0U;
            rxArray[i].pFunct = NULL;
        }
        for(i=0U; i<txSize; i++){
            txArray[i].bufferFull = false;
        }
    }

    /* First time only configuration */
    if(ret == CO_ERROR_NO && CANmodule->wasConfigured == 0){
        CANmodule->wasConfigured = 1;

        CANmodule->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if(CANmodule->sockfd == -1) {
            perror("socket failed");
            return CO_ERROR_ILLEGAL_ARGUMENT;
        }
        
        /*2 准备通信地址*/
        CANmodule->addr.sin_family = AF_INET;
        /*设置为服务器进程的端口号*/
        CANmodule->addr.sin_port = htons(SERVER_PORT);
        /*服务器所在主机IP地址*/
        inet_aton(SERVER_ADDR, &CANmodule->addr.sin_addr);

        if (connect(CANmodule->sockfd, (struct sockaddr *)&CANmodule->addr,
            sizeof(CANmodule->addr)) == -1) {
            perror("connect failed");
            close(CANmodule->sockfd);
            return CO_ERROR_ILLEGAL_ARGUMENT;
        }

        int flags = fcntl(CANmodule->sockfd, F_GETFL, 0);
        fcntl(CANmodule->sockfd, F_SETFL, flags | O_NONBLOCK);
    }

    return ret;
}


/******************************************************************************/
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule){
    if(CANmodule==NULL)
        return;

    close(CANmodule->sockfd);
}


/******************************************************************************/
uint16_t CO_CANrxMsg_readIdent(const CO_CANrxMsg_t *rxMsg){
    return (uint16_t) rxMsg->ident;
}


/******************************************************************************/
CO_ReturnError_t CO_CANrxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        uint16_t                mask,
        bool_t                  rtr,
        void                   *object,
        void                  (*pFunct)(void *object, const CO_CANrxMsg_t *message))
{
    CO_ReturnError_t ret = CO_ERROR_NO;

    if((CANmodule!=NULL) && (object!=NULL) && (pFunct!=NULL) &&
       (index < CANmodule->rxSize)){
        /* buffer, which will be configured */
        CO_CANrx_t *buffer = &CANmodule->rxArray[index];

        /* Configure object variables */
        buffer->object = object;
        buffer->pFunct = pFunct;

        /* Configure CAN identifier and CAN mask, bit aligned with CAN module. */
        buffer->ident = ident;
        buffer->mask = mask;

        /* Set CAN hardware module filter and mask. */
        if(CANmodule->useCANrxFilters){
            if(CANmodule->CANnormal){
                ret = setFilters(CANmodule);
            }
        }
    }
    else{
        ret = CO_ERROR_ILLEGAL_ARGUMENT;
    }

    return ret;
}


/******************************************************************************/
CO_CANtx_t *CO_CANtxBufferInit(
        CO_CANmodule_t         *CANmodule,
        uint16_t                index,
        uint16_t                ident,
        bool_t                  rtr,
        uint8_t                 noOfBytes,
        bool_t                  syncFlag)
{
    CO_CANtx_t *buffer = NULL;

    if((CANmodule != NULL) && (index < CANmodule->txSize)){
        /* get specific buffer */
        buffer = &CANmodule->txArray[index];

        /* CAN identifier, bit aligned with CAN module registers */
        buffer->ident = ident;
        if(rtr){
            buffer->RemoteFlag = 1;//是否是远程帧
        }

        buffer->DLC = noOfBytes;
        buffer->bufferFull = false;
        buffer->syncFlag = syncFlag;
    }

    return buffer;
}


/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer){
    CO_ReturnError_t err = CO_ERROR_NO;
    char s[1024];        
    int len, n = 0, res;
    static int id = 0;

    if(!CANmodule->CANnormal || CANmodule->sockfd == -1)
        return CO_ERROR_DISCONNECT;

    id++;
    res = htonl(id);
    memcpy(s, &res, 4);
    res = htonl(buffer->ident);
    memcpy(s+4, &res, 4);
    *(s+8) = buffer->RemoteFlag;
    *(s+9) = buffer->DLC;
    for (int i = 0; i < 8; i++)
        *(s+10+i) = buffer->data[i];
    len = 17;

    n = send(CANmodule->sockfd, s, len, 0);
    //n = sendto(CANmodule->sockfd, s, len, 0,
    //        (struct sockaddr *)&CANmodule->addr,
    //        sizeof(CANmodule->addr));

#ifdef CO_LOG_CAN_MESSAGES
    void CO_logMessage(const CanMsg *msg);
    CO_logMessage((const CanMsg*) buffer);
#endif

    //printf("%d %s",n, s);
    if(n != len){
        printf("send %d %d %d\n", n, len, errno);
        CO_errorReport((CO_EM_t*)CANmodule->em, CO_EM_CAN_TX_OVERFLOW, CO_EMC_CAN_OVERRUN, n);
        err = CO_ERROR_TX_OVERFLOW;
    }

    return err;
}


/******************************************************************************/
void CO_CANclearPendingSyncPDOs(CO_CANmodule_t *CANmodule){
    /* Messages can not be cleared, because they are allready in kernel */
}


/******************************************************************************/
void CO_CANverifyErrors(CO_CANmodule_t *CANmodule){
#if 0
    unsigned rxErrors, txErrors;
    CO_EM_t* em = (CO_EM_t*)CANmodule->em;
    uint32_t err;

    canGetErrorCounters(CANmodule->CANbaseAddress, &rxErrors, &txErrors);
    if(txErrors > 0xFFFF) txErrors = 0xFFFF;
    if(rxErrors > 0xFF) rxErrors = 0xFF;

    err = ((uint32_t)txErrors << 16) | ((uint32_t)rxErrors << 8) | CANmodule->error;

    if(CANmodule->errOld != err){
        CANmodule->errOld = err;

        if(txErrors >= 256U){                               /* bus off */
            CO_errorReport(em, CO_EM_CAN_TX_BUS_OFF, CO_EMC_BUS_OFF_RECOVERED, err);
        }
        else{                                               /* not bus off */
            CO_errorReset(em, CO_EM_CAN_TX_BUS_OFF, err);

            if((rxErrors >= 96U) || (txErrors >= 96U)){     /* bus warning */
                CO_errorReport(em, CO_EM_CAN_BUS_WARNING, CO_EMC_NO_ERROR, err);
            }

            if(rxErrors >= 128U){                           /* RX bus passive */
                CO_errorReport(em, CO_EM_CAN_RX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, err);
            }
            else{
                CO_errorReset(em, CO_EM_CAN_RX_BUS_PASSIVE, err);
            }

            if(txErrors >= 128U){                           /* TX bus passive */
                if(!CANmodule->firstCANtxMessage){
                    CO_errorReport(em, CO_EM_CAN_TX_BUS_PASSIVE, CO_EMC_CAN_PASSIVE, err);
                }
            }
            else{
                bool_t isError = CO_isError(em, CO_EM_CAN_TX_BUS_PASSIVE);
                if(isError){
                    CO_errorReset(em, CO_EM_CAN_TX_BUS_PASSIVE, err);
                    CO_errorReset(em, CO_EM_CAN_TX_OVERFLOW, err);
                }
            }

            if((rxErrors < 96U) && (txErrors < 96U)){       /* no error */
                bool_t isError = CO_isError(em, CO_EM_CAN_BUS_WARNING);
                if(isError){
                    CO_errorReset(em, CO_EM_CAN_BUS_WARNING, err);
                    CO_errorReset(em, CO_EM_CAN_TX_OVERFLOW, err);
                }
            }
        }

        if(CANmodule->error & 0x02){                       /* CAN RX bus overflow */
            CO_errorReport(em, CO_EM_CAN_RXB_OVERFLOW, CO_EMC_CAN_OVERRUN, err);
        }
    }
#endif
}


static int recvMsg(CO_CANmodule_t *CANmodule, CO_CANrxMsg_t *msg) {
    static char s[1024];        
    static int n = 0, total = 0;
    int num;
    int ret = CO_ERROR_NO;

    if (recv(CANmodule->sockfd, s, 100, 0) <=0 ) {
        return CO_ERROR_TIMEOUT;
    }

    memcpy(&num, s, 4);
    num = ntohl(num);
    memcpy(&msg->ident, s+4, 4);
    msg->ident = ntohl(msg->ident);
    msg->RemoteFlag = *(s+8);
    msg->DLC = *(s+9);
    for (int i = 0; i < 8; i++)
        msg->data[i] = *(s+10+i);

    if ((n+1) != num) {
        total ++;
        printf("miss total %d(%d)", total, num);
    }

    return ret;
}

/******************************************************************************/
void CO_CANrxWait(CO_CANmodule_t *CANmodule){
    int i, ret;
    CO_CANrxMsg_t rcvMsg;      /* pointer to received message in CAN module */
    uint32_t rcvMsgIdent;       /* identifier of the received message */
    CO_CANrx_t *buffer;         /* receive message buffer from CO_CANmodule_t object. */
    bool_t msgMatched = false;

    if(CANmodule == NULL){
        errno = EFAULT;
        CO_errExit("CO_CANreceive - CANmodule not configured.");
    }

    if(!CANmodule->CANnormal || CANmodule->sockfd == -1)
        return;

    /* Read and pre-process message one by one */
    ret = recvMsg(CANmodule, &rcvMsg);
    while(ret == CO_ERROR_NO){
        rcvMsgIdent = rcvMsg.ident;

        //printf("%x %d %d\n", rcvMsg.ident, rcvMsg.RemoteFlag, rcvMsg.DLC);

        /* Search rxArray form CANmodule for the matching CAN-ID. */
        buffer = &CANmodule->rxArray[0];
        for(i = CANmodule->rxSize; i > 0U; i--){
            if(((rcvMsgIdent ^ buffer->ident) & buffer->mask) == 0U){
                msgMatched = true;
                break;
            }
            buffer++;
        }

        /* Call specific function, which will process the message */
        if(msgMatched && (buffer->pFunct != NULL)){
            buffer->pFunct(buffer->object, &rcvMsg);
        }

#ifdef CO_LOG_CAN_MESSAGES
        void CO_logMessage(const CanMsg *msg);
        CO_logMessage((CanMsg*)&rcvMsg);
#endif
        /* Read next message */
        ret = recvMsg(CANmodule, &rcvMsg);
    }
}

