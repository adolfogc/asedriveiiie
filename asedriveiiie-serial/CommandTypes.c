#include "Ase.h"


/* ASE reader generic command types */


#define ASE_LONG_RESPONSE_PID               0x90
#define ASE_RESPONSE_PID                    0x10
#define ASE_ACK_PID                         0x20
#define ASE_LONG_RESPONSE_WITH_STATUS_PID   0xF0
#define ASE_RESPONSE_WITH_STATUS_PID        0x70


/*****************************************************************************
* 
*****************************************************************************/
//#define ASE_PACKET_TYPE(pid, cnt, dst) (pid | cnt << 2 | dst)
#define ASE_PACKET_TYPE(pid, cnt, dst) (pid | dst)


/*****************************************************************************
*
*****************************************************************************/
/* returns 0 on success, negative number otherwise */
int checkValidity (int retVal, int len, int actual, const char* message) {
    if (retVal < 0 || actual != len) {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "%s   Error = %d\n", message, retVal);
#endif
        return retVal; 
    }
    return ASE_OK;
}


/*****************************************************************************
*
*****************************************************************************/
static void cleanResponseBuffer (reader* globalData) {
    IO_CleanReadBuffer(globalData);
}

/*****************************************************************************
*
*****************************************************************************/
/* returns 0 on success, negative number otherwise
 outBuf should be large enough to contain the close response */
