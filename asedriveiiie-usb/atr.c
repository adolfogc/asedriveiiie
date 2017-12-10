#include "Ase.h"



/*****************************************************************************
* 
*****************************************************************************/
#define ATR_INTERFACE_BYTE_TA	0	/* Interface byte TAi */
#define ATR_INTERFACE_BYTE_TB	1	/* Interface byte TBi */
#define ATR_INTERFACE_BYTE_TC	2	/* Interface byte TCi */
#define ATR_INTERFACE_BYTE_TD	3	/* Interface byte TDi */


/*****************************************************************************
* 
*****************************************************************************/
static int fi[16] = { 372, 372, 558, 744, 1116, 1488, 1860, 1, 
                      1, 512, 768, 1024, 1536, 2048, 1, 1 };
static int di[16] = { 1, 1, 2, 4, 8, 16, 32, 1, 
                      12, 20, 1, 1, 1, 1, 1, 1 };
static long fs[16] = {4000000, 5000000, 6000000, 8000000, 12000000, 16000000, 20000000, 0, 
                      0, 5000000, 7500000, 10000000, 15000000, 20000000, 0, 0};

/*static int num_ib[16] = { 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4 };*/ /* number of interface bytes */

/*****************************************************************************
* 
*****************************************************************************/
int ParseATR  (reader* globalData, char socket, char data[], int len) {
    uchar TDi;
    int pointer = 0, pn = 0, i;
    ATR* atr = &(globalData->cards[(int)socket].atr);

    memset(atr, 0x00, sizeof(ATR));

    /* Store T0 and TS */
    if (len < 1)
        return ASE_ERROR_ATR;
    atr->TS = data[0];
    atr->data[pointer] = atr->TS;

    if (atr->TS == 0x03) 
        atr->TS = 0x3F;

    if ((atr->TS != 0x3B) && (atr->TS != 0x3F))
        return ASE_ERROR_ATR;

    if (len < 2)
        return ASE_ERROR_ATR;
    atr->T0 = data[1];

    TDi = atr->T0;
    pointer = 1;
    atr->data[pointer] = atr->T0;

    /* Store number of historical bytes */
    atr->hbn = TDi & 0x0F;

    /* TCK is not present by default */
    (atr->TCK).present = 0;

    /* Extract interface bytes */
    while (1) {
        /* Check TAi is present */
        if (TDi & 0x10) {
            pointer++;
            if (len < pointer)
                return ASE_ERROR_ATR;
            atr->ib[pn][ATR_INTERFACE_BYTE_TA].value = data[pointer];
            atr->ib[pn][ATR_INTERFACE_BYTE_TA].present = 1;
            atr->data[pointer] = atr->ib[pn][ATR_INTERFACE_BYTE_TA].value;
        }
        else
            atr->ib[pn][ATR_INTERFACE_BYTE_TA].present = 0;

        /* Check TBi is present */
        if (TDi & 0x20) {
            pointer++;
            if (len < pointer)
                return ASE_ERROR_ATR;
            atr->ib[pn][ATR_INTERFACE_BYTE_TB].value = data[pointer];
            atr->ib[pn][ATR_INTERFACE_BYTE_TB].present = 1;
            atr->data[pointer] = atr->ib[pn][ATR_INTERFACE_BYTE_TB].value;
        }
        else
            atr->ib[pn][ATR_INTERFACE_BYTE_TB].present = 0;

        /* Check TCi is present */
        if (TDi & 0x40) {
            pointer++;
            if (len < pointer)
                return ASE_ERROR_ATR;
            atr->ib[pn][ATR_INTERFACE_BYTE_TC].value = data[pointer];
            atr->ib[pn][ATR_INTERFACE_BYTE_TC].present = 1;
            atr->data[pointer] = atr->ib[pn][ATR_INTERFACE_BYTE_TC].value;
        }
        else
            atr->ib[pn][ATR_INTERFACE_BYTE_TC].present = 0;

        /* Read TDi if present */
        if (TDi & 0x80) {
            pointer++;
            if (len < pointer)
                return ASE_ERROR_ATR;
            atr->ib[pn][ATR_INTERFACE_BYTE_TD].value = data[pointer];
            TDi = atr->ib[pn][ATR_INTERFACE_BYTE_TD].value;
            atr->ib[pn][ATR_INTERFACE_BYTE_TD].present = 1;
            atr->data[pointer] = atr->ib[pn][ATR_INTERFACE_BYTE_TD].value;
            (atr->TCK).present = ((TDi & 0x0F) != ATR_PROTOCOL_TYPE_T0);
            if (pn >= ATR_MAX_PROTOCOLS)
                return ASE_ERROR_ATR;
            pn++;
        }
        else {
            atr->ib[pn][ATR_INTERFACE_BYTE_TD].present = 0;
            break;
        }
    }

    /* Store number of protocols */
    atr->pn = pn + 1;

    /* Store historical bytes */
    for (i = 0; i < (atr->hbn); i++) {
        pointer++;
        if (len < pointer)
            return ASE_ERROR_ATR;
        atr->hb[i] = data[pointer];
        atr->data[pointer] = atr->hb[i];
    }

    /* Store TCK */
    if ((atr->TCK).present) {
        pointer++;
        if (len < pointer)
            return ASE_ERROR_ATR;
        (atr->TCK).value = data[pointer];
        atr->data[pointer] = atr->TCK.value;
    }

    atr->length = pointer + 1;
    return ASE_OK;
}


