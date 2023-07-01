#include "Ase.h"


/*****************************************************************************
*
*****************************************************************************/
#define MEM_CARD_READ_CHUNK_SIZE     29
#define MEM_CARD_WRITE_CHUNK_SIZE    23


/*****************************************************************************
*
*****************************************************************************/
static void AddSW1SW2 (int status, uchar* outBuf, int* outBufLen, uchar moreInfo) {
    switch (status) {
        case ASE_OK:
            outBuf[*outBufLen] = 0x90;
            (*outBufLen)++;
            outBuf[*outBufLen] = 0x00;
            (*outBufLen)++;
            break;
        case MEM_CARD_COMM_ERROR: /* command failed */
        case MEM_CARD_INVALID_PARAMETER: /* wrong file in select */
            outBuf[*outBufLen] = 0x69;
            (*outBufLen)++;
            outBuf[*outBufLen] = 0x00;
            (*outBufLen)++;
            break;
        case MEM_CARD_ILLEGAL_ADDRESS:
            outBuf[*outBufLen] = 0x6B;
            (*outBufLen)++;
            outBuf[*outBufLen] = 0x00;
            (*outBufLen)++;
            break;
        case MEM_CARD_BAD_MODE:
            outBuf[*outBufLen] = 0x68;
            (*outBufLen)++;
            outBuf[*outBufLen] = 0x00;
            (*outBufLen)++;
            break;
        case MEM_CARD_BLOCKED:
            outBuf[*outBufLen] = 0x69;
            (*outBufLen)++;
            outBuf[*outBufLen] = 0x83;
            (*outBufLen)++;
            break;
        case MEM_CARD_WRONG_LENGTH:
            outBuf[*outBufLen] = 0x67;
            (*outBufLen)++;
            outBuf[*outBufLen] = 0x00;
            (*outBufLen)++;
            break;
        case MEM_CARD_WRONG_CLASS:
            outBuf[*outBufLen] = 0x6E;
            (*outBufLen)++;
            outBuf[*outBufLen] = 0x00;
            (*outBufLen)++;
            break;
        case MEM_CARD_WRONG_INST:
            outBuf[*outBufLen] = 0x6D;
            (*outBufLen)++;
            outBuf[*outBufLen] = 0x00;
            (*outBufLen)++;
            break;
        case MEM_CARD_FILE_NOT_FOUND:
            outBuf[*outBufLen] = 0x6A;
            (*outBufLen)++;
            outBuf[*outBufLen] = 0x82;
            (*outBufLen)++;
            break;
        case MEM_CARD_WRITE_FAILED:
            outBuf[*outBufLen] = 0x62;
            (*outBufLen)++;
            outBuf[*outBufLen] = 0x00;
            (*outBufLen)++;
            break;
        case MEM_CARD_VERIFY_FAILED:
            outBuf[*outBufLen] = 0x63;
            (*outBufLen)++;
            outBuf[*outBufLen] = (0xC0 | moreInfo);
            (*outBufLen)++;
            break;
        default:
            outBuf[*outBufLen] = 0x6F;
            (*outBufLen)++;
            outBuf[*outBufLen] = 0x00;
            (*outBufLen)++;
            break;
    }
}



/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// 2BUS cards functions
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////



/*****************************************************************************
*
*****************************************************************************/
static int _2BUSExecInCommand (reader* globalData, uchar socket, 
                               uchar cmdCode, unsigned short address, uchar len,
                               uchar* outBuffer, int *outBufLen) {
    int retVal, actual = 0;

    retVal = MemoryCardTransact(globalData, socket, 1, cmdCode, address, 
                                len, NULL, (char*)outBuffer, &actual);
    *outBufLen += actual;
    if (retVal < 0 || actual != len) {
        return MEM_CARD_COMM_ERROR;
    }

    return ASE_OK;
}

/*****************************************************************************
*
*****************************************************************************/
static int _2BUSExecOutCommand (reader* globalData, uchar socket, 
                                uchar cmdCode, unsigned short address, 
                                uchar* data, uchar len,
                                uchar* outBuffer, int *outBufLen) {
    int retVal;                      

    retVal = MemoryCardTransact(globalData, socket, 0, cmdCode, address, 
                                len, data, (char*)outBuffer, outBufLen);
    if (retVal < 0) {
        return MEM_CARD_COMM_ERROR;
    }

    return ASE_OK;
}    

