#include "Ase.h"



/*****************************************************************************
* 
*****************************************************************************/
//#define ASE_PACKET_TYPE(pid, cnt, dst) (pid | cnt << 2 | dst)
#define ASE_PACKET_TYPE(pid, cnt, dst) (pid | dst)
#define ASE_STOP_BYTE(cnt, dst) (0x60 | cnt << 2 | dst)

#define T0_DATA_CHUNK_SIZE           28
#define T1_DATA_CHUNK_SIZE           32


/*****************************************************************************
* 
*****************************************************************************/
static void GetDefaultReaderParams (reader* globalData, struct card_params* params) {
    params->protocol = 0;
    params->N = 0x03;
    params->CWT[0] = 0x00;
    params->CWT[1] = 0x25/*0x85*/;
    params->CWT[2] = 0x85/*0xF0*/;
    params->BWT[0] = 0x00;
    params->BWT[1] = 0x3A/*0x38*/;
    params->BWT[2] = 0x34/*0xA4*/;
    /* 372 */
    params->A = 0x01;
    params->B = 0x74;
    params->freq = 0x02;
}




/*****************************************************************************
*
*****************************************************************************/
/* returns 0 on success */
int cardCommandInit (reader* globalData, char socket, char needToBePoweredOn) {
    if (!globalData->cards[(int)socket].status) {
        return ASE_READER_NO_CARD_ERROR;
    }

    if (needToBePoweredOn && globalData->cards[(int)socket].status != 2) {
        return ASE_READER_CARD_NOT_POWERED_ERROR;
    }

    return ASE_OK;
}


/*****************************************************************************
*
*****************************************************************************/
int readerCommandInit (reader* globalData, char needToBeStarted) {
    if (needToBeStarted && !globalData->readerStarted) {
        return ASE_ERROR_COMMAND;
    }

    return ASE_OK;
}


/*****************************************************************************
*
*****************************************************************************/
static void lock_mutex (reader* globalData) {
    int retVal;

#ifdef ASE_DEBUG
    /*    syslog(LOG_INFO, "  Trying...");*/
#endif
    
    retVal = pthread_mutex_lock(&(globalData->semaphore)); 
    if (retVal != 0) {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "Error! - in pthread_mutex_lock\n");
#endif
    }
    
#ifdef ASE_DEBUG
    /*    syslog(LOG_INFO, "Entering mutex\n");*/
#endif
}


/*****************************************************************************
*
*****************************************************************************/
static void unlock_mutex (reader* globalData) {
    int retVal;

#ifdef ASE_DEBUG
    /*    syslog(LOG_INFO, "  Trying...");*/
#endif
    
    retVal = pthread_mutex_unlock(&(globalData->semaphore)); 
    if (retVal != 0) {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "Error! - in pthread_mutex_unlock\n");
#endif
    }
    
#ifdef ASE_DEBUG
    /*    syslog(LOG_INFO, "Leaving mutex\n");*/
#endif
}


/*****************************************************************************
*
*****************************************************************************/
int ReaderStartup (reader* globalData, char* response, int* len) {
    char cmd[4];
    int retVal, i, retryTimes = 2;

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "\n ReaderStartup - Enter\n");
#endif

    if (globalData->readerStarted) {
        return ASE_ERROR_COMMAND;
    }

    globalData->readerStarted = 0;
    globalData->commandCounter = 1;
    globalData->cards[0].status = globalData->cards[0].ceei = globalData->cards[0].atr.length = 0;
    globalData->cards[1].status = globalData->cards[1].ceei = globalData->cards[1].atr.length = 0;
    globalData->cards[0].cwt = globalData->cards[1].cwt = 1500000; /* 1.5 sec */

    /* init mutex */
    retVal = pthread_mutex_init(&(globalData->semaphore), NULL); 
    if (retVal != 0)
        return ASE_ERROR_COMMAND; //??????????

    if ((retVal = readerCommandInit(globalData, 0)))
        return retVal;

    cmd[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, 0);
    globalData->commandCounter++;
    globalData->commandCounter %= 4;
    cmd[1] = 0x10;
    cmd[2] = 0x00;
    cmd[3] = cmd[0] ^ cmd[1] ^ cmd[2];

    do {
        lock_mutex(globalData);
        /* A Retry command should be sent */
        if (retVal == ASE_ERROR_DATA_INCOMPLETE || retVal == ASE_ERROR_CHECKSUM) {
            char cmd2[4];

            cmd2[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, 0);
            globalData->commandCounter++;
            globalData->commandCounter %= 4;
            cmd2[1] = 0x44;
            cmd2[2] = 0x0;
            cmd2[3] = cmd2[0] ^ cmd2[1] ^ cmd2[2];

            retVal = sendCloseResponseCommand(globalData, 0, cmd2, 4, response, len, 1);
        }
        /* The command should be re-sent to the reader */
        else {
            retVal = sendCloseResponseCommand(globalData, 0, cmd, 4, response, len, 1);
        }
        unlock_mutex(globalData);

        retryTimes--;
    } while (retVal != ASE_OK && retryTimes);

    if (retVal < 0) {
        return retVal; 
    }
    globalData->readerStarted = 1;

    /* copy response to the ASE data memory field */
