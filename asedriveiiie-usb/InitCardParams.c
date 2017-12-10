#include "Ase.h"
#include <math.h>



/*****************************************************************************
* 
*****************************************************************************/
#define ASE_PPS_DEFAULT_USED  -1000
#define ASE_PPS_FAILED        -1001


/*****************************************************************************
* 
*****************************************************************************/
#define EPSILON 0.001

static int fi[16] = { 372, 372, 558, 744, 1116, 1488, 1860, 1, 
                      1, 512, 768, 1024, 1536, 2048, 1, 1 };
static int di[16] = { 1, 1, 2, 4, 8, 16, 32, 1, 
                      12, 20, 1, 1, 1, 1, 1, 1 };
static long fs[16] = {4000000, 5000000, 6000000, 8000000, 12000000, 16000000, 20000000, 0, 
                      0, 5000000, 7500000, 10000000, 15000000, 20000000, 0, 0};


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
    params->fTod = 372.0;
}


/*****************************************************************************
* 
*****************************************************************************/
static int MatchReaderParams(reader* globalData, char socket, struct card_params* params, ATR* atr,
                      uchar Fi, uchar Di, uchar WI, uchar N, char protocol) {
    uchar found = 0;
    int F, D, WWT;
    float cardFToD = GetFToDFactor(Fi, Di);
    float readerFToD;
    int etuFactor;
    long maxFreq = fs[Fi];
    int etus;
    
#ifdef ASE_DEBUG
    syslog(LOG_INFO, "MatchReaderParams: card Fi = %d, Di = %d, Wi = %d, cardFToD = %f\n",Fi, Di, WI, cardFToD);
#endif


    if (maxFreq >= 16000000) {
        params->freq = 0;
        //f = 16000000;
    }
    else 
    if (maxFreq >= 8000000) {
        params->freq = 1;
        //f = 8000000;
    }
    else if (maxFreq >= 4000000) {
        params->freq = 2;
        //f = 4000000;
    }
    else {
        params->freq = 3;
        //f = 2000000;
    }

    /* check to see if the card parameters are supported by the reader: if readerFToD is smaller than cardFToD */
    readerFToD = (float)(globalData->dataMemory[14]) * 256.0 + (float)(globalData->dataMemory[13]);

    if ((readerFToD - EPSILON) <= cardFToD) {
        found = 1;

        F = fi[Fi];
        D = di[Di];

        params->fTod = cardFToD;
    	globalData->cards[(int)socket].FiDi = (Fi << 4) | Di;

        etus = (int)(cardFToD + 0.5);
        params->A = (etus & 0xFF00) >> 8;
        params->B = etus & 0x00FF;
    }

    if (!found)
        return ASE_READER_NO_MATCHING_PARAMS;

    params->protocol = protocol;
    params->N = N;

    if (protocol == ATR_PROTOCOL_TYPE_T0) {
        WWT = WI * 960 * D + ASE_READER_ETU_DELTA;

        params->CWT[0] = (uchar)((WWT & 0x00FF0000) >> 16);
        params->CWT[1] = (uchar)((WWT & 0x0000FF00) >> 8);
        params->CWT[2] = (uchar)(WWT & 0x000000FF);

        /* for T=0 they are identical */
        params->BWT[0] = params->CWT[0];
        params->BWT[1] = params->CWT[1];
        params->BWT[2] = params->CWT[2];
    }
    else { /* T = 1 */
        WWT = 11 + (1 << GetT1CWI(atr)) + ASE_READER_ETU_DELTA;

        params->CWT[0] = (uchar)((WWT & 0x00FF0000) >> 16);
        params->CWT[1] = (uchar)((WWT & 0x0000FF00) >> 8);
        params->CWT[2] = (uchar)(WWT & 0x000000FF);

        etuFactor = (int)((float)(372.0 / cardFToD) + 0.5);
        if (etuFactor == 0)
            etuFactor = 1;

#ifdef ASE_DEBUG
        syslog(LOG_INFO, "MatchReaderParams: etuFactor = %d\n", etuFactor);
#endif

        WWT = 11 + (1 << GetT1BWI(atr)) * 960 * etuFactor + ASE_READER_ETU_DELTA; 

        params->BWT[0] = (uchar)((WWT & 0x00FF0000) >> 16);
        params->BWT[1] = (uchar)((WWT & 0x0000FF00) >> 8);
        params->BWT[2] = (uchar)(WWT & 0x000000FF);
    }

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "MatchReaderParams: freq = %d, A = %d, B = %d, WWT = %d, protocol = %d, N = %d\n",
	        params->freq, params->A, params->B, WWT, params->protocol, params->N);