/*****************************************************************************
*
*****************************************************************************/
static void ArrangeBitsIntoBytes(uchar* bitsBuff, uchar* buff,
                                 unsigned short address, int len) {
    int bytesLen = (address + len + 7) / 8;
    unsigned char buffIndex = 0;
    unsigned char  bitsCount = 0;
    unsigned char i, j;

    for (i = 0; i < bytesLen; i++) {
        for (j = 0; j < 8; j++) {
            if (bitsCount++ < address)
                continue;
            buff[buffIndex++] = ((bitsBuff[i] >> j) & 0x1);
            if (buffIndex >= len)
                return;
        }
    }
}

/*****************************************************************************
*
*****************************************************************************/
static int _2BUSReadData(reader* globalData, uchar socket, 
                         unsigned short address, int len, uchar* outBuffer, int *outBufLen) {
    int retVal = ASE_OK, counter = 0, actual;
    unsigned short maxAddress = 0;
    uchar cmdCode = 0, mode = globalData->cards[(int)socket].memCard.memType;
    uchar thisRead;
    unsigned short readLen = len;
    uchar bitsBuff[4]; /* for 2 bus cards, 32 protected  bits */
    uchar* readBuff = outBuffer;

    switch (mode) {
        case MEM_CARD_MAIN_MEM_MODE:
            if (address > _2BUS_MAX_ADDRESS) {
                retVal = MEM_CARD_ILLEGAL_ADDRESS;
            }
            else {
                maxAddress = _2BUS_MAX_ADDRESS;
                cmdCode = _2BUS_READ_DATA;
            }
            break;
        case MEM_CARD_PROT_MEM_MODE:
            if (address > _2BUS_MAX_PROTECTED_ADDRESS)
                retVal = MEM_CARD_ILLEGAL_ADDRESS;
            else {
                maxAddress = _2BUS_MAX_PROTECTED_ADDRESS;
                cmdCode = _2BUS_READ_PBIT;
                /* address is DON'T CARE in this case  */
                readLen = 4;    /* 4 bytes is 32 protections bits  */
                readBuff = bitsBuff;
            } 
            break;
        default:
            retVal = MEM_CARD_BAD_MODE;
            break;
    }

    if (retVal == ASE_OK) {
        /* check if we are not trying to read too much */
        if (/*mode != MEM_CARD_PROT_MEM_MODE &&*/ len > (maxAddress - address + 1))
            retVal = MEM_CARD_WRONG_LENGTH;
        else {
            if (mode == MEM_CARD_PROT_MEM_MODE)
                retVal = _2BUSExecInCommand(globalData, socket, cmdCode, address, readLen, readBuff, &actual);
            else {
                while (counter < len && retVal == ASE_OK) {
                    thisRead = (readLen > MEM_CARD_READ_CHUNK_SIZE ? MEM_CARD_READ_CHUNK_SIZE : readLen);
                    retVal = _2BUSExecInCommand(globalData, socket, cmdCode, 
                                                address + counter, thisRead, outBuffer + counter, outBufLen);
                    counter += thisRead;
                    readLen -= thisRead;
                }
            }
            if (retVal == ASE_OK) {
                *outBufLen = len; 

                /* Put each bit into one memory byte at the user's buffer */
                if (mode == MEM_CARD_PROT_MEM_MODE)
                    ArrangeBitsIntoBytes(bitsBuff, outBuffer, address, *outBufLen);
            }
        }
    }

    /* if command failed, clean the output buffer */
    if (retVal < 0)
        *outBufLen = 0;

    AddSW1SW2(retVal, outBuffer, outBufLen, 0);

    return ASE_OK;
}

/*****************************************************************************
*
*****************************************************************************/
static int _2BUSWriteData(reader* globalData, uchar socket, unsigned short address,
                          uchar* data, int len, uchar* outBuffer, int *outBufLen) {
    int retVal = ASE_OK, counter = 0, writeLen = len;
    unsigned short maxAddress = 0;
    uchar cmdCode = 0, thisWrite;
    uchar mode = globalData->cards[(int)socket].memCard.memType;

    switch (mode) {
        case MEM_CARD_MAIN_MEM_MODE:
            if (address > _2BUS_MAX_ADDRESS)
                retVal = MEM_CARD_ILLEGAL_ADDRESS;
            else {
                maxAddress = _2BUS_MAX_ADDRESS;
                cmdCode = _2BUS_UPDATE_DATA;
            }
            break;
        case MEM_CARD_PROT_MEM_MODE:
            if (address > _2BUS_MAX_PROTECTED_ADDRESS)
                retVal = MEM_CARD_ILLEGAL_ADDRESS;
            else {
                maxAddress = _2BUS_MAX_PROTECTED_ADDRESS;
                cmdCode = _2BUS_PROTECT_DATA_COND;
            } 
            break;
        default:
            retVal = MEM_CARD_BAD_MODE;
            break;
    }

    if (retVal == ASE_OK) {
        /* check if we are not trying to write too much */
        if (len > (maxAddress - address + 1)) {
            retVal = MEM_CARD_WRONG_LENGTH;
        }
        else {
            while (counter < len && retVal == ASE_OK) {
                thisWrite = (writeLen > MEM_CARD_WRITE_CHUNK_SIZE ? MEM_CARD_WRITE_CHUNK_SIZE : writeLen);
                retVal = _2BUSExecOutCommand(globalData, socket, cmdCode, 
                                             address + counter, data + counter, thisWrite, 
                                             outBuffer, outBufLen);
                counter += thisWrite;
                writeLen -= thisWrite;
            }
        }
    }

    if (retVal < 0)
        retVal = MEM_CARD_WRITE_FAILED; 

    AddSW1SW2(retVal, outBuffer, outBufLen, 0);

    return ASE_OK;
}