/* first byte is communication data ??? */
    for (i = 1 ; i < (*len) ; ++i)
        globalData->dataMemory[i - 1] = response[i];

#ifdef ASE_DEBUG
    syslog(LOG_INFO, " ReaderStartup - Exit\n");
#endif

    return ASE_OK; 
}



/*****************************************************************************
*
*****************************************************************************/
int ReaderFinish (reader* globalData) {
    char cmd[4], ack;
    int retVal, actual, retryTimes = 2;

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "\n ReaderFinish - Enter\n");
#endif

    if ((retVal = readerCommandInit(globalData, 1)))
        return retVal;

    cmd[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, 0);
    globalData->commandCounter++;
    globalData->commandCounter %= 4;
    cmd[1] = 0x11;
    cmd[2] = 0x0;
    cmd[3] = cmd[0] ^ cmd[1] ^ cmd[2];

    do {
        lock_mutex(globalData);
        retVal = sendControlCommand(globalData, 0, cmd, 4, &ack, &actual, 0);
        unlock_mutex(globalData);

        retryTimes--;
    } while (retVal != ASE_OK && retryTimes);

    // if during the 3 tries the command failed, return an error status
    if (retVal < 0) {
        return retVal; 
    }
    
    if (ack != 0x20) {
        return parseStatus(ack);
    }

    globalData->readerStarted = 0;

#ifdef ASE_DEBUG
    syslog(LOG_INFO, " ReaderFinish - Exit\n");
#endif

    return ASE_OK; 
}


/*****************************************************************************
*
*****************************************************************************/
int GetStatus (reader* globalData, char* response, int* len) {
    char cmd[4];
    int retVal, retryTimes = 2;

    /*    syslog(LOG_INFO, "\n GetStatus - Enter\n"); */

    if ((retVal = readerCommandInit(globalData, 1)))
        return retVal;

    cmd[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, 0);
    globalData->commandCounter++;
    globalData->commandCounter %= 4;
    cmd[1] = 0x16;
    cmd[2] = 0x0;
    cmd[3] = cmd[0] ^ cmd[1] ^ cmd[2];

    do {
        lock_mutex(globalData);

        /* A Retry command should be sent */
        if (retVal == ASE_ERROR_DATA_INCOMPLETE || retVal == ASE_ERROR_CHECKSUM) {
            char cmd2[4];

            cmd2[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, 0);
            globalData->commandCounter++;
            globalData->commandCounter %= 4;
            cmd2[1] = 0x44;
            cmd2[2] = 0x0;
            cmd2[3] = cmd2[0] ^ cmd2[1] ^ cmd2[2];

            retVal = sendCloseResponseCommand(globalData, 0, cmd2, 4, response, len, 1);
        }
        /* The command should be re-sent to the reader */
        else {
            retVal = sendCloseResponseCommand(globalData, 0, cmd, 4, response, len, 1);
        }
        unlock_mutex(globalData);

        retryTimes--;
    } while (retVal != ASE_OK && retryTimes);

    // if during the 3 tries the command failed, return an error status
    if (retVal < 0) {
        return retVal; 
    }

    if (response[0] & 0x01) {
        if (!(globalData->cards[0].status)) /* only if not powered on */
	       globalData->cards[0].status = 1; 
    }
    else
	    globalData->cards[0].status = 0;

    if (response[0] & 0x02) {
        if (!(globalData->cards[1].status)) /* only if not powered on */
	        globalData->cards[1].status = 1;
    }
    else
	    globalData->cards[1].status = 0;

    /*    syslog(LOG_INFO, " GetStatus - Exit\n"); */

    return ASE_OK; 
}