#endif

    return ASE_OK;
}



/*****************************************************************************
* 
*****************************************************************************/
static int PPS (reader* globalData, char socket, 
         uchar protocol, uchar* PPS1, uchar* PPS2, uchar* PPS3) {
    int retVal, len, actual, PPS1echoed, PPS2echoed, PPS3echoed;
    uchar request[6], response[6];
    uchar PPSSreq, PPS0req, PCKreq, checksum;

#ifdef ASE_DEBUG
    int i;
    syslog(LOG_INFO, "\n PPS - Enter - protocol = %d\n", protocol);
#endif

    PPSSreq = request[0] = 0xFF;
    PPS0req = request[1] = 0x00 | protocol;

    /* PPS1 must always appear -> if not passed as a parameter, take from the globalData!!!
     otherwise, we propose to work with Fd and Dd - and that's not usually the intention */
    /*    if (PPS1) */
        PPS0req = request[1] = PPS0req | 0x10;
    if (PPS2)
        PPS0req = request[1] = PPS0req | 0x20;
    if (PPS3)
        PPS0req = request[1] = PPS0req | 0x40;
    PCKreq = request[0] ^ request[1];
    len = 2;

    if (PPS1) {
        request[len] = *PPS1;
    }
    else {
        request[len] = globalData->cards[(int)socket].FiDi;
    }


    PCKreq ^= request[len];
    len++;
    if (PPS2) {
        request[len] = *PPS2;
        PCKreq ^= request[len];
        len++;
    }
    if (PPS3) {
        request[len] = *PPS3;
        PCKreq ^= request[len];
        len++;
    }
    request[len] = PCKreq;
    len++;

    if ((retVal = cardCommandInit(globalData, socket, 0)))
        return retVal;

    /* init the protocol if it's a PPS to T=1 */
    if (protocol == ATR_PROTOCOL_TYPE_T1)
	    T1InitProtocol(globalData, socket, 0);

    retVal = PPSTransact(globalData, socket, (char*)request, len, (char*)response, &actual);

    if (retVal < 0) {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "PPS - Error!.\n");
#endif
        return retVal; 
    }

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "PPS: response = ");
    for (i = 0 ; i < actual ; ++i) 
    	syslog(LOG_INFO, "0x%x%x ", PRINT_CHAR(response[i]));
    syslog(LOG_INFO, "\n");
#endif

    /* parse the reuslt */
    if (actual < 3) /* must return at least 3 bytes */
        return ASE_PPS_FAILED;

    len = 2;
    if (response[0] == PPSSreq) { /* PPSS */
        checksum = response[0];
        if ((response[1] & 0x0F) == (PPS0req & 0x0F)) { /* PPS0 */
            checksum ^= response[1];
            PPS1echoed = response[1] & 0x10;
            PPS2echoed = response[1] & 0x20;
            PPS3echoed = response[1] & 0x40;

            if (PPS1echoed) { 
		/*                if (PPS1 && response[len] == *PPS1)  // if echoed, should be identical */
                if (response[len] == request[2])  /* if echoed, should be identical */
                    checksum ^= response[len];
                else 
                    return ASE_PPS_FAILED;
                len++;
            }
            
            if (PPS2echoed) { 
                if (PPS2 && response[len] == *PPS2)  /* if echoed, should be identical */
                    checksum ^= response[len];
                else 
                    return ASE_PPS_FAILED;
                len++;
            }
            
            if (PPS3echoed) { 
                if (PPS3 && response[len] == *PPS3)  /* if echoed, should be identical */
                    checksum ^= response[len];
                else 
                    return ASE_PPS_FAILED;
                len++;
            }
            
            if (response[len] == checksum) { 
                if (!PPS1echoed)
                    return ASE_PPS_DEFAULT_USED;
                return ASE_OK;
            }
            else 
                return ASE_PPS_FAILED;
        }
        else
            return ASE_PPS_FAILED;
    }
    else
        return ASE_PPS_FAILED;
}