/*****************************************************************************
*
*****************************************************************************/
static int _2BUSVerifyPSC(reader* globalData, uchar socket, 
                          uchar* password, int len, uchar* outBuffer, int *outBufLen) {
    uchar errorCounter[4];
    uchar dataToWrite, i, attempts, newAttempts;
    int retVal = ASE_OK, actual;

    if (len != _2BUS_PSC_SIZE) {
        AddSW1SW2(MEM_CARD_INVALID_PARAMETER, outBuffer, outBufLen, 0);
        return retVal;
    }

    /* Read Error Counter - address has no effect in this command */
    actual = 4;
    retVal = _2BUSExecInCommand(globalData, socket, _2BUS_READ_SECURITY_MEM, 0, 4, errorCounter, &actual);
    if (retVal < 0) {
        AddSW1SW2(retVal, outBuffer, outBufLen, 0);
        return ASE_OK;
    }

    /* Count number of erased bits */
    attempts = 0;
    for (i = 1; i != 0x08; i <<= 1) {
        if (errorCounter[0] & i)
            attempts++;
    }

    /* Check if the card is already blocked */
    if (attempts == 0) {
        AddSW1SW2(MEM_CARD_BLOCKED, outBuffer, outBufLen, 0);
        return ASE_OK;
    }
    
    /* find an erased bit - only 3 bits in counter */
    for (i = 1; i != 0x08; i <<= 1) {
        if (errorCounter[0] & i) {
            dataToWrite = (~i & errorCounter[0]);
            break;
        }
    }

    /* Write the erased bit */
    retVal = _2BUSExecOutCommand(globalData, socket, 
                                  _2BUS_UPDATE_SECURITY_MEM, _2BUS_ERROR_COUNTER_ADDRESS, 
                                  &dataToWrite, 1, outBuffer, outBufLen);
    if (retVal < 0) {
        AddSW1SW2(retVal, outBuffer, outBufLen, 0);
        return ASE_OK;
    }

    /* Verify PSC data */
    retVal = _2BUSExecOutCommand(globalData, socket, 
                                  _2BUS_COMPARE_VERIFICATION, _2BUS_PSC_CODE_1_ADDRESS, 
                                  password, _2BUS_PSC_SIZE, outBuffer, outBufLen);
    if (retVal < 0) {
        AddSW1SW2(retVal, outBuffer, outBufLen, 0);
        return ASE_OK;
    }

    /* Try to clear the error counter */
    dataToWrite = 0xff;
    retVal = _2BUSExecOutCommand(globalData, socket, 
                                  _2BUS_UPDATE_SECURITY_MEM, _2BUS_ERROR_COUNTER_ADDRESS, 
                                  &dataToWrite, 1, outBuffer, outBufLen);
    if (retVal < 0) {
        AddSW1SW2(retVal, outBuffer, outBufLen, 0);
        return ASE_OK;
    }

    /* Read Error Counter - address has no effect in this command */
    actual = 4;
    retVal = _2BUSExecInCommand(globalData, socket, _2BUS_READ_SECURITY_MEM, 0, 4, errorCounter, &actual);
    if (retVal < 0) {
        AddSW1SW2(retVal, outBuffer, outBufLen, 0);
        return ASE_OK;
    }

    /* Count number of erased bits */
    newAttempts = 0;
    for (i = 1; i != 0x08; i <<= 1) {
        if (errorCounter[0] & i)
            newAttempts++;
    }

    if (newAttempts < attempts)
        AddSW1SW2(MEM_CARD_VERIFY_FAILED, outBuffer, outBufLen, newAttempts);
    else
        AddSW1SW2(ASE_OK, outBuffer, outBufLen, 0);
    
    return ASE_OK;
}