/*****************************************************************************
*
*****************************************************************************/
int SetCardParameters (reader* globalData, char socket, struct card_params params) {
    char cmd[15], ack;
    int retVal, i, actual, retryTimes = 2;

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "\n SetCardParameters - Enter\n");
#endif

    if ((retVal = cardCommandInit(globalData, socket, 0)))
        return retVal;

    cmd[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, socket);
    globalData->commandCounter++;
    globalData->commandCounter %= 4;
    cmd[1] = 0x15;
    cmd[2] = 0x0B;
    cmd[3] = params.protocol;
    cmd[4] = params.N;
    cmd[5] = params.CWT[0];
    cmd[6] = params.CWT[1];
    cmd[7] = params.CWT[2];
    cmd[8] = params.BWT[0];
    cmd[9] = params.BWT[1];
    cmd[10] = params.BWT[2];
    cmd[11] = params.A;
    cmd[12] = params.B;
    cmd[13] = params.freq;
    cmd[14] = cmd[0];
    for (i = 1 ; i < 14 ; i++)
        cmd[14] ^= cmd[i];

    do {
        lock_mutex(globalData);
        retVal = sendControlCommand(globalData, socket, cmd, 15, &ack, &actual, 0);
        unlock_mutex(globalData);

        retryTimes--;
    } while (retVal != ASE_OK && retryTimes);

    // if during the 3 tries the command failed, return an error status
    if (retVal < 0) {
        return retVal; 
    }

    if (ack != 0x20) {
        return parseStatus(ack); 
    }

    globalData->cards[(int)socket].cardParams = params;

#ifdef ASE_DEBUG
    syslog(LOG_INFO, " SetCardParameters - Exit\n");
#endif

    return ASE_OK; 
}


