#include "Ase.h"



/*****************************************************************************
*
*****************************************************************************/
/* reader status */
#define STS_PID_OK                  0x00
#define STS_PID_ERROR               0x01
#define STS_CNT_ERROR               0x02
#define STS_TRUNC_ERROR             0x03
#define STS_LEN_ERROR               0x04
#define STS_UNKNOWN_CMD_ERROR       0x05
#define STS_TIMEOUT_ERROR           0x06
#define STS_CS_ERROR                0x07
#define STS_INVALID_PARAM_ERROR     0x08
#define STS_CMD_FAILED_ERROR        0x09
#define STS_NO_CARD_ERROR           0x0A
#define STS_CARD_NOT_POWERED_ERROR  0x0B
#define STS_COMM_ERROR              0x0C
#define STS_EXTRA_WAITING_TIME      0x0D
#define STS_RETRY_FAILED            0x0E


/* Card events */
#define EID_CARD1_NOT_PRESENT       0x01 
#define EID_CARD1_INSERTED          0x02 
#define EID_CARD2_NOT_PRESENT       0x05 
#define EID_CARD2_INSERTED          0x06 
#define EID_CARD3_NOT_PRESENT       0x09 
#define EID_CARD3_INSERTED          0x0A 
#define EID_CARD4_NOT_PRESENT       0x0D 
#define EID_CARD4_INSERTED          0x0E 



/*****************************************************************************
*
*****************************************************************************/
/* returns 0 if no error, status code otherwise */
int parseStatus (uchar ackByte) {
    int retVal;

    if ((ackByte & 0xF0) != 0x20)
        retVal = ASE_READER_INVALID_STATUS_BYTE;
    else {
        ackByte &= 0x0F;

        switch (ackByte) {
            case STS_PID_ERROR:
                retVal = ASE_READER_PID_ERROR;
                break;
            case STS_CNT_ERROR:
                retVal = ASE_READER_CNT_ERROR;
                break;
            case STS_TRUNC_ERROR:
                retVal = ASE_READER_TRUNC_ERROR;
                break;
            case STS_LEN_ERROR:
                retVal = ASE_READER_LEN_ERROR;
                break;
            case STS_UNKNOWN_CMD_ERROR:
                retVal = ASE_READER_UNKNOWN_CMD_ERROR;
                break;
            case STS_TIMEOUT_ERROR:
                retVal = ASE_READER_TIMEOUT_ERROR;
                break;
            case STS_CS_ERROR:
                retVal = ASE_READER_CS_ERROR;
                break;
            case STS_INVALID_PARAM_ERROR:
                retVal = ASE_READER_INVALID_PARAM_ERROR;
                break;
            case STS_CMD_FAILED_ERROR:
                retVal = ASE_READER_CMD_FAILED_ERROR;
                break;
            case STS_NO_CARD_ERROR:
                retVal = ASE_READER_NO_CARD_ERROR;
                break;
            case STS_CARD_NOT_POWERED_ERROR:
                retVal = ASE_READER_CARD_NOT_POWERED_ERROR;
                break;
            case STS_COMM_ERROR:
                retVal = ASE_READER_COMM_ERROR;
                break;
            case STS_EXTRA_WAITING_TIME:
                retVal = ASE_READER_EXTRA_WAITING_TIME;
                break;
            case STS_RETRY_FAILED:
                retVal = ASE_READER_RETRY_FAILED;
                break;
            default:
                retVal = 0;
                break;
        }
    }

#ifdef ASE_DEBUG
    if (retVal) 
        syslog(LOG_INFO, "parseStatus: Error! status = %d\n", ackByte);
#endif

    return retVal;
}

/*****************************************************************************
*
*****************************************************************************/
int isEvent (uchar event) {
    if (event == EID_CARD1_NOT_PRESENT || event == EID_CARD2_NOT_PRESENT ||
        event == EID_CARD1_INSERTED || event == EID_CARD2_INSERTED ||
        event == EID_CARD3_NOT_PRESENT || event == EID_CARD4_NOT_PRESENT ||
        event == EID_CARD3_INSERTED || event == EID_CARD4_INSERTED)
        return 1;

    return 0;
}


/*****************************************************************************
*
*****************************************************************************/
/* returns 0 if not an error, error code otherwise.
 change global data status if neccesary */
int parseEvent (reader* globalData, char socket, uchar event) {
    uchar   skn;

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "=============> Event = 0x%x%x <===============\n", PRINT_CHAR(event));
#endif

	skn = (event & 0x0C) >> 2; 
    globalData->cards[skn].status = ((event & 0x01) ? 0 : 1);
    /* if the card is out and the socket number is the one we use, return an error indication */
	if ((event & 0x01) && (skn == socket))
	    return ASE_READER_NO_CARD_ERROR;

    return ASE_OK;
}


/*****************************************************************************
*
*****************************************************************************/
int writeToReader (reader* globalData, char* data, int len, int* actual) {
    *actual = IO_Write(globalData, len, (unsigned char*)data);

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "writeToReader: wrote %d bytes. data = ", *actual);
    int i;
    for (i = 0 ; i < *actual ; ++i)
    	syslog(LOG_INFO, "0x%x%x ", PRINT_CHAR(data[i]));
    syslog(LOG_INFO, "\n");
#endif

    if (*actual != len) {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "writeToReader - Error! len = %d, actual = %d\n", len, *actual);
#endif
        return ASE_ERROR_WRITE;
    }

    return ASE_OK;
}




/*****************************************************************************
*
*****************************************************************************/
/* returns 0 on success; negative value otherwise */
int readResponse (reader* globalData, char socket, int num, 
                  char* outBuf, int* outBufLen, unsigned long timeout/*, int startOfResponse*/) {

    int len;
    int remain = num;

    *outBufLen = 0;

    len = IO_Read(globalData, timeout, remain, (unsigned char*)(&(outBuf[*outBufLen])));
    (*outBufLen) = (*outBufLen) + len;

    if (num == *outBufLen)  /* all data arrived */
        return ASE_OK;
    else  { /* part of the data is missing */
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "readResponse - Error! read only %d out of %d bytes\n", *outBufLen, num);
#endif
        return ASE_ERROR_DATA_INCOMPLETE;
    }

}