/*****************************************************************************
*
*****************************************************************************/
static int _2BUSChangePSC (reader* globalData, uchar socket, 
                           uchar* password, int len, uchar* outBuffer, int *outBufLen) {
    int retVal = ASE_OK;

    if (len != 2 * _2BUS_PSC_SIZE) {
        AddSW1SW2(MEM_CARD_INVALID_PARAMETER, outBuffer, outBufLen, 0);
        return retVal;
    }

    retVal = _2BUSVerifyPSC(globalData, socket, password, _2BUS_PSC_SIZE, outBuffer, outBufLen);
    if (retVal < 0) {
        return ASE_OK;
    }

    if (*outBufLen >= 2 && outBuffer[*outBufLen - 2] == 0x90 && outBuffer[*outBufLen - 1] == 0x00) {
        /* verification has succeeded - try to change the password */
        retVal = _2BUSExecOutCommand(globalData, socket, 
                                      _2BUS_UPDATE_SECURITY_MEM, _2BUS_PSC_CODE_1_ADDRESS, 
                                      password + 3, _2BUS_PSC_SIZE, outBuffer, outBufLen);
        if (retVal < 0) {
            *outBufLen = 0; /* replace the 9000 of the Verify command */
            AddSW1SW2(retVal, outBuffer, outBufLen, 0);
            return ASE_OK;
        }
    }

    return ASE_OK;
}


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// 3BUS cards functions
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////


/*****************************************************************************
*
*****************************************************************************/
static int _3BUSExecInCommand (reader* globalData, uchar socket, uchar mode,
                               uchar cmdCode, unsigned short address, uchar len,
                               uchar* outBuffer, int *outBufLen) {
    int retVal, actual = 0;

    if (mode == MEM_CARD_MAIN_MEM_MODE)
        retVal = MemoryCardTransact(globalData, socket, 1, cmdCode, address, 
                                    len, NULL, (char*)outBuffer, &actual);
    else
        retVal = MemoryCardTransact(globalData, socket, 2, cmdCode, address, 
                                    len, NULL, (char*)outBuffer, &actual);

    *outBufLen += actual;
    if (retVal < 0 || actual != len) {
        return MEM_CARD_COMM_ERROR;
    }

    return ASE_OK;
}


/*****************************************************************************
*
*****************************************************************************/
static int _3BUSExecOutCommand (reader* globalData, uchar socket, 
                                uchar cmdCode, unsigned short address, 
                                uchar* data, uchar len,
                                uchar* outBuffer, int *outBufLen) {
    int retVal;                      

    retVal = MemoryCardTransact(globalData, socket, 0, cmdCode, address, 
                                len, data, (char*)outBuffer, outBufLen);
    if (retVal < 0) {
        return MEM_CARD_COMM_ERROR;
    }

    return ASE_OK;
}    


/*****************************************************************************
*
*****************************************************************************/
static int _3BUSReadData(reader* globalData, uchar socket, 
                         unsigned short address, int len, uchar* outBuffer, int *outBufLen) {
    int retVal = ASE_OK, counter = 0, actual, i;
    unsigned short maxAddress = 0;
    uchar cmdCode = 0, mode = globalData->cards[(int)socket].memCard.memType;
    uchar thisRead;
    unsigned short readLen = len;
    uchar temp[600];


    switch (mode) {
        case MEM_CARD_MAIN_MEM_MODE:
            if (address > _3BUS_MAX_ADDRESS) {
                retVal = MEM_CARD_ILLEGAL_ADDRESS;
            }
            else {
                maxAddress = _3BUS_MAX_ADDRESS;
                cmdCode = _3BUS_READ_DATA;
            }
            break;
        case MEM_CARD_PROT_MEM_MODE:
            if (address > _3BUS_MAX_ADDRESS)
                retVal = MEM_CARD_ILLEGAL_ADDRESS;
            else {
                maxAddress = _3BUS_MAX_ADDRESS;
                cmdCode = _3BUS_READ_DATA_AND_PBIT;
                readLen *= 2; /* we are reading 9 bits for each prot. bit stored in two bytes */
            } 
            break;
        default:
            retVal = MEM_CARD_BAD_MODE;
            break;
    }

    if (retVal == ASE_OK) {
        /* check if we are not trying to read too much */
        if (len > (maxAddress - address + 1))
            retVal = MEM_CARD_WRONG_LENGTH;
        else {
            if (mode == MEM_CARD_PROT_MEM_MODE) {
                int chunk = ((MEM_CARD_READ_CHUNK_SIZE % 2) ? (MEM_CARD_READ_CHUNK_SIZE - 1) : MEM_CARD_READ_CHUNK_SIZE);
                while (counter < 2 * len && retVal == ASE_OK) {
                    thisRead = (readLen > chunk  ? chunk : readLen);
                    retVal = _3BUSExecInCommand(globalData, socket, mode, cmdCode, 
                                                address + counter, thisRead, temp + counter, &actual);
                    counter += thisRead;
                    readLen -= thisRead;
                }

                /* remove the data bytes */
                for (i = 0 ; i < len ; ++i)
                    outBuffer[i] = temp[1 + i * 2]; 
            }
            else {
                while (counter < len && retVal == ASE_OK) {
                    thisRead = (readLen > MEM_CARD_READ_CHUNK_SIZE ? MEM_CARD_READ_CHUNK_SIZE : readLen);
                    retVal = _3BUSExecInCommand(globalData, socket, mode, cmdCode, 
                                                address + counter, thisRead, outBuffer + counter, outBufLen);
                    counter += thisRead;
                    readLen -= thisRead;
                }
            }

            *outBufLen = len; 
        }
    }

    /* if command failed, clean the output buffer */
    if (retVal < 0)
        *outBufLen = 0;

    AddSW1SW2(retVal, outBuffer, outBufLen, 0);

    return ASE_OK;
}


