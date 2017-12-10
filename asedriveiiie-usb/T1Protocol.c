#include "Ase.h"


static int SendSBlock (reader* globalData, uchar socket, uchar control, uchar data);

/*****************************************************************************
*
*****************************************************************************/
static uchar T1BlockGetType (T1Block* block) {
    if ((block->data[1] & 0x80) == T1_BLOCK_I)
        return T1_BLOCK_I;

    return (block->data[1] & 0xC0);
}

static int T1ErrorFreeRBlock (T1Block* block) {
    return ((block->data[1] & 0x0F) == 0);
}

static uchar T1BlockGetNS (T1Block* block) {
    return ((block->data[1] >> 6) & 0x01);
}

static uchar T1BlockGetMore (T1Block* block) {
    return ((block->data[1] >> 5) & 0x01);
}

static uchar T1BlockGetNR (T1Block* block) {
    return ((block->data[1] >> 4) & 0x01);
}

static int T1BlockGetLen (T1Block* block) {
    return block->data[2];
}

static uchar* T1BlockGetInf (T1Block* block) {
    if (block->len < 5)
        return NULL;

    return block->data + 3;
}


/*****************************************************************************
*
*****************************************************************************/
static void LRC(char* data, int len, uchar* edc) {
    int i;

    *edc = 0;

    for (i = 0 ; i < len ; ++i)
        (*edc) ^= data[i];
}


/*****************************************************************************
*
*****************************************************************************/
static void CRC(char* data, int len, uchar* edc1, uchar* edc2) {
    int i;
    unsigned short crc = 0xFFFF, temp, quick;

    for (i = 0 ; i < len ; ++i) {
        temp = (crc >> 8) ^ data[len];
        crc <<= 8;
        quick = temp ^ (temp >> 4);
        crc ^= quick;
        quick <<= 5;
        crc ^= quick;
        quick <<= 7;
        crc ^= quick;
    }

    *edc1 = (crc >> 8) & 0x00FF;
    *edc2 = crc & 0x00FF;
}


/*****************************************************************************
*
*****************************************************************************/
int T1InitProtocol (reader* globalData, char socket, char setIFSD) {
	int retVal;

    ATR *atr = &(globalData->cards[(int)socket].atr);

    /* set IFSC */
    globalData->cards[(int)socket].T1.ifsc = MIN(GetT1IFSC(atr), ATHENA_IFSC_MAX_SIZE);

    /* set EDC */
    globalData->cards[(int)socket].T1.edc = (GetT1EDC(atr) == 0 ? 1 : 0);

    /* set counters */
    globalData->cards[(int)socket].T1.ns = 1;
    globalData->cards[(int)socket].T1.nsCard = 0;

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "T1InitProtocol: ifsc = %d, edc = %d\n", 
            globalData->cards[(int)socket].T1.ifsc, globalData->cards[(int)socket].T1.edc);
#endif

    // Try to set the maximal ifsd - if it fails, use the default
//	for (i = 0 ; i < 3 ; ++i) {
    if (setIFSD) {
        T1Block* recBlock = &(globalData->cards[(int)socket].T1.recBlock);

		retVal = SendSBlock(globalData, socket, T1_BLOCK_S_IFS_REQ, 254/*globalData->cards[(int)socket].T1.ifsc*/);
        
        if (retVal == ASE_OK && T1BlockGetLen(recBlock) > 0x1) { // check Correct len
			// check that S(IFS response) has been received
			if (recBlock->data[1] == T1_BLOCK_S_IFS_RES) {
#ifdef ASE_DEBUG
				printf("<<<<<<<<< RECEIVING S(IFS response)\n");
#endif
		//		break;
			}
		}
    }

    return PROTOCOL_T1_OK;
}



