#ifndef __ASE_H
#define __ASE_H

#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <limits.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#include <linux/usbdevice_fs.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <stdio.h>
//#include <linux/spinlock.h>
#include <syslog.h>

#include "LinuxDefines.h"
#include "atr.h"
#include "T1Protocol.h"
#include "MemoryCards.h"


//#define ASE_DEBUG

/*****************************************************************************
* IOCTL
*****************************************************************************/
#define ASE_IOCTL_CLASS               0xF0
#define ASE_IOCTL_XI2C_INS            0xA0
#define ASE_IOCTL_CLOCK_INS           0xB0

// for sending raw reader ioctls
#define ASE_IOCTL_RAW_CLASS           0xFF
#define ASE_IOCTL_RAW_INS             0xA0


/*****************************************************************************
*
*****************************************************************************/
#define LEFT_NIBBLE(c) ((c & 0xF0) >> 4)
#define RIGHT_NIBBLE(c) (c & 0x0F)
#define PRINT_CHAR(c) LEFT_NIBBLE(c), RIGHT_NIBBLE(c)

#ifndef MIN
#define MIN(a, b) (a > b ? b : a)
#endif
#ifndef MAX
#define MAX(a, b) (a > b ? a : b)
#endif


#define ASE_READER_ETU_DELTA       150

/*****************************************************************************
*
*****************************************************************************/
#define BULK_BUFFER_SIZE        300 

#define EVENT_BUFFER_SIZE       16
#define RESPONSE_BUFFER_SIZE    4096 

#define MAX_SOCKET_NUM          4

#define ASE_DATA_MEMORY_SIZE    64 
#define READER_FIDI_TABLE_SIZE  100

#define BUFFER_SIZE             8192

/*****************************************************************************
*
*****************************************************************************/
struct card_params {
    uchar       protocol;
    uchar       N;
    uchar       CWT[3];
    uchar       BWT[3];
    uchar       A; 
    uchar       B; 
    uchar       freq;
    float       fTod;
};


typedef struct {
    int                 status; /* 0 - absent, 1 - inserted, 2 - powered on */
    int                 ceei;
    ATR                 atr;

    long                cwt; /* max time in microseconds to wait between characters */
    long                bwt; /* max time in microseconds to wait to first character (in T=0, equals to cwt) */

    T1Protocol          T1;

    struct card_params  cardParams;

    uchar               FiDi;

    char                activeProtocol;

    MemCard             memCard;

} card;



typedef struct {
    HANDLE          handle;
    unsigned char   baud;
    unsigned char   bits;
    int             stopbits;
    char            parity;
    long            blocktime;

} ioport;




typedef struct {
//    int                 fd;
    ioport              io;

    char                dataMemory[ASE_DATA_MEMORY_SIZE];
    
    int                 readerStarted; /* 1 - started, 0 - not yet */

    char                commandCounter;
    
    card                cards[MAX_SOCKET_NUM]; 

    pthread_mutex_t     semaphore;     

} reader;


/*****************************************************************************
*
*****************************************************************************/
/* return codes */
#define ASE_OK                      0
#define ASE_ERROR_DATA_INCOMPLETE     -1
#define ASE_ERROR_IOCTL_RESPONSE      -2
#define ASE_ERROR_IOCTL_EVENT         -3
#define ASE_ERROR_READ              -4
#define ASE_ERROR_WRITE             -5
#define ASE_ERROR_TIMEOUT           -6
#define ASE_ERROR_COMMAND           -7
#define ASE_ERROR_CHECKSUM          -8
#define ASE_ERROR_RESPONSE_INCORRECT  -9
#define ASE_ERROR_OVERHEATING        -10
#define ASE_ERROR_ATR                -11
#define ASE_ERROR_PARITY             -12
#define ASE_ERROR_FIFO_OVERRUN       -13
#define ASE_ERROR_FRAMING            -14
#define ASE_ERROR_EARLY_ANSWER       -15

#define ASE_ERROR_RESEND_COMMAND       -16


#define ASE_READER_PID_ERROR               -110
#define ASE_READER_CNT_ERROR               -111
#define ASE_READER_TRUNC_ERROR             -112
#define ASE_READER_LEN_ERROR               -113
#define ASE_READER_UNKNOWN_CMD_ERROR       -114
#define ASE_READER_TIMEOUT_ERROR           -115
#define ASE_READER_CS_ERROR                -116
#define ASE_READER_INVALID_PARAM_ERROR     -117
#define ASE_READER_CMD_FAILED_ERROR        -118
#define ASE_READER_NO_CARD_ERROR           -119
#define ASE_READER_CARD_NOT_POWERED_ERROR  -120
#define ASE_READER_COMM_ERROR              -121
#define ASE_READER_EXTRA_WAITING_TIME      -122

#define ASE_READER_NOT_CPU_CARD            -123
#define ASE_READER_NO_MATCHING_PARAMS      -124
#define ASE_READER_CARD_REJECTED           -125

#define ASE_READER_INVALID_STATUS_BYTE     -126
#define ASE_READER_RETRY_FAILED            -127