/*****************************************************************************
*
*****************************************************************************/
static int _3BUSWriteData(reader* globalData, uchar socket, unsigned short address,
                          uchar* data, int len, uchar* outBuffer, int *outBufLen) {
    int retVal = ASE_OK, counter = 0, writeLen = len;
    unsigned short maxAddress = 0;
    uchar cmdCode = 0, thisWrite;
    uchar mode = globalData->cards[(int)socket].memCard.memType;

    switch (mode) {
        case MEM_CARD_MAIN_MEM_MODE:
            if (address > _3BUS_MAX_ADDRESS)
                retVal = MEM_CARD_ILLEGAL_ADDRESS;
            else {
                maxAddress = _3BUS_MAX_ADDRESS;
                cmdCode = _3BUS_UPDATE_DATA;
            }
            break;
        case MEM_CARD_PROT_MEM_MODE:
            if (address > _3BUS_MAX_ADDRESS)
                retVal = MEM_CARD_ILLEGAL_ADDRESS;
            else {
                maxAddress = _3BUS_MAX_ADDRESS;
                cmdCode = _3BUS_WRITE_PBIT_COND;
            } 
            break;
        default:
            retVal = MEM_CARD_BAD_MODE;
            break;
    }

    if (retVal == ASE_OK) {
        /* check if we are not trying to write too much */
        if (len > (maxAddress - address + 1)) {
            retVal = MEM_CARD_WRONG_LENGTH;
        }
        else {
            while (counter < len && retVal == ASE_OK) {
                thisWrite = (writeLen > MEM_CARD_WRITE_CHUNK_SIZE ? MEM_CARD_WRITE_CHUNK_SIZE : writeLen);
                retVal = _3BUSExecOutCommand(globalData, socket, cmdCode, 
                                             address + counter, data + counter, thisWrite, 
                                             outBuffer, outBufLen);
                counter += thisWrite;
                writeLen -= thisWrite;
            }
        }
    }

    if (retVal < 0)
        retVal = MEM_CARD_WRITE_FAILED; 

    AddSW1SW2(retVal, outBuffer, outBufLen, 0);

    return ASE_OK;
}