/*****************************************************************************
*
*****************************************************************************/
static int T1CommandResponseProccessor(reader* globalData, char socket, 
                                       char* response, int len,
                                       char* outBuf, int* outBufLen) {
    int i;
    uchar readByte, edc1, edc2, edc11, edc22;

    *outBufLen = 0;

    if (len < 3)
        return PROTOCOL_T1_ERROR;

    readByte = response[0];
    outBuf[*outBufLen] = readByte;
    (*outBufLen)++;
    
    readByte = response[1];
    outBuf[*outBufLen] = readByte;
    (*outBufLen)++;
    
    readByte = response[2];
    outBuf[*outBufLen] = readByte;
    (*outBufLen)++;

    /* check that LEN doesnt exceed 254 */
    if (readByte > T1_BLOCK_INF_MAX_SIZE || len < (readByte + 3)) {
        return PROTOCOL_T1_ERROR;
    }

    /* read the information field */
    if (readByte > 0) {
        for (i = 0 ; i < readByte ; ++i) 
            outBuf[3 + i] = response[3 + i];
        (*outBufLen) += readByte;
    }
    
    /* read the EDC */
    if (globalData->cards[(int)socket].T1.edc == 1) { /* LRC */
        LRC(outBuf, *outBufLen, &edc11);

        if (len < (readByte + 4)) {
            return PROTOCOL_T1_ERROR;
        }

        edc1 = response[readByte + 3];
        outBuf[*outBufLen] = edc1;
        (*outBufLen)++;

        if (edc1 != edc11) {
            return PROTOCOL_T1_ERROR;
        }
    }
    else { /* CRC */
        CRC(outBuf, *outBufLen, &edc11, &edc22);

        if (len < (readByte + 5)) {
            return PROTOCOL_T1_ERROR;
        }

        edc1 = response[readByte + 3];
        outBuf[*outBufLen] = edc1;
        (*outBufLen)++;

        edc2 = response[readByte + 4];
        outBuf[*outBufLen] = edc2;
        (*outBufLen)++;

        if (edc1 != edc11 || edc2 != edc22) {
            return PROTOCOL_T1_ERROR;
        }
    }

    return ASE_OK;
}


/*****************************************************************************
*
*****************************************************************************/
/* the block to be sent is ready in T1.sendBlock -> send it using CPUCardTransact */
static int SendBlock (reader* globalData, uchar socket) {
    char response[BULK_BUFFER_SIZE];
    int retVal, len;
    T1Block* send = &(globalData->cards[(int)socket].T1.sendBlock);
    uchar edc1, edc2, LEN = send->data[2];

#ifdef ASE_DEBUG
    if (T1BlockGetType(send) == T1_BLOCK_I)
        syslog(LOG_INFO, ">>>>>>>> SENDING I(%d, %d)\n", T1BlockGetNS(send), T1BlockGetMore(send));
    else if (T1BlockGetType(send) == T1_BLOCK_R)
        syslog(LOG_INFO, ">>>>>>>> SENDING R(%d)\n", T1BlockGetNR(send));
    else if (T1BlockGetType(send) == T1_BLOCK_S) {
        switch (send->data[1]) {
        case T1_BLOCK_S_RESYNCH_REQ:
            syslog(LOG_INFO, ">>>>>>>> SENDING S(RESYNCH request)\n");
            break;
        case T1_BLOCK_S_RESYNCH_RES:
            syslog(LOG_INFO, ">>>>>>>> SENDING S(RESYNCH response)\n");
            break;
        case T1_BLOCK_S_IFS_REQ:
            syslog(LOG_INFO, ">>>>>>>> SENDING S(IFS request)\n");
            break;
        case T1_BLOCK_S_IFS_RES:
            syslog(LOG_INFO, ">>>>>>>> SENDING S(IFS = %d response)\n", send->data[3]);
            break;
        case T1_BLOCK_S_ABORT_REQ:
            syslog(LOG_INFO, ">>>>>>>> SENDING S(ABORT request)\n");
            break;
        case T1_BLOCK_S_ABORT_RES:
            syslog(LOG_INFO, ">>>>>>>> SENDING S(ABORT response)\n");
            break;
        case T1_BLOCK_S_WTX_REQ:
            syslog(LOG_INFO, ">>>>>>>> SENDING S(WTX request)\n");
            break;
        case T1_BLOCK_S_WTX_RES:
            syslog(LOG_INFO, ">>>>>>>> SENDING S(WTX = %d response)\n", send->data[3]);
            break;
        case T1_BLOCK_S_VPP_ERR:
            syslog(LOG_INFO, ">>>>>>>> SENDING S(VPP error)\n");
        }
    }

    syslog(LOG_INFO, "sendBlock: NAD = 0x%x%x, PCB = 0x%x%x, LEN = 0x%x%x sendBlock.len = %d\n", 
            PRINT_CHAR(globalData->cards[(int)socket].T1.sendBlock.data[0]),
            PRINT_CHAR(globalData->cards[(int)socket].T1.sendBlock.data[1]),
            PRINT_CHAR(globalData->cards[(int)socket].T1.sendBlock.data[2]),
    	    globalData->cards[(int)socket].T1.sendBlock.len);
#endif

    /* calculate the EDC */
    if (globalData->cards[(int)socket].T1.edc == 1) { /* LRC */
        LRC((char*)(send->data), send->len, &edc1);
        globalData->cards[(int)socket].T1.sendBlock.data[3 + LEN] = edc1; 
        globalData->cards[(int)socket].T1.sendBlock.len++;
    }
    else { /* CRC */
        CRC((char*)(send->data), send->len, &edc1, &edc2);
        globalData->cards[(int)socket].T1.sendBlock.data[3 + LEN] = edc1; 
        globalData->cards[(int)socket].T1.sendBlock.data[4 + LEN] = edc2; 
        globalData->cards[(int)socket].T1.sendBlock.len += 2;
    }
    
    retVal = T1CPUCardTransact(globalData, socket, 
                               (char*)globalData->cards[(int)socket].T1.sendBlock.data, 
			       globalData->cards[(int)socket].T1.sendBlock.len, 
                               response, &len);
    if (retVal < 0) {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "SendBlock - Error!\n");
#endif
        return retVal;
    }
                               
    retVal = T1CommandResponseProccessor(globalData, socket, 
                                         response, len,
                                         (char*)globalData->cards[(int)socket].T1.recBlock.data, 
            			                 &(globalData->cards[(int)socket].T1.recBlock.len));

