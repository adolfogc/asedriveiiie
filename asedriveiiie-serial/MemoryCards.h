#ifndef __MEMEORY_CARDS_H
#define __MEMEORY_CARDS_H


/*****************************************************************************
*
*****************************************************************************/
#define MAIN_MEM        0
#define PROTECTED_MEM   1


#define PROTOCOL_MEM_2BUS   0
#define PROTOCOL_MEM_3BUS   1
#define PROTOCOL_MEM_I2C    2
#define PROTOCOL_MEM_XI2C   3


#define MEM_CARD_MAIN_MEM_MODE  1
#define MEM_CARD_PROT_MEM_MODE  2


typedef struct {
    char    memType;
    char    protocolType;
} MemCard;


/*****************************************************************************
*
*****************************************************************************/
#define MEM_CARD_OK                         0
#define MEM_CARD_ERROR_CMD_STRUCT       -3000
#define MEM_CARD_ERROR_APDU             -3001

#define MEM_CARD_COMM_ERROR             -3002
#define MEM_CARD_BLOCKED                -3003
#define MEM_CARD_PROTOCOL_NOT_SUPPORTED -3004
#define MEM_CARD_INVALID_PARAMETER      -3005
#define MEM_CARD_ILLEGAL_ADDRESS        -3006
#define MEM_CARD_BAD_MODE               -3007
#define MEM_CARD_VERIFY_FAILED          -3008
#define MEM_CARD_NOT_VERIFIED           -3009
#define MEM_CARD_WRONG_LENGTH           -3010
#define MEM_CARD_WRONG_CLASS            -3011
#define MEM_CARD_WRONG_INST             -3012
#define MEM_CARD_FILE_NOT_FOUND         -3013
#define MEM_CARD_WRITE_FAILED           -3014


/*****************************************************************************
*
*****************************************************************************/
#define APDU_INS_SELECT             0xA4
#define APDU_INS_VERIFY             0x20
#define APDU_INS_CHANGE_PASSWORD    0x24
#define APDU_INS_READ_BINARY        0xB0
#define APDU_INS_WRITE_BINARY       0xD6


/*****************************************************************************
*
*****************************************************************************/
/* 2 Card commands definitions */
#define _2BUS_READ_DATA             0x30
#define _2BUS_UPDATE_DATA           0x38
#define _2BUS_READ_PBIT             0x34
#define _2BUS_PROTECT_DATA_COND     0x3C
#define _2BUS_READ_SECURITY_MEM     0x31
#define _2BUS_UPDATE_SECURITY_MEM   0x39
#define _2BUS_COMPARE_VERIFICATION  0x33

/* 3 Card commands definitions */
#define _3BUS_UPDATE_AND_PROTECT    0x31
#define _3BUS_UPDATE_DATA           0x33
#define _3BUS_WRITE_PBIT_COND       0x30
#define _3BUS_READ_DATA             0x0E
#define _3BUS_READ_DATA_AND_PBIT    0x0C
#define _3BUS_WRITE_ERROR_COUNTER   0x32
#define _3BUS_VERIFY_PSC            0x0D

/* 2 BUS Addresses */
#define _2BUS_MAX_ADDRESS           255
#define _2BUS_MAX_PROTECTED_ADDRESS 31
#define _2BUS_ERROR_COUNTER_ADDRESS 0
#define _2BUS_PSC_CODE_1_ADDRESS    1
#define _2BUS_PSC_CODE_2_ADDRESS    2
#define _2BUS_PSC_CODE_3_ADDRESS    3
#define _2BUS_PSC_SIZE              3

/* 3 BUS Addresses */
#define _3BUS_MAX_ADDRESS           1020    /* 1KByte card - 3 bytes */
#define _3BUS_ERROR_COUNTER_ADDRESS 1021
#define _3BUS_PSC_CODE_1_ADDRESS    1022
#define _3BUS_PSC_CODE_2_ADDRESS    1023
#define _3BUS_PSC_SIZE              2

#define _23BUS_ATR_SIZE             4
#define _23BUS_COMMAND_SIZE         3




#endif 