/*****************************************************************************
* 
*****************************************************************************/
int GetCurrentMode (ATR* atr) {
    /* TA2 */
    if (atr->ib[1][ATR_INTERFACE_BYTE_TA].present) {
        return SPECIFIC_MODE;
    }
    else
        return NEGOTIABLE_MODE;
}


/*****************************************************************************
* 
*****************************************************************************/
int CapableOfChangingMode (ATR* atr) {
    if (atr->ib[1][ATR_INTERFACE_BYTE_TA].present)  /* TA2 */
        return ((atr->ib[1][ATR_INTERFACE_BYTE_TA].value) & 0x80) == 0;
    else
        return 0;
}


/*****************************************************************************
* 
*****************************************************************************/
uchar GetSpecificModeProtocol (ATR* atr) {
    if (GetCurrentMode(atr) == SPECIFIC_MODE)
        return ((atr->ib[1][ATR_INTERFACE_BYTE_TA].value) & 0x0F); 
    else
        return UNKNOWN_PROTOCOL;
}


/*****************************************************************************
* 
*****************************************************************************/
static int UseDefaultBaudrateInSpecificMode (ATR* atr) {
    if (GetCurrentMode(atr) == SPECIFIC_MODE)
        return ((atr->ib[1][ATR_INTERFACE_BYTE_TA].value) & 0x10); /* if b5=1 default */
    else
        return 0;
}


/*****************************************************************************
* 
*****************************************************************************/
float GetFToDFactor (int F, int D) {
    return (float)(fi[F]) / (float)(di[D]);
}

/*****************************************************************************
* 
*****************************************************************************/
float GetDToFFactor (int F, int D) {
    return (float)(di[D]) / (float)(fi[F]);
}


/*****************************************************************************
* 
*****************************************************************************/
uchar GetFi (ATR* atr) {
    if (UseDefaultBaudrateInSpecificMode(atr))
        return 0;
    else if (atr->ib[0][ATR_INTERFACE_BYTE_TA].present) 
        return (atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0xF0) >> 4;
    else 
        return 1; /* 372 */
}


/*****************************************************************************
* 
*****************************************************************************/
long GetMaxFreq (ATR* atr) {
    return fs[GetFi(atr)];
}


/*****************************************************************************
* 
*****************************************************************************/
uchar GetDi (ATR* atr) {
    if (UseDefaultBaudrateInSpecificMode(atr))
        return 1;
    else if (atr->ib[0][ATR_INTERFACE_BYTE_TA].present) 
        return (atr->ib[0][ATR_INTERFACE_BYTE_TA].value & 0x0F);
    else 
        return 1; /* 1 */
}


/*****************************************************************************
* 
*****************************************************************************/
uchar GetN (ATR* atr) {
    if (atr->ib[0][ATR_INTERFACE_BYTE_TC].present) 
        return (atr->ib[0][ATR_INTERFACE_BYTE_TC].value);
    else 
        return 0; 
}


/*****************************************************************************
* 
*****************************************************************************/
uchar GetWI (ATR* atr) {
    if (atr->ib[1][ATR_INTERFACE_BYTE_TC].present) 
        return (atr->ib[1][ATR_INTERFACE_BYTE_TC].value);
    else 
        return 10; 
}


/*****************************************************************************
* 
*****************************************************************************/
uchar GetFirstOfferedProtocol (ATR* atr) {
    /* only in NEGOTIABLE_MODE */
    if (atr->ib[0][ATR_INTERFACE_BYTE_TD].present) 
        return (atr->ib[0][ATR_INTERFACE_BYTE_TD].value) & 0x0F;
    else 
        return ATR_PROTOCOL_TYPE_T0; 
}