#ifdef ASE_DEBUG
    if (retVal < 0)
        syslog(LOG_INFO, "SendBlock - Error!\n");
#endif

    return retVal;
}


/*****************************************************************************
*
*****************************************************************************/
static int SendIBlock (reader* globalData, uchar socket, uchar* data, int len, char more, int increaseNs) {
    uchar PCB = 0, LEN;
    int retVal, i;

    if (increaseNs)
        globalData->cards[(int)socket].T1.ns = (globalData->cards[(int)socket].T1.ns + 1) % 2;

    if (globalData->cards[(int)socket].T1.ns)
        PCB |= 0x40; /* N(s) */

    LEN = len;
    if (more)
        PCB |= 0x20; /* M=1 */
    else
	PCB &= 0xdf; /* M=0 */

    /* prepare the sendBlock */
    globalData->cards[(int)socket].T1.sendBlock.data[0] = 0x00; /* NAD */
    globalData->cards[(int)socket].T1.sendBlock.data[1] = PCB;  /* PCB */
    globalData->cards[(int)socket].T1.sendBlock.data[2] = LEN;  /* LEN */

    /* copy the INF data (if exists) */
    for (i = 0 ; i < LEN ; ++i)
        globalData->cards[(int)socket].T1.sendBlock.data[3 + i] = data[i];

    globalData->cards[(int)socket].T1.sendBlock.len = LEN + 3;

    retVal = SendBlock(globalData, socket);         
    
    return retVal;
}


