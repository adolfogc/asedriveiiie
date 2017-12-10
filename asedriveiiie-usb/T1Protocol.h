#ifndef __T1PROTOCOL_H
#define __T1PROTOCOL_H

typedef unsigned char uchar;


/*****************************************************************************
*
*****************************************************************************/
#define T1_BLOCK_MAX_SIZE                259
#define T1_BLOCK_INF_MAX_SIZE            254

#define ATHENA_IFSC_MAX_SIZE             254 


/* PCBs */
#define T1_BLOCK_I                0x00
#define T1_BLOCK_R                0x80
#define T1_BLOCK_S                0xC0

#define T1_BLOCK_S_RESYNCH_REQ    0xC0
#define T1_BLOCK_S_RESYNCH_RES    0xE0
#define T1_BLOCK_S_IFS_REQ        0xC1
#define T1_BLOCK_S_IFS_RES        0xE1
#define T1_BLOCK_S_ABORT_REQ      0xC2
#define T1_BLOCK_S_ABORT_RES      0xE2
#define T1_BLOCK_S_WTX_REQ        0xC3
#define T1_BLOCK_S_WTX_RES        0xE3
#define T1_BLOCK_S_VPP_ERR        0xE4


/*****************************************************************************
*
*****************************************************************************/
#define PROTOCOL_T1_OK                   0          /* Command OK  */
#define PROTOCOL_T1_ICC_ERROR           -2000       /* ICC comunication error */
#define PROTOCOL_T1_ERROR               -2001       /* T=1 Protocol Error */
#define T1_ABORT_RECEIVED               -2002
#define T1_RESYNCH_RECEIVED             -2003
#define T1_VPP_ERROR_RECEIVED           -2004


#define PROTOCOL_T1_SEND_I_BLOCK        1
#define PROTOCOL_T1_SEND_R_BLOCK        2
#define PROTOCOL_T1_SEND_S_BLOCK        3



typedef struct {
    uchar data[T1_BLOCK_MAX_SIZE];
    int   len;
} T1Block;


typedef struct {
    int     ifsc;             /* Information field size for the ICC */
    int     edc;              /* Type of error detection code */

    uchar   ns;               /* Send sequence number */
    uchar   nsCard;           /* Send sequence number of the card */

    T1Block sendBlock, recBlock;

} T1Protocol;




#endif