int sendCloseResponseCommand (reader* globalData, char socket, char* cmd, int len, 
                              char* outBuf, int* outBufLen, char ignoreEvents) {
    int retVal, actual, i, readLen, retryTimes = 5, withStatus = 0;
    uchar oneByte, cs, readcs;
    long cwt = (globalData->cards[(int)socket].cwt > 0 ? globalData->cards[(int)socket].cwt : 1000);

    /* send the command */

    retVal = writeToReader(globalData, cmd, len, &actual);
    if (checkValidity(retVal, len, actual, "sendCloseResponseCommand - Error! in command write.\n")) {
        cleanResponseBuffer(globalData);
        return retVal; 
    }

    /* read the close response */

    /* set the timeout for the packet header */
    cwt = MAX(cwt * 260, globalData->cards[(int)socket].bwt);
    cwt += 200000; // delta of 0.2 seconds

    /* read packet header and verify it's ok */
    retVal = readResponse(globalData, socket, 1, (char*)(&oneByte), &actual, cwt);
    if (checkValidity(retVal, 1, actual, "sendCloseResponseCommand - Error! in packet header read.\n")) {
        cleanResponseBuffer(globalData);
        return retVal; 
    }

    /* loop until we get the start of a response or an error has occured */
    while (oneByte != ASE_LONG_RESPONSE_PID && oneByte != ASE_RESPONSE_PID && 
           oneByte != ASE_RESPONSE_WITH_STATUS_PID && oneByte != ASE_LONG_RESPONSE_WITH_STATUS_PID && 
           retryTimes) {
        /* check if it's an acknowledgment */
        if (oneByte & ASE_ACK_PID) {
            if (parseStatus(oneByte) != ASE_READER_EXTRA_WAITING_TIME) {
#ifdef ASE_DEBUG
                syslog(LOG_INFO, "sendCloseResponseCommand - Status 0x%x%x.\n", (oneByte & 0xF0) >> 4, oneByte & 0x0F);
#endif
                cleanResponseBuffer(globalData);
                return parseStatus(oneByte); 
            }
	    else
    	    	retryTimes = 5;
        }
        /* check if it's an event */
        else if (isEvent(oneByte)) { 
            /* this is an event and it's concerning the current card */
            parseEvent(globalData, socket, oneByte);
#ifdef ASE_DEBUG
	    syslog(LOG_INFO, "sendCloseResponseCommand - Event 0x%x%x.\n", (oneByte & 0xF0) >> 4, oneByte & 0x0F);
#endif
	    retryTimes = 5;
        }
        /* this must be an error -> send a retry command */
        else {
            char cmdTmp[4];

            cmdTmp[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, socket);
            globalData->commandCounter++;
            globalData->commandCounter %= 4;
            cmdTmp[1] = 0x44;
            cmdTmp[2] = 0x0;
            cmdTmp[3] = cmdTmp[0] ^ cmdTmp[1] ^ cmdTmp[2];

            retVal = writeToReader(globalData, cmdTmp, 4, &actual);
            if (checkValidity(retVal, 4, actual, "sendControlCommand - Error! in command write.\n")) {
                cleanResponseBuffer(globalData);
                return retVal; 
            }

	    //            retryTimes = 5;
        }

        /* read packet header and verify it's ok */
        retVal = readResponse(globalData, socket, 1, (char*)(&oneByte), &actual, cwt);
        if (checkValidity(retVal, 1, actual, "sendCloseResponseCommand - Error! in packet header read.\n")) {
            cleanResponseBuffer(globalData);
            return retVal; 
        }

        retryTimes--;
    }

    if (retryTimes == 0) {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "sendCloseResponseCommand - Error! retryTimes is 0.\n");
#endif
        return ASE_ERROR_RESEND_COMMAND;
    }

    cs = oneByte;

    /* set the timeout for the length  */
    cwt = 100000; // 0.1 sec
    
    /* check if a status byte appears at the end of the response */
    if (oneByte == ASE_LONG_RESPONSE_WITH_STATUS_PID || oneByte == ASE_RESPONSE_WITH_STATUS_PID)
        withStatus = 1;

    /* this is a response header, so read its length */
    if (oneByte == ASE_LONG_RESPONSE_PID || oneByte == ASE_LONG_RESPONSE_WITH_STATUS_PID) { 
        /* long response */
        retVal = readResponse(globalData, socket, 1, (char*)(&oneByte), &actual, cwt);
        if (checkValidity(retVal, 1, actual, "sendCloseResponseCommand - Error! in packet header read.\n")) {
            cleanResponseBuffer(globalData);
            return retVal; 
        }

        cs ^= oneByte;

        readLen = (oneByte << 8);
        readLen &= 0xFF00;

        retVal = readResponse(globalData, socket, 1, (char*)(&oneByte), &actual, cwt);
        if (checkValidity(retVal, 1, actual, "sendCloseResponseCommand - Error! in packet header read.\n")) {
            cleanResponseBuffer(globalData);
            return retVal; 
        }

        cs ^= oneByte;

        readLen |= (oneByte & 0xFF);
    }
    else { 
        /* short response */
        retVal = readResponse(globalData, socket, 1, (char*)(&oneByte), &actual, cwt);
        if (checkValidity(retVal, 1, actual, "sendCloseResponseCommand - Error! in packet header read.\n")) {
            cleanResponseBuffer(globalData);
            return retVal; 
        }

        cs ^= oneByte;

        readLen = oneByte;
    }

    /* set the timeout for the data  */
    cwt = 100000 * (readLen + 1); // 0.1 sec per byte
    
/***!!!!!!!!!!!!!!!!!!!!***************************************/
    /* read data + checksum */
    retVal = readResponse(globalData, socket, readLen + 1, outBuf, outBufLen, cwt);
    if (checkValidity(retVal, readLen + 1, *outBufLen, "sendCloseResponseCommand - Error! in data read.\n")) {
        cleanResponseBuffer(globalData);
        return retVal; 
    }

    readcs = outBuf[*outBufLen - 1];
    (*outBufLen)--;

    for (i = 0 ; i < *outBufLen ; ++i)
        cs ^= outBuf[i];

    /* check the status byte provided */
    if (withStatus) {
        /* remove the last byte of outBuf and check the status */
        (*outBufLen)--;

        if (outBuf[*outBufLen] != ASE_ACK_PID) {
            /* read the checksum */
            /*retVal = readResponse(globalData, socket, 1, &oneByte, &actual, cwt);*/

            /* return error according to the status byte */
            cleanResponseBuffer(globalData);
            return parseStatus(outBuf[*outBufLen]);
        }
    }

    /* read the checksum */
    /*
    retVal = readResponse(globalData, socket, 1, &oneByte, &actual);
    if (checkValidity(retVal, 1, actual, "sendCloseResponseCommand - Error! in checksum read.\n")) {
        cleanResponseBuffer(globalData);
        return retVal; 
    }
    */

    if (cs != readcs) {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "sendCloseResponseCommand - Error! invalid checksum.\n");