/*****************************************************************************
*
*****************************************************************************/
/* data is invalid except in T1_IFS_S_BLOCK and T1_WTX_S_BLOCK  */
static int SendSBlock (reader* globalData, uchar socket, uchar control, uchar data) {
    uchar PCB = 0, LEN = 0;
    int retVal;


    PCB = control;
    if (PCB == T1_BLOCK_S_IFS_REQ || PCB == T1_BLOCK_S_IFS_RES || PCB == T1_BLOCK_S_WTX_RES)
	LEN = 1;     

    /* prepare the sendBlock */
    globalData->cards[(int)socket].T1.sendBlock.data[0] = 0x00; /* NAD */
    globalData->cards[(int)socket].T1.sendBlock.data[1] = PCB;  /* PCB */
    globalData->cards[(int)socket].T1.sendBlock.data[2] = LEN;  /* LEN */

    /* copy the INF data (if exists) */
    if (LEN)
        globalData->cards[(int)socket].T1.sendBlock.data[3] = data;

    globalData->cards[(int)socket].T1.sendBlock.len = LEN + 3;

    retVal = SendBlock(globalData, socket);         
    
    return retVal;
}


/*****************************************************************************
*
*****************************************************************************/
static int SendRBlock (reader* globalData, uchar socket, char nr) {
    int retVal;


    /* NAD */
    globalData->cards[(int)socket].T1.sendBlock.data[0] = 0x00;
    /* PCB */
    globalData->cards[(int)socket].T1.sendBlock.data[1] = 0x80;
    if (nr)
        globalData->cards[(int)socket].T1.sendBlock.data[1] |= 0x10;
    /* LEN */
    globalData->cards[(int)socket].T1.sendBlock.data[2] = 0x00;

    globalData->cards[(int)socket].T1.sendBlock.len = 3;

    retVal = SendBlock(globalData, socket);

    return retVal;
}


/*****************************************************************************
*
*****************************************************************************/
/* handles S-Block requests from the card */
static int ProcessSBlock(reader* globalData, uchar socket) {
    int retVal, retVal2, wwt, origBwt;
    T1Block* recBlock = &(globalData->cards[(int)socket].T1.recBlock);
    //    T1Block* sendBlock = &(globalData->cards[(int)socket].T1.sendBlock);
    struct card_params origCardParams = globalData->cards[(int)socket].cardParams, cardParams;
    uchar wtx;

    /* if this is WTX, update params including readers */
    if (recBlock->data[1] == T1_BLOCK_S_WTX_REQ) {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "<<<<<<<<< RECEIVING S(WTX request)\n");
#endif
        if (!T1BlockGetInf(recBlock))
            return PROTOCOL_T1_ERROR;

        cardParams = origCardParams;

        wtx = *(T1BlockGetInf(recBlock));
        wwt = cardParams.BWT[0] * 65536 + cardParams.BWT[1] * 256 + cardParams.CWT[2]; 
        wwt *= wtx;

        wwt += ASE_READER_ETU_DELTA;

        cardParams.BWT[0] = (uchar)((wwt & 0x00FF0000) >> 16);
        cardParams.BWT[1] = (uchar)((wwt & 0x0000FF00) >> 8);
        cardParams.BWT[2] = (uchar)(wwt & 0x000000FF);

        retVal = SetCardParameters(globalData, socket, cardParams);
        if (retVal < 0) {
            return retVal;
        }

	    origBwt = globalData->cards[(int)socket].bwt;
	    globalData->cards[(int)socket].bwt *= wtx;

        retVal = SendSBlock(globalData, socket, T1_BLOCK_S_WTX_RES, wtx);

        /* even if an error occured, we have to set the bwt to its original value */
        globalData->cards[(int)socket].bwt = origBwt;

        /* the next block has already been received, so we can reset the BWT of the 
         reader to its original values. */
        retVal2 = SetCardParameters(globalData, socket, origCardParams);
        
        // NOTE: the error of the SBlock is checked only after re-setting the reader params to the default
        if (retVal < 0) {
            return retVal;
        }
        if (retVal2 < 0) {
            return retVal;
        }
    }
    /* if this if IFS, update params */
    else if (recBlock->data[1] == T1_BLOCK_S_IFS_REQ) {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "<<<<<<<<< RECEIVING S(IFS request)\n");