/*****************************************************************************
*
*****************************************************************************/
static int _3BUSVerifyPSC(reader* globalData, uchar socket, 
                          uchar* password, int len, uchar* outBuffer, int *outBufLen) {
    uchar errorCounter, dataToWrite, i, attempts, newAttempts;
    int retVal = ASE_OK, actual;

    if (len != _3BUS_PSC_SIZE) {
        AddSW1SW2(MEM_CARD_INVALID_PARAMETER, outBuffer, outBufLen, 0);
        return retVal;
    }

    /* Read Error Counter - address has no effect in this command */
    retVal = _3BUSExecInCommand(globalData, socket, MEM_CARD_MAIN_MEM_MODE,
                                _3BUS_READ_DATA, _3BUS_ERROR_COUNTER_ADDRESS, 1, &errorCounter, &actual);
    if (retVal < 0) {
        AddSW1SW2(retVal, outBuffer, outBufLen, 0);
        return ASE_OK;
    }

    /* Count number of erased bits */
    attempts = 0;
    for (i = 1; i != 0; i <<= 1) {
        if (errorCounter & i)
            attempts++;
    }

    /* Check if the card is already blocked */
    if (attempts == 0) {
        AddSW1SW2(MEM_CARD_BLOCKED, outBuffer, outBufLen, 0);
        return ASE_OK;
    }
    
    /* find an erased bit - only 3 bits in counter */
    for (i = 1; i != 0; i <<= 1) {
        if (errorCounter & i) {
            dataToWrite = (~i & errorCounter);
            break;
        }
    }

    /* Write the erased bit */
    retVal = _3BUSExecOutCommand(globalData, socket, 
                                  _3BUS_WRITE_ERROR_COUNTER, _3BUS_ERROR_COUNTER_ADDRESS, 
                                  &dataToWrite, 1, outBuffer, outBufLen);
    if (retVal < 0) {
        AddSW1SW2(retVal, outBuffer, outBufLen, 0);
        return ASE_OK;
    }

    /* Verify PSC data */
    retVal = _3BUSExecOutCommand(globalData, socket, 
                                  _3BUS_VERIFY_PSC, _3BUS_PSC_CODE_1_ADDRESS, 
                                  password, _3BUS_PSC_SIZE, outBuffer, outBufLen);
    if (retVal < 0) {
        AddSW1SW2(retVal, outBuffer, outBufLen, 0);
        return ASE_OK;
    }

    /* Try to clear the error counter */
    dataToWrite = 0xff;
    retVal = _3BUSExecOutCommand(globalData, socket, 
                                  _3BUS_UPDATE_DATA, _3BUS_ERROR_COUNTER_ADDRESS, 
                                  &dataToWrite, 1, outBuffer, outBufLen);
    if (retVal < 0) {
        AddSW1SW2(retVal, outBuffer, outBufLen, 0);
        return ASE_OK;
    }

    /* Read Error Counter - address has no effect in this command */
    retVal = _3BUSExecInCommand(globalData, socket, MEM_CARD_MAIN_MEM_MODE,
                                _3BUS_READ_DATA, _3BUS_ERROR_COUNTER_ADDRESS, 1, &errorCounter, &actual);
    if (retVal < 0) {
        AddSW1SW2(retVal, outBuffer, outBufLen, 0);
        return ASE_OK;
    }

    /* Count number of erased bits */
    newAttempts = 0;
    for (i = 1; i != 0; i <<= 1) {
        if (errorCounter & i)
            newAttempts++;
    }

    if (newAttempts < attempts)
        AddSW1SW2(MEM_CARD_VERIFY_FAILED, outBuffer, outBufLen, newAttempts);
    else
        AddSW1SW2(ASE_OK, outBuffer, outBufLen, 0);
    
    return ASE_OK;
}


/*****************************************************************************
*
*****************************************************************************/
static int _3BUSChangePSC (reader* globalData, uchar socket, 
                           uchar* password, int len, uchar* outBuffer, int *outBufLen) {
    int retVal = ASE_OK;

    if (len != 2 * _3BUS_PSC_SIZE) {
        AddSW1SW2(MEM_CARD_INVALID_PARAMETER, outBuffer, outBufLen, 0);
        return retVal;
    }

    retVal = _3BUSVerifyPSC(globalData, socket, password, _3BUS_PSC_SIZE, outBuffer, outBufLen);
    if (retVal < 0) {
        return ASE_OK;
    }

    if (*outBufLen >= 2 && outBuffer[*outBufLen - 2] == 0x90 && outBuffer[*outBufLen - 1] == 0x00) {
        /* verification has succeeded - try to change the password */
        retVal = _3BUSExecOutCommand(globalData, socket, 
                                      _3BUS_UPDATE_DATA, _3BUS_PSC_CODE_1_ADDRESS, 
                                      password + 2, _3BUS_PSC_SIZE, outBuffer, outBufLen);
        if (retVal < 0) {
            *outBufLen = 0; /* replace the 9000 of the Verify command */
            AddSW1SW2(retVal, outBuffer, outBufLen, 0);
            return ASE_OK;
        }
    }

    return ASE_OK;
}


/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
// X/I2C cards functions
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////



/*****************************************************************************
*
*****************************************************************************/
static int I2CExecInCommand (reader* globalData, uchar socket, 
                             uchar cmdCode, unsigned short address, uchar len,
                             uchar* outBuffer, int *outBufLen) {
    int retVal, actual = 0;

    retVal = MemoryCardTransact(globalData, socket, 1, cmdCode, address, 
                                len, NULL, (char*)outBuffer, &actual);
    *outBufLen += actual;
    if (retVal < 0) {
        return MEM_CARD_COMM_ERROR;
    }

    return ASE_OK;
}

/*****************************************************************************
*
*****************************************************************************/
static int I2CExecOutCommand (reader* globalData, uchar socket, 
                              uchar cmdCode, unsigned short address, 
                              uchar* data, uchar len,
                              uchar* outBuffer, int *outBufLen) {
    int retVal;                      

    retVal = MemoryCardTransact(globalData, socket, 0, cmdCode, address, 
                                len, data, (char*)outBuffer, outBufLen);
    if (retVal < 0) {
        return MEM_CARD_COMM_ERROR;
    }

    return ASE_OK;
}    