/*****************************************************************************
*
*****************************************************************************/
int CardPowerOn (reader* globalData, char socket, uchar cardType, uchar voltage) {
    char cmd[6], response[300];
    int retVal, len, retryTimes = 2;
    ATR* atr = &(globalData->cards[(int)socket].atr);

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "\n CardPowerOn - Enter\n");
#endif

    if ((retVal = cardCommandInit(globalData, socket, 0)))
        return retVal;

    /*    GetDefaultReaderParams(globalData, &params);
       retVal = SetCardParameters(globalData, socket, params);
    if (retVal < 0) {
        return retVal; 
    }
    */

    cmd[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, socket);
    globalData->commandCounter++;
    globalData->commandCounter %= 4;
    cmd[1] = 0x20;
    cmd[2] = 0x02;
    cmd[3] = cardType;
    cmd[4] = voltage; 
    cmd[5] = cmd[0] ^ cmd[1] ^ cmd[2] ^ cmd[3] ^ cmd[4];

    /* CPU cards -> an open response should be receievd */
    if (cardType == 0x00 || cardType == 0x01) {
        do {
            lock_mutex(globalData);

            /* A Retry command should be sent */
            if (retVal == ASE_ERROR_DATA_INCOMPLETE || retVal == ASE_ERROR_CHECKSUM) {
                char cmd2[4];

                cmd2[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, socket);
                globalData->commandCounter++;
                globalData->commandCounter %= 4;
                cmd2[1] = 0x44;
                cmd2[2] = 0x0;
                cmd2[3] = cmd2[0] ^ cmd2[1] ^ cmd2[2];

                retVal = sendCloseResponseCommand(globalData, socket, cmd2, 4, response, &len, 0);
            }
            /* The command should be re-sent to the reader */
            else {
                retVal = sendCloseResponseCommand(globalData, socket, cmd, 6, response, &len, 0); 
            }
            unlock_mutex(globalData);

            retryTimes--;
        } while (retVal != ASE_OK && retryTimes);

        // if during the 3 tries the command failed, return an error status
        if (retVal < 0) {
            return retVal; 
        }

        retVal = ParseATR(globalData, socket, response, len);
    }
    /* 2/3BUS memory cards -> a close response should be received */
    else if (cardType == 0x11 || cardType == 0x12) {
        memset(atr, 0x00, sizeof(ATR));
        do {
            lock_mutex(globalData);

            /* A Retry command should be sent */
            if (retVal == ASE_ERROR_DATA_INCOMPLETE || retVal == ASE_ERROR_CHECKSUM) {
                char cmd2[4];

                cmd2[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, socket);
                globalData->commandCounter++;
                globalData->commandCounter %= 4;
                cmd2[1] = 0x44;
                cmd2[2] = 0x0;
                cmd2[3] = cmd2[0] ^ cmd2[1] ^ cmd2[2];

                retVal = sendCloseResponseCommand(globalData, socket, cmd2, 4, response, &len, 0);
            }
            /* The command should be re-sent to the reader */
            else {
                retVal = sendCloseResponseCommand(globalData, socket, cmd, 6, response, &len, 0); 
            }
            unlock_mutex(globalData);

            retryTimes--;
        } while (retVal != ASE_OK && retryTimes);

        // if during the 3 tries the command failed, return an error status
        if (retVal < 0) {
            return retVal; 
        }

        if (retVal == ASE_OK) {
            if (len) {
                memcpy(atr->data, response, len);
                atr->length = len;
            }
        }
    }
    /* X/I2C memory cards -> an acknowledgement should be received */
    else {
        memset(atr, 0x00, sizeof(ATR));
        do {
            lock_mutex(globalData);
            retVal = sendControlCommand(globalData, socket, cmd, 6, response, &len, 0); 
            unlock_mutex(globalData);

            retryTimes--;
        } while (retVal != ASE_OK && retryTimes);

        // if during the 3 tries the command failed, return an error status
        if (retVal < 0) {
            return retVal; 
        }
    }

    if (retVal < 0) {
        return retVal; 
    }

#ifdef ASE_DEBUG
    syslog(LOG_INFO, " CardPowerOn - Exit\n");
#endif

    return ASE_OK; 
}


/*****************************************************************************
*
*****************************************************************************/
int CPUCardReset (reader* globalData, char socket) {
    char cmd[4], response[300];
    int retVal, len, retryTimes = 2;
    struct card_params params;

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "\n CPUCardReset - Enter\n");
#endif

    if ((retVal = cardCommandInit(globalData, socket, 1)))
        return retVal;

    GetDefaultReaderParams(globalData, &params);
    retVal = SetCardParameters(globalData, socket, params);
    if (retVal < 0) {
        return retVal; 
    }

    cmd[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, socket);
    globalData->commandCounter++;
    globalData->commandCounter %= 4;
    cmd[1] = 0x22;
    cmd[2] = 0x00;
    cmd[3] = cmd[0] ^ cmd[1] ^ cmd[2];

    do {
        lock_mutex(globalData);

        /* A Retry command should be sent */
        if (retVal == ASE_ERROR_DATA_INCOMPLETE || retVal == ASE_ERROR_CHECKSUM) {
            char cmd2[4];

            cmd2[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, socket);
            globalData->commandCounter++;
            globalData->commandCounter %= 4;
            cmd2[1] = 0x44;
            cmd2[2] = 0x0;
            cmd2[3] = cmd2[0] ^ cmd2[1] ^ cmd2[2];

            retVal = sendCloseResponseCommand(globalData, socket, cmd2, 4, response, &len, 0);
        }
        /* The command should be re-sent to the reader */
        else {
            retVal = sendCloseResponseCommand(globalData, socket, cmd, 4, response, &len, 0); 
        }
        unlock_mutex(globalData);

        retryTimes--;
    } while (retVal != ASE_OK && retryTimes);

    // if during the 3 tries the command failed, return an error status
    if (retVal < 0) {
        return retVal; 
    }

    retVal = ParseATR(globalData, socket, response, len);
    if (retVal < 0) {
        return retVal; 
    }
    