#endif

        if (!T1BlockGetInf(recBlock))
            return PROTOCOL_T1_ERROR;

        globalData->cards[(int)socket].T1.ifsc = MIN(*T1BlockGetInf(recBlock), ATHENA_IFSC_MAX_SIZE);

        retVal = SendSBlock(globalData, socket, T1_BLOCK_S_IFS_RES, *T1BlockGetInf(recBlock));
        if (retVal < 0) {
            return retVal;
        }
    }
    /* if Abort -> response + return error code */
    else if (recBlock->data[1] == T1_BLOCK_S_ABORT_REQ) {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "<<<<<<<<< RECEIVING S(ABORT request)\n");
#endif

        retVal = SendSBlock(globalData, socket, T1_BLOCK_S_ABORT_RES, 0);
        if (retVal < 0) {
            return retVal;
        }

        return T1_ABORT_RECEIVED;
    }
    /* if RESYNCH response -> return error code */
    else if (recBlock->data[1] == T1_BLOCK_S_RESYNCH_RES) {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "<<<<<<<<< RECEIVING S(RESYNCH response)\n");
#endif

        return T1_RESYNCH_RECEIVED;
    }
    /* if VPP error -> return error code */
    else {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "<<<<<<<<< RECEIVING S(VPP error)\n");
#endif

        return T1_VPP_ERROR_RECEIVED;
    }

    /* Resynch request can only be sent by the interface device */
    
    return PROTOCOL_T1_OK;    
}



