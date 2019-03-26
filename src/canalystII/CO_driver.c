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
#include "controlcan.h"
#include <string.h> /* for memcpy */
#include <stdlib.h> /* for malloc, free */
#include <errno.h>
#include <sys/socket.h>

#define DRIVER_DEVICE_TYPE   VCI_USBCAN2

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

        if(VCI_OpenDevice(DRIVER_DEVICE_TYPE, 0, 0) != 1) {//打开设备
            ret = CO_ERROR_ILLEGAL_ARGUMENT;
        } else {
            VCI_BOARD_INFO pInfo;//用来获取设备信息。
            //初始化参数，严格参数二次开发函数库说明书。
            VCI_INIT_CONFIG config;

            if(VCI_ReadBoardInfo(DRIVER_DEVICE_TYPE, 0, &pInfo) == 1) {//读取设备序列号、版本等信息。
                printf(">>Get VCI_ReadBoardInfo success!\n");

                //printf(" %08X", pInfo.hw_Version);printf("\n");
                //printf(" %08X", pInfo.fw_Version);printf("\n");
                //printf(" %08X", pInfo.dr_Version);printf("\n");
                //printf(" %08X", pInfo.in_Version);printf("\n");
                //printf(" %08X", pInfo.irq_Num);printf("\n");
                //printf(" %08X", pInfo.can_Num);printf("\n");
                printf(">>Serial_Num:");
                for (i = 0; i < 20; i++)
                    printf("%c", pInfo.str_Serial_Num[i]);
                printf("\n");

                printf(">>hw_Type:");
                for (i = 0; i < 10; i++)
                    printf("%c", pInfo.str_hw_Type[i]);
                printf("\n");	
            }
            
            config.AccCode=0;
            config.AccMask=0xFFFFFFFF;
            config.Filter=1;//接收所有帧
            config.Mode=0;//正常模式        
            switch(CANbitRate) {
                case 1000:
                    config.Timing0=0x00;
                    config.Timing1=0x14;
                break;
                case 500:
                    config.Timing0=0x00;/*波特率500Kbps  0x00  0x1C*/
                    config.Timing1=0x1C;
                break;
                case 250:
                    config.Timing0=0x01;
                    config.Timing1=0x1C;
                break;
                case 125:
                    config.Timing0=0x03;
                    config.Timing1=0x1C;
                break;
                case 100:
                    config.Timing0=0x04;
                    config.Timing1=0x1C;
                break;
                default:
                    config.Timing0=0x03;
                    config.Timing1=0x1C;
            }

            if(VCI_InitCAN(DRIVER_DEVICE_TYPE, 0, CANbaseAddress, &config) != 1) {
		        printf(">>Init can2 error\n");
                ret = CO_ERROR_ILLEGAL_ARGUMENT;
            } else {
                if(VCI_StartCAN(DRIVER_DEVICE_TYPE, 0, CANbaseAddress) != 1) {
		            printf(">>Start can2 error\n");
                    ret = CO_ERROR_ILLEGAL_ARGUMENT;
                }
            }
        }
    }

    return ret;
}


/******************************************************************************/
void CO_CANmodule_disable(CO_CANmodule_t *CANmodule){
    VCI_ResetCAN(DRIVER_DEVICE_TYPE, 0, CANmodule->CANbaseAddress);//复位CAN1通道。
    VCI_CloseDevice(DRIVER_DEVICE_TYPE, 0);//关闭设备。
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

        buffer->SendType = 0;
        buffer->ExternFlag = 0;//是否是扩展帧

        buffer->TimeStamp = 0;
        buffer->TimeFlag  = 0;
    }

    return buffer;
}


/******************************************************************************/
CO_ReturnError_t CO_CANsend(CO_CANmodule_t *CANmodule, CO_CANtx_t *buffer){
    CO_ReturnError_t err = CO_ERROR_NO;
    ssize_t n;

    n = VCI_Transmit(DRIVER_DEVICE_TYPE, 0, CANmodule->CANbaseAddress, (PVCI_CAN_OBJ)buffer, 1);
#ifdef CO_LOG_CAN_MESSAGES
    void CO_logMessage(const CanMsg *msg);
    CO_logMessage((const CanMsg*) buffer);
#endif

    if(n != 1){
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


/******************************************************************************/
void CO_CANrxWait(CO_CANmodule_t *CANmodule){
    VCI_CAN_OBJ msg;
    int i, n;
    CO_CANrxMsg_t *rcvMsg;      /* pointer to received message in CAN module */
    uint32_t rcvMsgIdent;       /* identifier of the received message */
    CO_CANrx_t *buffer;         /* receive message buffer from CO_CANmodule_t object. */
    bool_t msgMatched = false;

    if(CANmodule == NULL){
        errno = EFAULT;
        CO_errExit("CO_CANreceive - CANmodule not configured.");
    }

    if(!CANmodule->CANnormal)
        return;

    /* Read and pre-process message one by one */
    n = VCI_Receive(DRIVER_DEVICE_TYPE, 0, CANmodule->CANbaseAddress, &msg, 1, 0);
    while(n == 1){
        rcvMsg = (CO_CANrxMsg_t *) &msg;
        rcvMsgIdent = rcvMsg->ident;

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
            buffer->pFunct(buffer->object, rcvMsg);
        }

#ifdef CO_LOG_CAN_MESSAGES
        void CO_logMessage(const CanMsg *msg);
        CO_logMessage((CanMsg*)&rcvMsg);
#endif
        /* Read next message */
        n = VCI_Receive(DRIVER_DEVICE_TYPE, 0, CANmodule->CANbaseAddress, &msg, 1, 0);
    }
}