/*****************************************************************************
* 
*****************************************************************************/
static int DoPPS (reader* globalData, char socket, struct card_params* params, uchar protocol, 
                  int voltage) {
    int  retVal;
    ATR* atr = &(globalData->cards[(int)socket].atr);
    float cardFToD;
    float readerFToD;
    uchar PPS1;
    int cardDi = GetDi(atr), cardFi = GetFi(atr), divisionFactor;
    long cardClock = fs[cardFi], myClock;


    cardClock = (cardClock >= 16000000 ? 16000000 : cardClock >= 8000000 ? 8000000 : 4000000);
    myClock = cardClock;
    divisionFactor = (myClock == 4000000 ? 1 : myClock == 8000000 ? 2 : 4);

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "DoPPS: initial clock = %d\n", (int)myClock);
#endif

    /* check to see if the card parameters are supported by the reader: if readerFToD is smaller than cardFToD */
    readerFToD = (float)(globalData->dataMemory[14]) * 256.0 + (float)(globalData->dataMemory[13]);

    while (myClock >= 4000000) {
        cardDi = GetDi(atr);

        while (cardDi >= 1) {
	        /* use only permitted values */
	        if (cardDi != 7 && cardDi <= 9) {
		        /* calculate the card's f/d factor */
		        cardFToD = GetFToDFactor(cardFi, cardDi);
		        
		        if (divisionFactor != 1)
		            cardFToD /= (float)divisionFactor;
		        
#ifdef ASE_DEBUG
		        syslog(LOG_INFO, "DoPPS: card Fi = %d, Di = %d, F = %d, D = %d cardFToD = %f\n", 
	                       cardFi, cardDi, fi[cardFi], di[cardDi], cardFToD);
#endif
		        
                if ((readerFToD - EPSILON) <= cardFToD) {
#ifdef ASE_DEBUG
  			        syslog(LOG_INFO, "DoPPS: F = %d, D = %d\n", cardFi, cardDi);
#endif
			        
			        PPS1 = (cardFi << 4) | cardDi; 
			        retVal = PPS(globalData, socket, protocol, &PPS1, NULL, NULL);
			        if (retVal == ASE_OK) { 
			            /* PPS succeeded - compute the reader's params */
			            retVal = MatchReaderParams(globalData, socket, params, atr, cardFi, cardDi, 
						               GetWI(atr), GetN(atr), protocol);
			            
			            /* force the reader's clock to current clock */
			            params->freq = (myClock == 16000000 ? 0 : myClock == 8000000 ? 1 : 2);
			            
			            return retVal;
			        }
			        else { /* PPS failed */
#ifdef ASE_DEBUG
			            syslog(LOG_INFO, "DoPPS: PPS failed - retVal = %d.\n", retVal);
#endif
			            myClock /= 2;
			            divisionFactor /= 2;
			            
			            /* to exit the do-while loop over cardDi */
			            cardDi = 0;
			        }
	            } /* if */
		            
	        } /* if */

	        cardDi--;

	    } /* while (cardDi >= 1) */

    } /* while myClock */

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "\n********DoPPS: No parameters found in PPS - default parameters will be used.\n\n");
#endif

    /* proceed with default baudrate - it must succeed */
	MatchReaderParams(globalData, socket, params, atr, 1 /*Fi*/, 1 /*Di*/, 
                      GetWI(atr), GetN(atr), protocol);

    usleep(10000); /* wait at least 10ms between two cold resets */

    /* must do a Cold Reset before proceeding with default values */
    return CardPowerOn(globalData, socket, 0, voltage);
}