/*****************************************************************************
*
*****************************************************************************/
static int SendT1Command (reader* globalData, uchar socket, 
                   uchar* inBuf, int inBufLen, uchar* outBuffer, int *outBufLen) {
    uchar rsp_type;
    int bytes;
    char nr;
    int retVal, counter, retransmit = 0;
    char more, finished;
    T1Block* recBlock = &(globalData->cards[(int)socket].T1.recBlock);
    //    T1Block* sendBlock = &(globalData->cards[(int)socket].T1.sendBlock);


#ifdef ASE_DEBUG
    syslog(LOG_INFO, " SendT1Command - Enter\n");
#endif

    if ((retVal = cardCommandInit(globalData, socket, 1)))
        return retVal;

    /* calculate the number of bytes to send  */
    counter = 0;
    bytes = (inBufLen > globalData->cards[(int)socket].T1.ifsc ? globalData->cards[(int)socket].T1.ifsc : inBufLen);

    /* see if chaining is needed  */
    more = (inBufLen > globalData->cards[(int)socket].T1.ifsc);
    finished = 0;

    /*================================
     Sending the request to the card
    ================================ */

    /* send block of data */
    retVal = SendIBlock(globalData, socket, inBuf, bytes, more, 1);  			      	 

    while (!finished) {
        /* check if an error occured */
        if (retVal < 0) {
            if (retransmit == 3)
                return PROTOCOL_T1_ERROR;
            retransmit++;                
            retVal = SendRBlock(globalData, socket, globalData->cards[(int)socket].T1.ns);
        }
        else { /* the block received is valid */
            rsp_type = T1BlockGetType(recBlock);

            /* if all the data has been sent and the response from the card has arrived -> we are done here */
            if (!more && rsp_type == T1_BLOCK_I) {
                finished = 1;
            }
            else if (rsp_type == T1_BLOCK_R) {
                /* errorneous R-Block received */
                if (!T1ErrorFreeRBlock(recBlock)) {
#ifdef ASE_DEBUG
                    syslog(LOG_INFO, "<<<<<<<<< RECEIVING R(%d + ERROR)\n", T1BlockGetNR(recBlock));
#endif
                    if (retransmit == 3)
                        return PROTOCOL_T1_ERROR;
                    retransmit++;                
                    retVal = SendIBlock(globalData, socket, inBuf + counter, bytes, more, 0);  			      	 
                }
                /* positive ACK R-Block received -> send next I-Block (if there still data left to send) */
                else if (T1BlockGetNR(recBlock) != (globalData->cards[(int)socket].T1.ns)) {
#ifdef ASE_DEBUG
                    syslog(LOG_INFO, "<<<<<<<<< RECEIVING R(%d)\n", T1BlockGetNR(recBlock));
#endif
                    retransmit = 0;

                    if (more) {
                        /* calculate the number of bytes to send */ 
                        counter += bytes;
                        bytes = ((inBufLen - counter)> globalData->cards[(int)socket].T1.ifsc ? 
                                                globalData->cards[(int)socket].T1.ifsc : (inBufLen - counter));

                        /* see if chaining is needed  */
                        more = ((inBufLen - counter) > globalData->cards[(int)socket].T1.ifsc);

                        /* send block of data */
                        retVal = SendIBlock(globalData, socket, inBuf + counter, bytes, more, 1);  			      	 
                    }
                    else { /* no more data left to send -> send an R-Block to get acknowledgement */
                        retVal = SendRBlock(globalData, socket, globalData->cards[(int)socket].T1.ns);
                    }
                }
                /* retransmit the current block */
                else {
#ifdef ASE_DEBUG
                    syslog(LOG_INFO, "<<<<<<<<< RECEIVING R(%d)\n", T1BlockGetNR(recBlock));
#endif
                    if (retransmit == 3)
                        return PROTOCOL_T1_ERROR;
                    retransmit++;                
                    retVal = SendIBlock(globalData, socket, inBuf + counter, bytes, more, 0);  			      	 
                }
            }
            /* procees the S-Block request received */
            else if (rsp_type == T1_BLOCK_S) { 
                retVal = ProcessSBlock(globalData, socket);
                retransmit = 0;

                /* (scenario 20) */
                if (retVal == T1_ABORT_RECEIVED || retVal == T1_VPP_ERROR_RECEIVED) {
                    return retVal;
                }
            }
        }
    }

    /*===================================
     Getting the response from the card
    =================================== */

    counter = 0;      
    *outBufLen = 0; 
    more = 1;
    retransmit = 0;

    nr = T1BlockGetNS(recBlock);

    while (more) {
        /* check if an error occured */
        if (retVal < 0) {
            if (retransmit == 3)
                return PROTOCOL_T1_ERROR;
            retransmit++;                
            retVal = SendRBlock(globalData, socket, (globalData->cards[(int)socket].T1.nsCard + 1) % 2); 
        }
        else { /* the block received is valid */
            rsp_type = T1BlockGetType(recBlock);

            if (rsp_type == T1_BLOCK_I) {
#ifdef ASE_DEBUG
                syslog(LOG_INFO, "<<<<<<<<< RECEIVING I(%d,%d)\n", T1BlockGetNS(recBlock), T1BlockGetMore(recBlock));
#endif
                retransmit = 0;

		if (nr != T1BlockGetNS(recBlock))
		    return PROTOCOL_T1_ERROR;

                globalData->cards[(int)socket].T1.nsCard = T1BlockGetNS(recBlock);

                /* calculate nr */
                nr = (T1BlockGetNS(recBlock) + 1) % 2;

                /* save inf field */
                bytes = T1BlockGetLen(recBlock);
                if (bytes)
                    memcpy(outBuffer + counter, &(recBlock->data[3]), bytes);
                counter += bytes;
                *outBufLen = counter;

                /* see if chaining is requested */
                more = T1BlockGetMore(recBlock);

                if (more) {
                    retVal = SendRBlock(globalData, socket, nr);
                }
            }
            /* retransmit the current R-Block */
            else if ( rsp_type == T1_BLOCK_R) {
#ifdef ASE_DEBUG
                syslog(LOG_INFO, "<<<<<<<<< RECEIVING R(%d)\n", T1BlockGetNR(recBlock));
#endif
                if (retransmit == 3)
                    return PROTOCOL_T1_ERROR;
                retransmit++;                

                retVal = SendRBlock(globalData, socket, (globalData->cards[(int)socket].T1.nsCard + 1) % 2); 
            }
            /* procees the S-Block request received */
            else if (rsp_type == T1_BLOCK_S) { 
                retVal = ProcessSBlock(globalData, socket);
                retransmit = 0;

                /* if an Abort request is received from the card, check if the next received block is an R-Block:
                 if yes -> return with error, if not -> clear response and continue */
                if (retVal == T1_ABORT_RECEIVED) {
                    if (T1BlockGetType(recBlock) == T1_BLOCK_R) { /* the card gave up sending is response */
#ifdef ASE_DEBUG
                        syslog(LOG_INFO, "SendT1Command: card ABORT\n");
#endif
                        return retVal;
                    }
                    else { /* the received block is an S/I-Block -> the card begins retransmitting its response */
                        *outBufLen = counter = 0;
                        retVal = PROTOCOL_T1_OK; /* so that we could proceed */
                    }
                }
                else if (retVal < 0) {
#ifdef ASE_DEBUG
                    syslog(LOG_INFO, "SendT1Command: received an errorneous S-Block\n");
#endif
                    return retVal;
                }
            }
        }
    }

#ifdef ASE_DEBUG
    syslog(LOG_INFO, " SendT1Command - Exit\n");
#endif

    return retVal;
}