#ifdef ASE_DEBUG
    syslog(LOG_INFO, " CPUCardReset - Exit\n");
#endif

    return ASE_OK; 
}


/*****************************************************************************
*
*****************************************************************************/
int CardPowerOff (reader* globalData, char socket) {
    char cmd[4], ack;
    int retVal, actual, retryTimes = 2;

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "\n CardPowerOff - Enter\n");
#endif

    if ((retVal = cardCommandInit(globalData, socket, 1)))
        return retVal;

    cmd[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, socket);
    globalData->commandCounter++;
    globalData->commandCounter %= 4;
    cmd[1] = 0x21;
    cmd[2] = 0x0;
    cmd[3] = cmd[0] ^ cmd[1] ^ cmd[2];

    do {
        lock_mutex(globalData);
        retVal = sendControlCommand(globalData, socket, cmd, 4, &ack, &actual, 0);
        unlock_mutex(globalData);

        retryTimes--;
    } while (retVal != ASE_OK && retryTimes);

    // if during the 3 tries the command failed, return an error status
    if (retVal < 0) {
        return retVal; 
    }

    if (ack != 0x20) {
        return parseStatus(ack); 
    }

    /* if the card is present, change the status to powered off */
    if (globalData->cards[(int)socket].status)
	    globalData->cards[(int)socket].status = 1; 

#ifdef ASE_DEBUG
    syslog(LOG_INFO, " CardPowerOff - Exit\n");
#endif

    return ASE_OK; 
}


/*****************************************************************************
*
*****************************************************************************/
/* on == 0x01, off == 0x00 */
int ChangeLedState (reader* globalData, char on) {
    char cmd[5], ack;
    int retVal, actual, retryTimes = 2;

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "\n ChangeLedState - Enter\n");
#endif

    if ((retVal = readerCommandInit(globalData, 1)))
        return retVal;

    cmd[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, 0);
    globalData->commandCounter++;
    globalData->commandCounter %= 4;
    cmd[1] = 0x17;
    cmd[2] = 0x01;
    cmd[3] = on;
    cmd[4] = cmd[0] ^ cmd[1] ^ cmd[2] ^ cmd[3];

    do {
        lock_mutex(globalData);
        retVal = sendControlCommand(globalData, 0, cmd, 5, &ack, &actual, 1);
        unlock_mutex(globalData);

        retryTimes--;
    } while (retVal != ASE_OK && retryTimes);

    // if during the 3 tries the command failed, return an error status
    if (retVal < 0) {
        return retVal; 
    }

    if (ack != 0x20) {
        return parseStatus(ack); 
    }

#ifdef ASE_DEBUG
    syslog(LOG_INFO, " ChangeLedState - Exit\n");
#endif

    return ASE_OK; 
}



/*****************************************************************************
*
*****************************************************************************/
int PPSTransact (reader* globalData, char socket, char* buffer, int len, char* outBuf, int* outBufLen) {
    int retVal;

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "\n PPSTransact - Enter\n");
#endif

    if ((retVal = cardCommandInit(globalData, socket, 0)))
        return retVal;

    retVal = CardCommand(globalData, socket, 0x43, (unsigned char*)buffer, len, (unsigned char*)outBuf, outBufLen);
    if (retVal < 0) {
        return retVal; 
    }

#ifdef ASE_DEBUG
    syslog(LOG_INFO, " PPSTransact - Exit\n");
#endif

    return ASE_OK; 
}