/*****************************************************************************
* 
*****************************************************************************/
int InitCard (reader* globalData, char socket, char coldReset, char* protocol) {
    int retVal, actual, working = 0, retryTimes = 3, voltage = 3;
    uchar s[300], r[300];
    struct card_params params;
    double freq, wwt;
    ATR* atr = &(globalData->cards[(int)socket].atr);
    
    // bit 0 -> 5V, bit 1 -> 3V, bit 2 -> 1.8V support
    uchar readerVoltage = globalData->dataMemory[9];
    
    GetDefaultReaderParams(globalData, &params);
    //////////////////
    retVal = SetCardParameters(globalData, socket, params);
    
    /* Check to see if we have a card in the socket */
    if (globalData->cards[(int)socket].status) {
        globalData->cards[(int)socket].cwt = 3000000; /* 3sec */
        globalData->cards[(int)socket].activeProtocol = ATR_PROTOCOL_TYPE_T0;
        
        /* Call CardPowerOn to do a cold reset. Store and parse the ATR returned.
        (first call it with 3V and if no answer is returned, try with 5V) */
        
        if (coldReset) {
#ifdef ASE_DEBUG
            syslog(LOG_INFO, "\n Trying CardPowerOn with %dV\n\n", voltage);
#endif
            retVal = CardPowerOn(globalData, socket, 0, voltage);
			/* if the card supports 1.8V and the reader too - > switch to 1.8V */
			if (retVal == ASE_OK && (GetClassIndicator(atr) & 0x04) && (readerVoltage & 0x04)) {
				voltage = 2;
#ifdef ASE_DEBUG
                syslog(LOG_INFO, "\n ReTrying CardPowerOn with %dV classIndicator = %d\n\n", voltage,(GetClassIndicator(atr) & 0x01));
#endif
                usleep(10000); /* wait at least 10ms between two cold resets */
                retVal = CardPowerOn(globalData, socket, 0, voltage);
                if (retVal < 0) {
                    voltage = 3;
                    /* return to 3V since this already works */
                    usleep(10000);
#ifdef ASE_DEBUG
                    syslog(LOG_INFO, "\n ReTrying CardPowerOn with %dV classIndicator = %d\n\n", voltage,(GetClassIndicator(atr) & 0x01));
#endif
                    retVal = CardPowerOn(globalData, socket, 0, voltage);
                }
			}
			else {
				/* if the card supports 3V and the reader too -> we're done */
			    if (retVal == ASE_OK && (GetClassIndicator(atr) & 0x02) && (readerVoltage & 0x02)) {
#ifdef ASE_DEBUG
				syslog(LOG_INFO, "\n 3V it is\n\n");
#endif
			    }
				/* if the card supports only 5V and the reader doesnt -> we cannot power on this card */
				if (retVal == ASE_OK && !(GetClassIndicator(atr) & 0x02) && (GetClassIndicator(atr) & 0x01) && !(readerVoltage & 0x01))
					return ASE_READER_CARD_REJECTED;
			}

            /* We should switch to 5V if either we didnt get a valid atr or the class indicator
               in the atr supports 5V (class A) and the reader too */
            /* NOTE: we check here that the card doesnt support 3V so that we dont enter the cond if we're done */
            if (retVal < 0 || (!(GetClassIndicator(atr) & 0x02) && (GetClassIndicator(atr) & 0x01) && (readerVoltage & 0x01))) {
                voltage = 5;
#ifdef ASE_DEBUG
                syslog(LOG_INFO, "\n ReTrying CardPowerOn with %dV classIndicator = %d\n\n", voltage,(GetClassIndicator(atr) & 0x01));
#endif
                usleep(10000); /* wait at least 10ms between two cold resets */
                retVal = CardPowerOn(globalData, socket, 0, voltage);

				/* if the card supports 1.8V and the reader too - > switch to 1.8V */
				if (retVal == ASE_OK && (GetClassIndicator(atr) & 0x04) && (readerVoltage & 0x04)) {
					voltage = 2;
#ifdef ASE_DEBUG
					syslog(LOG_INFO, "\n ReTrying CardPowerOn with %dV classIndicator = %d\n\n", voltage,(GetClassIndicator(atr) & 0x01));
#endif
					usleep(10000); /* wait at least 10ms between two cold resets */
					retVal = CardPowerOn(globalData, socket, 0, voltage);
                    if (retVal < 0) {
                        voltage = 5;
                        /* return to 5V since this already works */
                        usleep(10000);
#ifdef ASE_DEBUG
                        syslog(LOG_INFO, "\n ReTrying CardPowerOn with %dV classIndicator = %d\n\n", voltage,(GetClassIndicator(atr) & 0x01));
#endif
                        retVal = CardPowerOn(globalData, socket, 0, voltage);
                    }
				}

                if (retVal < 0) {
                    voltage = 5;
#ifdef ASE_DEBUG
                    syslog(LOG_INFO, "\n ReTrying CardPowerOn with 2BUS memory card\n\n");
#endif
                    retVal = CardPowerOn(globalData, socket, 0x12, voltage);
                    if (retVal < 0 || (atr->data[0] != 0xA2 && atr->data[0] != 0x92)) {
#ifdef ASE_DEBUG
                        syslog(LOG_INFO, "\n ReTrying CardPowerOn with I2C memory card\n\n");
#endif
                        retVal = CardPowerOn(globalData, socket, 0x21, voltage);
                        if (retVal < 0) {
                            return ASE_READER_CARD_REJECTED; 
                        }
                        
                        /* To check if this card is supported by the reader, we send a read command.
                        If the command doesnt succeed, we reject the card. */
                        {
                            unsigned char curStatus = globalData->cards[(int)socket].status;
                            globalData->cards[(int)socket].memCard.memType = MEM_CARD_MAIN_MEM_MODE;
                            globalData->cards[(int)socket].status = 2; /* temporarilly */
                            globalData->cards[(int)socket].memCard.protocolType = PROTOCOL_MEM_I2C;
                            
                            s[0] = 0x0;
                            s[1] = 0xB0;
                            s[2] = 0x0;
                            s[3] = 0x0;
                            s[4] = 0x4;
                            retVal = MemoryCardCommand(globalData, socket, s, 5, r, &actual);
                            if (retVal < 0 || (actual >= 2 && r[actual - 2] != 0x90)) {
                                // try with XI2C
#ifdef ASE_DEBUG
                                syslog(LOG_INFO, "\n ReTrying CardPowerOn with XI2C memory card\n\n");
#endif
                                params.protocol = 0x22;
                                retVal = SetCardParameters(globalData, socket, params);
                                if (retVal < 0) {
                                    globalData->cards[(int)socket].status = curStatus;
                                    return ASE_READER_CARD_REJECTED; 
                                }
                                {
                                    globalData->cards[(int)socket].memCard.protocolType = PROTOCOL_MEM_XI2C;
                                    
                                    s[0] = 0x0;
                                    s[1] = 0xB0;
                                    s[2] = 0x0;
                                    s[3] = 0x0;
                                    s[4] = 0x4;
                                    retVal = MemoryCardCommand(globalData, socket, s, 5, r, &actual);
                                    globalData->cards[(int)socket].status = curStatus;
                                    if (retVal < 0 || (actual >= 2 && r[actual - 2] != 0x90)) {
                                        return ASE_READER_CARD_REJECTED; 
                                    }
                                }
                                globalData->cards[(int)socket].status = curStatus;
                                return ASE_READER_CARD_REJECTED; 
                            }
                            globalData->cards[(int)socket].status = curStatus;
                        }
                    }
                    globalData->cards[(int)socket].activeProtocol = ATR_PROTOCOL_TYPE_RAW;
                }
            }
        }
        /* Warm reset */
        else {
            retVal = CPUCardReset(globalData, socket); 
            if (retVal < 0) {
                return ASE_READER_NOT_CPU_CARD;  
            }
        }
        
        /* Check if it's a memory card */
        if (globalData->cards[(int)socket].activeProtocol == ATR_PROTOCOL_TYPE_RAW) {
            if ( atr->data[0] == 0xA2 /*&& atr->data[1] == 0x13 && atr->data[2] == 0x10 && atr->data[3] == 0x91*/) {
#ifdef ASE_DEBUG
                syslog(LOG_INFO, "\nInitCard - this is a 2BUS card\n\n");
#endif
                params.protocol = 0x11; /* 2BUS */
                globalData->cards[(int)socket].memCard.protocolType = PROTOCOL_MEM_2BUS;
            }
            else if ( atr->data[0] == 0x92) {
#ifdef ASE_DEBUG
                syslog(LOG_INFO, "\nInitCard - this is a 3BUS card\n\n");
#endif
                params.protocol = 0x12; /* 3BUS */
                globalData->cards[(int)socket].memCard.protocolType = PROTOCOL_MEM_3BUS;
            }
            else { /* X/I2C */
#ifdef ASE_DEBUG
                syslog(LOG_INFO, "\nInitCard - this is an I2C card\n\n");
#endif
                params.protocol = 0x21; 
                globalData->cards[(int)socket].memCard.protocolType = PROTOCOL_MEM_I2C;
            }
            
            retVal = SetCardParameters(globalData, socket, params);
            if (retVal < 0) {
                return ASE_READER_CARD_REJECTED; 
            }
            
            /* set ATR to dummy value */
            memset(atr, 0x00, sizeof(ATR));
            /*
            atr->data[0] = 0x3B;
            atr->data[1] = 0x00;
            atr->length = 2;
            */
            
            /* set bwt and cwt */
            globalData->cards[(int)socket].cwt = globalData->cards[(int)socket].bwt = 1510000; /* ~1.5sec */
            
            globalData->cards[(int)socket].memCard.memType = MEM_CARD_MAIN_MEM_MODE;
            
            globalData->cards[(int)socket].status = 2; /* card is powered on and ready for work */
            
            return ASE_OK;
        }
        
        /* Do the following until a working state has been found, or after retrying enough times: */
        while (!working && retryTimes) {
            /* In SPECIFIC_MODE, try to match the reader params to that of the card */
            if (GetCurrentMode(atr) == SPECIFIC_MODE) {
#ifdef ASE_DEBUG
                syslog(LOG_INFO, "\n***** SPECIFIC_MODE: Fi = %d, Di = %d, WI = %d, N = %d, protocol = %d\n\n", 
                    GetFi(atr), GetDi(atr), GetWI(atr), GetN(atr), GetSpecificModeProtocol(atr));
#endif
                globalData->cards[(int)socket].activeProtocol = GetSpecificModeProtocol(atr);
                retVal = MatchReaderParams(globalData, socket, &params, atr, GetFi(atr), GetDi(atr), 
                    GetWI(atr), GetN(atr), globalData->cards[(int)socket].activeProtocol);
                if (retVal < 0) { /* reader params could not be matched */
                    /* check if a warm reset can change modes */
                    if (CapableOfChangingMode(atr)) {
                        retVal = CPUCardReset(globalData, socket); /* try a warm reset */
                        if (retVal < 0) {
                            return ASE_READER_CARD_REJECTED;  
                        }
                    }
                    else {
                        return ASE_READER_CARD_REJECTED;
                    }
                }
                else { /* a match found */
                    retVal = SetCardParameters(globalData, socket, params);
                    if (retVal < 0) {
                        return ASE_READER_CARD_REJECTED; 
                    }
                    working = 1;
                }
            }
            /* In NEGOTIABLE_MODE, if we cannot match the reader params to that of the card, we can do a PPS */
            else { /* NEGOTIABLE_MODE */
#ifdef ASE_DEBUG
                syslog(LOG_INFO, "\n***** NEGOTIABLE_MODE: Fi = %d, Di = %d, WI = %d, N = %d, protocol = %d\n\n", 
                    GetFi(atr), GetDi(atr), GetWI(atr), GetN(atr), (protocol ? *protocol : GetFirstOfferedProtocol(atr)));
#endif		
                /* if the values are the default, no PPS should be done */
                if ( (GetFi(atr) == 0 || GetFi(atr) == 1) && GetDi(atr) == 1) {
#ifdef ASE_DEBUG
                    syslog(LOG_INFO, "DEFAULT PARAMETERS will be used\n");
#endif
                    globalData->cards[(int)socket].activeProtocol = (protocol ? *protocol : GetFirstOfferedProtocol(atr));
                    retVal = MatchReaderParams(globalData, socket, &params, atr, GetFi(atr), GetDi(atr), 
                        GetWI(atr), GetN(atr), globalData->cards[(int)socket].activeProtocol);
                    if (retVal < 0)
                        return ASE_READER_CARD_REJECTED;
                    else {
                        retVal = SetCardParameters(globalData, socket, params);
                        if (retVal < 0) {
                            return ASE_READER_CARD_REJECTED; 
                        }
                        working = 1;
                    }
                }
                else {
                    globalData->cards[(int)socket].activeProtocol = (protocol ? *protocol : GetFirstOfferedProtocol(atr));
                    usleep(10000); /* wait at least 10ms before sending PPS */
                    retVal = DoPPS(globalData, socket, &params, globalData->cards[(int)socket].activeProtocol, voltage);
                    
                    if (retVal < 0) { 
                        return ASE_READER_CARD_REJECTED; 
                    }
                    else { /* success */
                        retVal = SetCardParameters(globalData, socket, params);
                        if (retVal < 0) {
                            return ASE_READER_CARD_REJECTED; 
                        }
                        working= 1;
                    }
                }
            }
            
            retryTimes--;
        }
        
        if (working) {
            globalData->cards[(int)socket].status = 2; /* card is powered on and ready for work */
            
            /* compute the character waiting time max delay */
            wwt = params.CWT[0] * 65536 + params.CWT[1] * 256 + params.CWT[2]; 
            freq = globalData->dataMemory[12] * 256 + globalData->dataMemory[11];
            freq *= 1000;
            freq /= (params.freq == 0 ? 1.0 : params.freq == 1 ? 2.0 : params.freq == 2 ? 4.0 : 8.0);
            globalData->cards[(int)socket].cwt = (long)( (params.fTod * wwt) / freq * 1000000.0 ); /* in microsec */
            
            /* compute the max delay for the first response character */
            wwt = params.BWT[0] * 65536 + params.BWT[1] * 256 + params.BWT[2]; 
            freq = globalData->dataMemory[12] * 256 + globalData->dataMemory[11];
            freq *= 1000;
            freq /= (params.freq == 0 ? 1.0 : params.freq == 1 ? 2.0 : params.freq == 2 ? 4.0 : 8.0);
            globalData->cards[(int)socket].bwt = (long)( (params.fTod * wwt) / freq * 1000000.0 ); /* in microsec */
            
#ifdef ASE_DEBUG
            syslog(LOG_INFO, "InitCard: protocol = %d cwt = %d  bwt = %d (each 1000 == 1ms)\n", 
                globalData->cards[(int)socket].activeProtocol, (int)globalData->cards[(int)socket].cwt, (int)globalData->cards[(int)socket].bwt);
#endif
            
            /* if it's T=1, the protocol must be inited */
            if (globalData->cards[(int)socket].activeProtocol == ATR_PROTOCOL_TYPE_T1)
                T1InitProtocol(globalData, socket, 1);
            
            return ASE_OK;
        }
        else {
            return ASE_READER_CARD_REJECTED;
        }
    }
    
    return ASE_READER_NO_CARD_ERROR;
}