/*****************************************************************************
*
*****************************************************************************/
static int I2CReadData(reader* globalData, uchar socket, 
                       unsigned short address, int len, uchar* outBuffer, int *outBufLen) {
    int retVal = ASE_OK, counter = 0;
    uchar cmdCode = 0;
    uchar thisRead;
    unsigned short readLen = len;

    
    cmdCode = 0xA0;

    while (counter < len && retVal == ASE_OK) {
        thisRead = (readLen > MEM_CARD_READ_CHUNK_SIZE ? MEM_CARD_READ_CHUNK_SIZE : readLen);
        retVal = I2CExecInCommand(globalData, socket, cmdCode, 
                                  address + counter, thisRead, outBuffer + counter, outBufLen);
        counter += thisRead;
        readLen -= thisRead;
    }

    if (retVal == ASE_OK) 
        *outBufLen = len; 
    else
        *outBufLen = 0;

    AddSW1SW2(retVal, outBuffer, outBufLen, 0);

    return ASE_OK;
}

/*****************************************************************************
*
*****************************************************************************/
static int I2CWriteData(reader* globalData, uchar socket, unsigned short address,
                        uchar* data, int len, uchar* outBuffer, int *outBufLen) {
    int retVal = ASE_OK, counter = 0, writeLen = len;
    uchar cmdCode = 0, thisWrite;

    cmdCode = 0xA0;

    while (counter < len && retVal == ASE_OK) {
        thisWrite = (writeLen > MEM_CARD_WRITE_CHUNK_SIZE ? MEM_CARD_WRITE_CHUNK_SIZE : writeLen);
        retVal = I2CExecOutCommand(globalData, socket, cmdCode, 
                                   address + counter, data + counter, thisWrite, 
                                   outBuffer, outBufLen);
        counter += thisWrite;
        writeLen -= thisWrite;
    }

    if (retVal < 0)
        retVal = MEM_CARD_WRITE_FAILED; 

    AddSW1SW2(retVal, outBuffer, outBufLen, 0);

    return ASE_OK;
}




/*****************************************************************************
*
*****************************************************************************/
static int VerifyAPDUValidStructure(uchar* inBuf, int inBufLen) {
    int lc, dataLen;

    if (inBufLen <= 3)
        return MEM_CARD_ERROR_CMD_STRUCT;

    /* APDU case 1 */
    if (inBufLen == 4)
        return ASE_OK;

    /* APDU case 2 */
    if (inBufLen == 5)
        return ASE_OK;

    /* APDU case 3 + 4 (len > 5 here) */
    lc = inBuf[4];
    if (lc == 0x00) /* lc should be positive */
        return MEM_CARD_ERROR_CMD_STRUCT;
    dataLen = inBufLen - 5;
    /* dataLen is equal to lc in case 3 and equal to lc+1 in case 4 */
    if (dataLen != lc && dataLen != (lc + 1))
        return MEM_CARD_ERROR_CMD_STRUCT;

    return ASE_OK;
}