/*****************************************************************************
*
*****************************************************************************/
int MemoryCardTransact (reader* globalData, char socket, 
                        char cmdType, uchar command, ushort address, uchar len,
                        uchar* data, char* outBuf, int* outBufLen) {
    char cmd[BULK_BUFFER_SIZE];
    uchar checksum, readByte;
    int retVal, i, actual;

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "\n MemoryCardTransact - Enter\n");
#endif

    if ((retVal = cardCommandInit(globalData, socket, 1)))
        return retVal;

    cmd[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, socket);
    globalData->commandCounter++;
    globalData->commandCounter %= 4;
    cmd[1] = 0x25;
    cmd[2] = 5 + (cmdType == 0 ? len : 0);
    cmd[3] = cmdType;
    cmd[4] = command;
    cmd[5] = (address >> 8) & 0xFF;
    cmd[6] = address & 0xFF;
    cmd[7] = len;
    checksum = cmd[0] ^ cmd[1] ^ cmd[2] ^ cmd[3] ^ cmd[4] ^ cmd[5] ^ cmd[6] ^ cmd[7];
    i = 0;
    if (cmdType == 0) {
        for (; i < len ; ++i) {
            cmd[i + 8] = data[i]; 
            checksum ^= cmd[i + 8];
        }
    }
    cmd[i + 8] = checksum;

    lock_mutex(globalData);

    /* send the command */
    retVal = writeToReader(globalData, cmd, cmd[2] + 4, &actual);
    if (retVal < 0) {
        return retVal; 
    }

    checksum = 0;

    /* read the Ack control byte or the close response packet type */
    retVal = readResponse(globalData, socket, 1, (char*)&readByte, &actual, 1000000);
    if (retVal < 0) {
	    unlock_mutex(globalData);
        return retVal; 
    }

    while ((readByte & 0xF0) != 0x10) {
        if ((readByte & 0xF0) == 0x20) {
            if (readByte != 0x20) {
    	        unlock_mutex(globalData);
                return parseStatus(readByte);
            }
	    else
		break;
        }
        else if (isEvent(readByte)) { 
#ifdef ASE_DEBUG
	        syslog(LOG_INFO, "MemoryCardTransact - Event 0x%x%x.\n", (readByte & 0xF0) >> 4, readByte & 0x0F);
#endif
	        parseEvent(globalData, socket, readByte);
        }

        retVal = readResponse(globalData, socket, 1, (char*)(&readByte), &actual, 1000000);
        if (retVal < 0) {
	        unlock_mutex(globalData);
            return retVal; 
        }
    }

    checksum ^= readByte;

    /* if readByte == 0x2x */
    if ((readByte & 0xF0) == 0x20) {
        if (readByte != 0x20) {
/*            *outBufLen = 0; */
    	    unlock_mutex(globalData);
            return parseStatus(readByte);
        }
    }
    /* ths response is a close response */
    else if ((readByte & 0xF0) == 0x10) {
        if (readByte != 0x10) {
    	    unlock_mutex(globalData);
            return parseStatus(readByte); 
        }

        /* read data length */
	    retVal = readResponse(globalData, socket, 1, (char*)&readByte, &actual, 1000000);
        if (retVal < 0) {
    	    unlock_mutex(globalData);
            return retVal; 
        }

        checksum ^= readByte;

        /* read data  */
        retVal = readResponse(globalData, socket, readByte, outBuf, outBufLen, 1000000);
        if (retVal < 0 || (*outBufLen) != readByte) {
    	    unlock_mutex(globalData);
            return retVal; 
        }

        for (i = 0 ; i < *outBufLen ; ++i)
            checksum ^= outBuf[i];

        /* read the checksum */
        retVal = readResponse(globalData, socket, 1, (char*)&readByte, &actual, 1000000);
        if (retVal < 0) {
    	    unlock_mutex(globalData);
            return retVal; 
        }

        if (checksum != readByte) {
    	    unlock_mutex(globalData);
            return ASE_ERROR_CHECKSUM; 
        }
    }

    unlock_mutex(globalData);

#ifdef ASE_DEBUG
    syslog(LOG_INFO, " MemoryCardTransact - Exit\n\n");
#endif

    return ASE_OK; 
}


/*****************************************************************************
*
*****************************************************************************/
int T0Read (reader* globalData, char socket, 
            uchar* buffer, int len, uchar* outBuf, int* outBufLen) {
    int retVal;

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "\n T0Read - Enter\n");
#endif

    if ((retVal = cardCommandInit(globalData, socket, 1)))
        return retVal;

    retVal = CardCommand(globalData, socket, 0x41, buffer, len, outBuf, outBufLen);
    if (retVal < 0) {
        return retVal; 
    }

#ifdef ASE_DEBUG
    syslog(LOG_INFO, " T0Read - Exit\n");
#endif

    return ASE_OK; 
}