#endif
        cleanResponseBuffer(globalData);
        return ASE_ERROR_CHECKSUM; 
    }

    return ASE_OK; 
}


/*****************************************************************************
*
*****************************************************************************/
/* returns 0 on success, negative number otherwise
   outBuf will contain the ACK/NAK byte sent as response */
int sendControlCommand (reader* globalData, char socket, char* cmd, int len, 
                        char* outBuf, int* outBufLen, char ignoreEvents) {
    int retVal, actual, retryTimes = 5;
    unsigned long cwt = (globalData->cards[(int)socket].cwt > 0 ? globalData->cards[(int)socket].cwt : 1000);

#ifdef ASE_DEBUG
    uchar oneByte;
#endif

    /* send the command */

    retVal = writeToReader(globalData, cmd, len, &actual);
    if (checkValidity(retVal, len, actual, "sendControlCommand - Error! in command write.\n")) {
        cleanResponseBuffer(globalData);
        return retVal; 
    }

    /* read the Ack control byte */

    /* set the timeout for the packet header */
    //    cwt = MAX(cwt * 260, globalData->cards[(int)socket].bwt);
    cwt = 3000000; // 3 sec

    retVal = readResponse(globalData, socket, 1, outBuf, outBufLen, cwt);
    if (checkValidity(retVal, 1, *outBufLen, "sendControlCommand - Error! in ack read.\n")) {
        cleanResponseBuffer(globalData);
        return retVal; 
    }

    /* loop until we get an OK acknowledgement */
    while (outBuf[0] != ASE_ACK_PID && retryTimes) {
        /* check if it's an acknowledgment */
        if (outBuf[0] & ASE_ACK_PID) {
            if (parseStatus(outBuf[0]) != ASE_READER_EXTRA_WAITING_TIME) {
#ifdef ASE_DEBUG
                syslog(LOG_INFO, "sendControlCommand - Error! Status 0x%x%x.\n", (oneByte & 0xF0) >> 4, oneByte & 0x0F);
#endif
                cleanResponseBuffer(globalData);
                return parseStatus(outBuf[0]); 
            }
    	    else
		retryTimes = 5;
        }
        /* check if it's an event */
        else if (isEvent((uchar)(outBuf[0]))) { 
            parseEvent(globalData, socket, outBuf[0]);
	    retryTimes = 5;
        }
        /* this must be an error -> send a retry command */
        else {
            char cmdTmp[4];

            cmdTmp[0] = ASE_PACKET_TYPE(0x50, globalData->commandCounter, socket);
            globalData->commandCounter++;
            globalData->commandCounter %= 4;
            cmdTmp[1] = 0x44;
            cmdTmp[2] = 0x0;
            cmdTmp[3] = cmdTmp[0] ^ cmdTmp[1] ^ cmdTmp[2];

            retVal = writeToReader(globalData, cmdTmp, 4, &actual);
            if (checkValidity(retVal, 4, actual, "sendControlCommand - Error! in command write.\n")) {
                cleanResponseBuffer(globalData);
                return retVal; 
            }

	    //            retryTimes = 5;
        }

        /* read packet header and verify it's ok */
        retVal = readResponse(globalData, socket, 1, outBuf, outBufLen, cwt);
        if (checkValidity(retVal, 1, *outBufLen, "sendControlCommand - Error! in ack read.\n")) {
            cleanResponseBuffer(globalData);
            return retVal; 
        }

        retryTimes--;
    }

    return ASE_OK; 
}