/*****************************************************************************
* externs
*****************************************************************************/
extern int ParseATR  (reader* globalData, char socket, char data[], int len);
extern int GetCurrentMode (ATR* atr);
extern int CapableOfChangingMode (ATR* atr);
extern uchar GetSpecificModeProtocol (ATR* atr);
extern uchar GetFi (ATR* atr);
extern uchar GetDi (ATR* atr);
extern uchar GetN (ATR* atr);
extern uchar GetWI (ATR* atr);
extern long GetMaxFreq (ATR* atr);
extern uchar GetFirstOfferedProtocol (ATR* atr);
extern float GetFToDFactor (int F, int D);
extern float GetDToFFactor (int F, int D);
extern uchar GetT1IFSC (ATR* atr);
extern uchar GetT1CWI (ATR* atr);
extern uchar GetT1BWI (ATR* atr);
extern uchar GetT1EDC (ATR* atr);
extern int   IsT1Available (ATR* atr);
extern uchar GetClassIndicator (ATR* atr);


extern int parseStatus (uchar ackByte);
extern int isEvent (uchar event);
extern int parseEvent  (reader* globalData, char socket, uchar event);

extern int readResponse (reader* globalData, char socket, int num, 
                         char* outBuf, int* outBufLen, unsigned long timeout/*, int startOfResponse*/);

extern int writeToReader (reader* globalData, char* data, int len, int* actual);

extern int checkValidity (int retVal, int len, int actual, const char* message);

extern int sendCloseResponseCommand (reader* globalData, char socket, char* cmd, int len, char* outBuf, int* outBufLen, char ignoreEvents);
extern int sendControlCommand (reader* globalData, char socket, char* cmd, int len, char* outBuf, int* outBufLen, char ignoreEvents);


extern int cardCommandInit (reader* globalData, char socket, char needToBePoweredOn);
extern int readerCommandInit (reader* globalData, char needToBeStarted);

extern int ReaderStartup (reader* globalData, char* response, int* len);
extern int ReaderFinish (reader* globalData);
extern int GetStatus (reader* globalData, char* response, int* len);
extern int SetCardParameters (reader* globalData, char socket, struct card_params params);
extern int CardPowerOn (reader* globalData, char socket, uchar cardType, uchar voltage);
extern int CPUCardReset (reader* globalData, char socket);
extern int CardPowerOff (reader* globalData, char socket);
extern int ChangeLedState (reader* globalData, char on);
extern int PPSTransact (reader* globalData, char socket, char* buffer, int len, char* outBuf, int* outBufLen);
extern int CardCommand (reader* globalData, char socket, char command, 
			uchar* buffer, int len, uchar* outBuf, int* outBufLen);

/*
extern int T0CPUCardTransact (reader* globalData, char socket, char* buffer, int len, char* outBuf, int* outBufLen,
                              openResponseProccessor orp, unsigned long arg);
*/

extern int T1CPUCardTransact (reader* globalData, char socket, char* buffer, int len, char* outBuf, int* outBufLen);

extern int MemoryCardTransact (reader* globalData, char socket, 
                               char cmdType, uchar command, unsigned short address, uchar len,
                               uchar* data, char* outBuf, int* outBufLen);


/* the protocol is not NULL if a PPS is required to switch to it.*/
extern int InitCard (reader* globalData, char socket, char coldReset, char* protocol);

/* outBuf will contain the SW1/SW2 as well */
extern int T0Read  (reader* globalData, char socket, 
                    uchar* inBuf, int inBufLen, uchar* outBuffer, int *outBufLen);
extern int T0Write  (reader* globalData, char socket, 
                     uchar* inBuf, int inBufLen, uchar* outBuffer, int *outBufLen);


extern int T1InitProtocol (reader* globalData, char socket, char setIFSD);
extern int T1Command (reader* globalData, uchar socket, 
                      uchar* inBuf, int inBufLen, uchar* outBuffer, int *outBufLen);

extern int MemoryCardCommand (reader* globalData, uchar socket, 
                              uchar* inBuf, int inBufLen, uchar* outBuffer, int *outBufLen);

extern int SendIOCTL (reader* globalData, uchar socket, 
                      uchar* inBuf, int inBufLen, uchar* outBuffer, int *outBufLen);

bool IO_InitializePort(	/* Initialize the card reader port.	*/
    reader* globalData, 
	int baud,	/* Baud rate to set port to		*/
	int bits,	/* Bytesize: 5, 6, 7 or 8		*/
	char par,	/* Parity: 'E' even, 'O' odd, 'N' none	*/
	char* port	/* Name of port, or (char *)0L for dialog */
);

int IO_Read(		/* Read up to 256 bytes from the port	*/
    reader* globalData, 
    unsigned long timeout,
	int readsize,   /* Number of bytes to read		*/
	unsigned char *response  /* Bytes read                           */
);	

int IO_Write(
    reader* globalData, 
    int writesize,
	unsigned char* c          /* Bytes to be written                   */
);

bool			/* True for success, false otherwise */
IO_Close(		/* On a Mac, gotta close the port */
	reader* globalData
);


HANDLE IO_ReturnHandle(        /* Returns the current handle           */
	reader* globalData
);

int IO_UpdateReturnBlock(   /* Returns the current blocking time    */
    reader* globalData, 
	int blocktime   /* The updated blocking time            */
);

int IO_ReturnBaudRate(      /* Return the current baudrate          */
	reader* globalData
);


char IO_ReturnParity( 
	reader* globalData            /*  Return the current parity  */
);

char IO_UpdateParity(
    reader* globalData, 
	char parity	
); 

int IO_ReturnStopBits(
    reader* globalData            /*  Return stop bits  */
);

int IO_UpdateStopBits(
    reader* globalData, 
    int stopbits
);


bool 		/* True for sucess, false otherwise	*/
IO_FlushBuffer(		/* Flush buffer to card reader		*/
	reader* globalData
);

void 		
IO_CleanReadBuffer(		/* Removes all data arrived but not read yet		*/
	reader* globalData
);


#endif 