/*****************************************************************************
*
*****************************************************************************/
int T1Command (reader* globalData, uchar socket, 
               uchar* inBuf, int inBufLen, uchar* outBuffer, int *outBufLen) {
    int retVal, retransmitTimes = 0, i;
    T1Block* recBlock = &(globalData->cards[(int)socket].T1.recBlock);

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "\n T1Command - Enter\n================\n");
#endif

    retVal = SendT1Command(globalData, socket, inBuf, inBufLen, outBuffer, outBufLen);
    while (retVal < 0 && retransmitTimes < 3) {
        /* if the command failed, check the reason and try to recover */
        if (retVal < 0) {
            if (retVal == T1_VPP_ERROR_RECEIVED) {
#ifdef ASE_DEBUG
                syslog(LOG_INFO, "T1Command - Error! T1_VPP_ERROR_RECEIVED\n");
#endif
                return retVal; /* fatal error */
            }
            else if (retVal == T1_ABORT_RECEIVED) {
#ifdef ASE_DEBUG
                syslog(LOG_INFO, "T1Command - Error! T1_ABORT_RECEIVED - trying again\n");
#endif
                /* try to retransmit the command */
                retVal = SendT1Command(globalData, socket, inBuf, inBufLen, outBuffer, outBufLen);
                retransmitTimes++;
            }
            else { 
#ifdef ASE_DEBUG
                syslog(LOG_INFO, "T1Command - Error! retVal = %d\n", retVal);
#endif
                /* general error -> try sending a RESYNCH request up to 3 times */
                for (i = 0 ; (i < 3) && (retVal < 0) ; ++i) {
                    retVal = SendSBlock(globalData, socket, T1_BLOCK_S_RESYNCH_REQ, 0);
                    if (!retVal && T1BlockGetType(recBlock) == T1_BLOCK_S) { /* success - check if it's S(RESYNCH response) */
                        retVal = ProcessSBlock(globalData, socket);
                        if (retVal == T1_RESYNCH_RECEIVED)
                            retVal = PROTOCOL_T1_OK;
                    }
                    else
                        retVal = PROTOCOL_T1_ERROR; /* so that we could send RESYNCH req. again */
                }

                if (retVal < 0) { /* RESYNCH failed 3 times */
#ifdef ASE_DEBUG
                    syslog(LOG_INFO, "T1Command - Error! could not recover - resetting the card\n");
#endif
                    /*                    retVal = InitCard(globalData, socket, 0, &protocol); // warm reset ?????? */
                    return PROTOCOL_T1_ERROR; 
                }
                else { /* RESYNCH succeeded -> init protocol + retry command */
                    /* init the protocol */
                    retVal = T1InitProtocol(globalData, socket, 1);

                    /* try to transmit the command again */
                    retVal = SendT1Command(globalData, socket, inBuf, inBufLen, outBuffer, outBufLen);
                    retransmitTimes++;
                }
            }
        }
    }

    /*
#ifdef ASE_DEBUG
    syslog(LOG_INFO, "T1Command - response: ");
    for (i = 0 ; i < *outBufLen ; ++i)
	    syslog(LOG_INFO, " 0x%x%x", PRINT_CHAR(outBuffer[i]));
    syslog(LOG_INFO, "\n");
    
    syslog(LOG_INFO, " T1Command - Exit\n================\n\n");
#endif
    */

    return retVal;
}
