#ifndef __Linux_Defines_h_
#define __Linux_Defines_h_


#define BOOL unsigned short
#define bool unsigned short
#define HANDLE unsigned long

/*
#ifndef BOOL
typedef unsigned short  BOOL;
#endif
typedef BOOL            bool;
typedef unsigned short  unsigned short;
typedef unsigned char	UCHAR;
typedef UCHAR           BYTE;
typedef unsigned char*	PUCHAR;
typedef char*           STR;
typedef unsigned long   ULONG;
typedef short           BOOLEAN;
typedef ULONG  		RESPONSECODE;
typedef ULONG*          PULONG;          

typedef ULONG  		DWORD;
typedef void*  		PVOID;
typedef ULONG           HANDLE;
*/


#ifndef TRUE
  #define TRUE  1
#endif 

#ifndef FALSE
  #define FALSE 0
#endif

#ifndef NULL
  #define NULL			   0x0000
#endif

/********************************************/

#ifndef MAX_BUFFER_SIZE
#define MAX_BUFFER_SIZE            264
#endif
#define MAX_ATR_SIZE               33

#define STATUS_SUCCESS                 0x0000
#define STATUS_UNSUCCESSFUL            0x0001
#define STATUS_IO_TIMEOUT              0x0002
#define STATUS_DATA_ERROR              0x0004
#define STATUS_UNRECOGNIZED_MEDIA      0x0008
#define STATUS_BUFFER_TOO_SMALL        0x0010
#define STATUS_DEVICE_PROTOCOL_ERROR   0x0020


/*****************************************************************************
*
*****************************************************************************/
/* Packet IDs */
#define READER_ACK                       0x20
#define READER_CLOSE_RESPONSE            0x10
#define READER_LONG_CLOSE_RESPONSE       0x90
#define HOST_COMMAND                     0x50
#define HOST_LONG_COMMAND                0xD0



#endif