/*****************************************************************************
*
*****************************************************************************/
int T0Write (reader* globalData, char socket, 
             uchar* buffer, int len, uchar* outBuf, int* outBufLen) {
    int retVal;

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "\n T0Write - Enter\n");
#endif

    if ((retVal = cardCommandInit(globalData, socket, 1)))
        return retVal;

    retVal = CardCommand(globalData, socket, 0x40, buffer, len, outBuf, outBufLen);
    if (retVal < 0) {
        return retVal; 
    }

#ifdef ASE_DEBUG
    syslog(LOG_INFO, " T0Write - Exit\n");
#endif

    return ASE_OK; 
}


/*****************************************************************************
*
*****************************************************************************/
int T1CPUCardTransact (reader* globalData, char socket, 
                       char* buffer, int len, char* outBuf, int* outBufLen) {
    int retVal;

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "\n T1CPUCardTransact - Enter\n");
#endif

    if ((retVal = cardCommandInit(globalData, socket, 1)))
        return retVal;

    retVal = CardCommand(globalData, socket, 0x42, (unsigned char*)buffer, len, (unsigned char*)outBuf, outBufLen);
    if (retVal < 0) {
        return retVal; 
    }

#ifdef ASE_DEBUG
    syslog(LOG_INFO, " T1CPUCardTransact - Exit\n");
#endif

    return ASE_OK; 
}


/*****************************************************************************
*
*****************************************************************************/
int CardCommand (reader* globalData, char socket, char command, 
                 uchar* buffer, int len, uchar* outBuf, int* outBufLen) {
    char cmd[BULK_BUFFER_SIZE];
    uchar checksum;
    int retVal = ASE_OK, i, retryTimes = 2;

    /* short format */
    if (len <= 255) {
        cmd[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, socket);
        globalData->commandCounter++;
        globalData->commandCounter %= 4;
        cmd[1] = command;
        cmd[2] = (uchar)len;
        checksum = cmd[0] ^ cmd[1] ^ cmd[2];
        i = 0;
        for (; i < len ; ++i) {
            cmd[i + 3] = buffer[i]; 
            checksum ^= cmd[i + 3];
        }
        cmd[i + 3] = checksum;

        do {
            lock_mutex(globalData);

            /* A Retry command should be sent */
            if (retVal == ASE_ERROR_DATA_INCOMPLETE || retVal == ASE_ERROR_CHECKSUM) {
                char cmd2[4];

                cmd2[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, socket);
                globalData->commandCounter++;
                globalData->commandCounter %= 4;
                cmd2[1] = 0x44;
                cmd2[2] = 0x0;
                cmd2[3] = cmd2[0] ^ cmd2[1] ^ cmd2[2];

                retVal = sendCloseResponseCommand(globalData, socket, cmd2, 4, (char*)outBuf, outBufLen, 0);
            }
            /* The command should be re-sent to the reader */
            else {
                retVal = sendCloseResponseCommand(globalData, socket, cmd, len + 4, (char*)outBuf, outBufLen, 0); 
            }
            unlock_mutex(globalData);

            retryTimes--;
        } while (retVal != ASE_OK && retryTimes);
    }
    /* long format */
    else {
        cmd[0] = ASE_PACKET_TYPE(0xD0, globalData->commandCounter, socket);
        globalData->commandCounter++;
        globalData->commandCounter %= 4;
        cmd[1] = command;
        cmd[2] = (len >> 8) & 0xFF;
        cmd[3] = len & 0xFF;
        checksum = cmd[0] ^ cmd[1] ^ cmd[2] ^ cmd[3];
        i = 0;
        for (; i < len ; ++i) {
            cmd[i + 4] = buffer[i]; 
            checksum ^= cmd[i + 4];
        }
        cmd[i + 4] = checksum;

        do {
            lock_mutex(globalData);

            /* A Retry command should be sent */
            if (retVal == ASE_ERROR_DATA_INCOMPLETE || retVal == ASE_ERROR_CHECKSUM) {
                char cmd2[4];

                cmd2[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, socket);
                globalData->commandCounter++;
                globalData->commandCounter %= 4;
                cmd2[1] = 0x44;
                cmd2[2] = 0x0;
                cmd2[3] = cmd2[0] ^ cmd2[1] ^ cmd2[2];

                retVal = sendCloseResponseCommand(globalData, socket, cmd2, 4, (char*)outBuf, outBufLen, 0);
            }
            /* The command should be re-sent to the reader */
            else {
                retVal = sendCloseResponseCommand(globalData, socket, cmd, len + 5, (char*)outBuf, outBufLen, 0); 
            }
            unlock_mutex(globalData);

            retryTimes--;
        } while (retVal != ASE_OK && retryTimes);
    }

    if (retVal < 0) {
        return retVal; 
    }

    return ASE_OK; 
}