/*****************************************************************************
*
*****************************************************************************/
static int SendMemoryCommand (reader* globalData, uchar socket, 
                              uchar* inBuf, int inBufLen, uchar* outBuffer, int *outBufLen) {
    uchar INS = inBuf[1];
    unsigned short address;
    char cardType = globalData->cards[(int)socket].memCard.protocolType;
    int retVal = ASE_OK;

    *outBufLen = 0;

    /* check if the class is valid */
    if (inBuf[0] != 0x00) {
        AddSW1SW2(MEM_CARD_WRONG_CLASS, outBuffer, outBufLen, 0);
        return retVal; 
    }

    switch (INS) {
        case APDU_INS_SELECT: 
            switch (cardType) {
                case PROTOCOL_MEM_2BUS:
                case PROTOCOL_MEM_3BUS:
                    if (inBuf[5] == 0x3F && inBuf[6] == 0x00)
                        globalData->cards[(int)socket].memCard.memType = MEM_CARD_MAIN_MEM_MODE;
                    else if (inBuf[5] == 0x3F && inBuf[6] == 0x01)
                        globalData->cards[(int)socket].memCard.memType = MEM_CARD_PROT_MEM_MODE;
                    else
                        AddSW1SW2(MEM_CARD_FILE_NOT_FOUND, outBuffer, outBufLen, 0);
                    break;
                case PROTOCOL_MEM_I2C:
                case PROTOCOL_MEM_XI2C:
                    if (inBuf[5] == 0x3F && inBuf[6] == 0x00)
                        globalData->cards[(int)socket].memCard.memType = MEM_CARD_MAIN_MEM_MODE;
                    else
                        AddSW1SW2(MEM_CARD_FILE_NOT_FOUND, outBuffer, outBufLen, 0);
		    break;
                default:
                    AddSW1SW2(MEM_CARD_COMM_ERROR, outBuffer, outBufLen, 0);
                    break;
            }
            break;
        case APDU_INS_VERIFY: 
            switch (cardType) {
                case PROTOCOL_MEM_2BUS:
                    retVal = _2BUSVerifyPSC(globalData, socket, inBuf + 5, inBuf[4], outBuffer, outBufLen);
                    break;
                case PROTOCOL_MEM_3BUS:
                    retVal = _3BUSVerifyPSC(globalData, socket, inBuf + 5, inBuf[4], outBuffer, outBufLen);
                    break;
                case PROTOCOL_MEM_I2C:
                case PROTOCOL_MEM_XI2C:
                default:
                    AddSW1SW2(MEM_CARD_COMM_ERROR, outBuffer, outBufLen, 0);
                    break;
            }
            break;
        case APDU_INS_CHANGE_PASSWORD: 
            switch (cardType) {
                case PROTOCOL_MEM_2BUS:
                    retVal = _2BUSChangePSC(globalData, socket, inBuf + 5, inBuf[4], outBuffer, outBufLen);
                    break;
                case PROTOCOL_MEM_3BUS:
                    retVal = _3BUSChangePSC(globalData, socket, inBuf + 5, inBuf[4], outBuffer, outBufLen);
                    break;
                case PROTOCOL_MEM_I2C:
                case PROTOCOL_MEM_XI2C:
                default:
                    AddSW1SW2(MEM_CARD_COMM_ERROR, outBuffer, outBufLen, 0);
                    break;
            }
            break;
        case APDU_INS_READ_BINARY: 
            address = (inBuf[2] << 8) | inBuf[3];
            switch (cardType) {
                case PROTOCOL_MEM_2BUS:
                    /* if p3==0 then we should read 256 bytes */
                    retVal = _2BUSReadData(globalData, socket, address, 
                                           (inBuf[4] == 0 ? 256 : inBuf[4]), outBuffer, outBufLen);
                    break;
                case PROTOCOL_MEM_3BUS:
                    retVal = _3BUSReadData(globalData, socket, address, 
                                           (inBuf[4] == 0 ? 256 : inBuf[4]), outBuffer, outBufLen);
                    break;
                case PROTOCOL_MEM_I2C:
                case PROTOCOL_MEM_XI2C:
                    retVal = I2CReadData(globalData, socket, address, 
                                         (inBuf[4] == 0 ? 256 : inBuf[4]), outBuffer, outBufLen);
                    break;
                default:
                    AddSW1SW2(MEM_CARD_COMM_ERROR, outBuffer, outBufLen, 0);
                    break;
            }
            break;
        case APDU_INS_WRITE_BINARY: 
            address = (inBuf[2] << 8) | inBuf[3];
            switch (cardType) {
                case PROTOCOL_MEM_2BUS:
                    retVal = _2BUSWriteData(globalData, socket, address, inBuf + 5, inBuf[4], outBuffer, outBufLen);
                    break;
                case PROTOCOL_MEM_3BUS:
                    retVal = _3BUSWriteData(globalData, socket, address, inBuf + 5, inBuf[4], outBuffer, outBufLen);
                    break;
                case PROTOCOL_MEM_I2C:
                case PROTOCOL_MEM_XI2C:
                    retVal = I2CWriteData(globalData, socket, address, inBuf + 5, inBuf[4], outBuffer, outBufLen);
                    break;
                default:
                    AddSW1SW2(MEM_CARD_COMM_ERROR, outBuffer, outBufLen, 0);
                    break;
            }
            break;
        default: 
            AddSW1SW2(MEM_CARD_WRONG_INST, outBuffer, outBufLen, 0);
            break;
    }

    return retVal;
}


/*****************************************************************************
*
*****************************************************************************/
int MemoryCardCommand (reader* globalData, uchar socket, 
                       uchar* inBuf, int inBufLen, uchar* outBuffer, int *outBufLen) {
    int retVal;

    retVal = VerifyAPDUValidStructure(inBuf, inBufLen);
    if (retVal < 0) {
        return MEM_CARD_ERROR_CMD_STRUCT; /* no SW1/SW2 here */
    }

    retVal = SendMemoryCommand(globalData, socket, inBuf, inBufLen, outBuffer, outBufLen); 
    if (retVal < 0) {
        return retVal;
    }

    return ASE_OK;
}