/*****************************************************************************
* 
*****************************************************************************/
uchar GetT1IFSC (ATR* atr) {
    int i;

    for (i = 1 ; i < atr->pn ; ++i) {
        /* check for the first occurance of T=1 in TDi */
        if (atr->ib[i][ATR_INTERFACE_BYTE_TD].present && 
            (atr->ib[i][ATR_INTERFACE_BYTE_TD].value & 0x0F) == ATR_PROTOCOL_TYPE_T1) {
            /* return TA(i+1) if present */
            if (atr->ib[i + 1][ATR_INTERFACE_BYTE_TA].present)
                return atr->ib[i + 1][ATR_INTERFACE_BYTE_TA].value;
            else
                return 32; /* default */
        }
    }

    return 32;
}


/*****************************************************************************
* 
*****************************************************************************/
uchar GetT1CWI (ATR* atr) {
    int i;

    for (i = 1 ; i < atr->pn ; ++i) {
        /* check for the first occurance of T=1 in TDi */
        if (atr->ib[i][ATR_INTERFACE_BYTE_TD].present && 
            (atr->ib[i][ATR_INTERFACE_BYTE_TD].value & 0x0F) == ATR_PROTOCOL_TYPE_T1) {
            /* return TA(i+1) if present */
            if (atr->ib[i + 1][ATR_INTERFACE_BYTE_TB].present)
                return (atr->ib[i + 1][ATR_INTERFACE_BYTE_TB].value & 0x0F);
            else
                return 13; /* default */
        }
    }

    return 13;
}


/*****************************************************************************
* 
*****************************************************************************/
uchar GetT1BWI (ATR* atr) {
    int i;

    for (i = 1 ; i < atr->pn ; ++i) {
        /* check for the first occurance of T=1 in TDi */
        if (atr->ib[i][ATR_INTERFACE_BYTE_TD].present && 
            (atr->ib[i][ATR_INTERFACE_BYTE_TD].value & 0x0F) == ATR_PROTOCOL_TYPE_T1) {
            /* return TA(i+1) if present */
            if (atr->ib[i + 1][ATR_INTERFACE_BYTE_TB].present)
                return ((atr->ib[i + 1][ATR_INTERFACE_BYTE_TB].value & 0xF0) >> 4);
            else
                return 4; /* default */
        }
    }

    return 4;
}


/*****************************************************************************
* 
*****************************************************************************/
uchar GetT1EDC (ATR* atr) {
    int i;

    for (i = 1 ; i < atr->pn ; ++i) {
        /* check for the first occurance of T=1 in TDi */
        if (atr->ib[i][ATR_INTERFACE_BYTE_TD].present && 
            (atr->ib[i][ATR_INTERFACE_BYTE_TD].value & 0x0F) == ATR_PROTOCOL_TYPE_T1) {
            /* return TA(i+1) if present */
            if (atr->ib[i + 1][ATR_INTERFACE_BYTE_TC].present)
                return (atr->ib[i + 1][ATR_INTERFACE_BYTE_TC].value & 0x01);
            else
                return 0; /* default - LRC */
        }
    }

    return 0;
}


/*****************************************************************************
* 
*****************************************************************************/
int   IsT1Available (ATR* atr) {
    int i;
    
    for (i = 1 ; i < atr->pn ; ++i) {
        /* check for the first occurance of T=1 in TDi */
        if (atr->ib[i][ATR_INTERFACE_BYTE_TD].present && 
            (atr->ib[i][ATR_INTERFACE_BYTE_TD].value & 0x0F) == ATR_PROTOCOL_TYPE_T1) {
            return 1; 
        }
    }

    return 0;
}


/*****************************************************************************
* 
*****************************************************************************/
uchar GetClassIndicator (ATR* atr) {
    int i;

    for (i = 1 ; i < atr->pn ; ++i) {
        /* check for the first occurance of T=15 in TDi */
        if (atr->ib[i][ATR_INTERFACE_BYTE_TD].present && 
            (atr->ib[i][ATR_INTERFACE_BYTE_TD].value & 0x0F) == ATR_PROTOCOL_TYPE_T15) {
            /* return TA(i+1) if present */
            if (atr->ib[i + 1][ATR_INTERFACE_BYTE_TA].present)
                return (atr->ib[i + 1][ATR_INTERFACE_BYTE_TA].value & 0x3F);
            else
                return 1; /* default - only class A (5V) supported */
        }
    }

    return 1; /* default - only class A (5V) supported */
}