/*****************************************************************************
*
*****************************************************************************/
int SendIOCTL (reader* globalData, uchar socket, 
               uchar* inBuf, int inBufLen, uchar* outBuf, int *outBufLen) {
    char ack = 0x00;
    int retVal, actual, retryTimes = 2, sendLen = *outBufLen;
    int dataLen, i;
    unsigned char cs = 0;

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "\n SendIOCTL - Enter sendLen = %d\n", sendLen);
#endif

    if ((retVal = readerCommandInit(globalData, 1)))
        return retVal;

    // verify that inBuf has a valid reader command structure (including checksum)
    if (inBuf[0] != 0x50)
        return ASE_READER_PID_ERROR;
    
    // inBuf[1] is the command
    
    dataLen = inBuf[2];
    if (dataLen != (inBufLen - 4 /*PID+CMD+LEN+CKM*/))
        return ASE_READER_LEN_ERROR;

    for (i = 0 ; i < inBufLen  ; ++i)
        cs ^= inBuf[i];
    if (cs)
        return ASE_READER_CS_ERROR;

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "\n SendIOCTL - cmd struct ok\n");
#endif

    do {
        lock_mutex(globalData);

        if (sendLen == 2) {
#ifdef ASE_DEBUG
	    syslog(LOG_INFO, "\n SendIOCTL - ControlCommand\n");
#endif
            retVal = sendControlCommand(globalData, 0, (char*)inBuf, inBufLen, &ack, &actual, 1);
        }
        else {
#ifdef ASE_DEBUG
	    syslog(LOG_INFO, "\n SendIOCTL - CloseResponseCommand retVal = %0x\n", retVal);
#endif
            /* A Retry command should be sent */
            if (retVal == ASE_ERROR_DATA_INCOMPLETE || retVal == ASE_ERROR_CHECKSUM) {
                char cmd2[4];

                cmd2[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, socket);
                globalData->commandCounter++;
                globalData->commandCounter %= 4;
                cmd2[1] = 0x44;
                cmd2[2] = 0x0;
                cmd2[3] = cmd2[0] ^ cmd2[1] ^ cmd2[2];

                retVal = sendCloseResponseCommand(globalData, socket, cmd2, 4, (char*)outBuf, outBufLen, 0);
            }
            /* The command should be re-sent to the reader */
            else {
                retVal = sendCloseResponseCommand(globalData, socket, (char*)inBuf, inBufLen, (char*)outBuf, outBufLen, 0); 
            }
        }
        unlock_mutex(globalData);

        retryTimes--;
    } while (retVal != ASE_OK && retryTimes);

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "\n SendIOCTL - retVal = %0x ack = %0x *outBufLen = %d\n", retVal, ack, *outBufLen);
#endif

    // if during the 3 tries the command failed, return an error status
    if (retVal < 0) {
        // general error
        outBuf[0] = 0x6F;
        outBuf[1] = 0x00;
        *outBufLen = 2;
        return retVal; 
    }

    if (sendLen == 2 && ack != 0x20) {
        // general error
        outBuf[0] = 0x6F;
        outBuf[1] = 0x00;
        return parseStatus(ack); 
    }

    // success
    if (sendLen == 2) {
        outBuf[0] = 0x90;
        outBuf[1] = 0x00;
    }
    else {
        outBuf[(*outBufLen)++] = 0x90;
        outBuf[(*outBufLen)++] = 0x00;
    }

#ifdef ASE_DEBUG
    syslog(LOG_INFO, " SendIOCTL - Exit\n");
#endif

    return ASE_OK; 
}



