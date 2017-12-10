/*****************************************************************
/
/ File   :   ifdhandler.c
/ Date   :   April 9, 2001
/ Purpose:   This provides reader specific low-level calls
/            for the ASE-III (USB) reader of Athena Smartcard Solutions. 
/ License:   See file LICENSE & COPYING
/
******************************************************************/

#include <pcsclite.h>
#include "ifdhandler.h"
#include <stdio.h>
#include <string.h>
#include "Ase.h"



/*****************************************************************************
*
*****************************************************************************/

static reader readerData[MAX_READER_NUM];


/*****************************************************************************
*
*****************************************************************************/
static void closeDriver ( DWORD Lun ) {
    int retVal, i, readerNum = (Lun & 0xFFFF0000) >> 16;

    /* if there are some cards that are powered on, power them off */
    for (i = 0 ; i < MAX_SOCKET_NUM ; ++i)
        if (readerData[readerNum].cards[i].status == 2) {
            retVal = CardPowerOff(&readerData[readerNum], i);
/*
            if (retVal < 0) {
                return IFD_COMMUNICATION_ERROR;
            }
*/
            readerData[readerNum].cards[0].atr.length = 0;
        }

    retVal = ReaderFinish(&readerData[readerNum]);
/*
    if (retVal < 0) {
        return IFD_COMMUNICATION_ERROR;
    }
*/
    // stop reading any response from the reader (if we're doing so currently)
    //    readerData[readerNum].io.stopReading = 1;

    CloseUSB(&readerData[readerNum]);

    readerData[readerNum].readerStarted = 0;

    readerData[readerNum].io.handle = 0;
}


/*****************************************************************************
*
*****************************************************************************/
RESPONSECODE IFDHCreateChannel ( DWORD Lun, DWORD Channel ) {

  /* Lun - Logical Unit Number, use this for multiple card slots 
     or multiple readers. 0xXXXXYYYY -  XXXX multiple readers,
     YYYY multiple slots. The resource manager will set these 
     automatically.  By default the resource manager loads a new
     instance of the driver so if your reader does not have more than
     one smartcard slot then ignore the Lun in all the functions.
     Future versions of PC/SC might support loading multiple readers
     through one instance of the driver in which XXXX would be important
     to implement if you want this.
  */
  
  /* Channel - Channel ID.  This is denoted by the following:
     0x000001 - /dev/pcsc/1
     0x000002 - /dev/pcsc/2
     0x000003 - /dev/pcsc/3
     
     USB readers may choose to ignore this parameter and query 
     the bus for the particular reader.
  */

  /* This function is required to open a communications channel to the 
     port listed by Channel.  For example, the first serial reader on COM1 would
     link to /dev/pcsc/1 which would be a sym link to /dev/ttyS0 on some machines
     This is used to help with intermachine independance.
     
     Once the channel is opened the reader must be in a state in which it is possible
     to query IFDHICCPresence() for card status.
 
     returns:

     IFD_SUCCESS
     IFD_COMMUNICATION_ERROR
  */


    UCHAR data[300];
    int retVal, len, readerNum = (Lun & 0xFFFF0000) >> 16;
    

#ifdef ASE_DEBUG
    char name[30];
    syslog(LOG_INFO, "==============================================\n");
    syslog(LOG_INFO, "ASE IIIe USB reader : entering IFDHCreateChannel Channel = 0x%04x%04x Lun = 0x%04x%04x\n", (int)((Lun >> 16) & 0xFFFF), (int)(Lun & 0xFFFF), (int)((Channel >> 16) & 0xFFFF), (int)(Channel & 0xFFFF));       
    syslog(LOG_INFO, "==============================================\n");
#endif

    /* open the communication */
    if (OpenUSB(readerData, &readerData[readerNum]) == TRUE) {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, " opened for IO.\n"/*, readerData[readerNum].io.bus_device*/);
#endif
    }
    else {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "IFDHCreateChannel: Error! Could not open %s. Existing.\n", name);
#endif
        return IFD_COMMUNICATION_ERROR;
    }

    
    /* reader open */
    retVal = ReaderStartup(&readerData[readerNum], (char*)data, &len);
    if (retVal < 0) {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "IFDHCreateChannel: Error! - in ReaderStartup\n");
        syslog(LOG_INFO, "Closing %s.\n", name);
#endif
        CloseUSB(&readerData[readerNum]);
	    return IFD_COMMUNICATION_ERROR;
    }

/*
    retVal = GetFIDITable(&readerData[readerNum], data, &len);
    if (retVal < 0) {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "IFDHCreateChannel: Error! - in GetFIDITable\n");
        syslog(LOG_INFO, "Closing %s.\n", name);
#endif
        retVal = ReaderFinish(&readerData[readerNum]);
        CloseUSB(&readerData[readerNum]);
	    return IFD_COMMUNICATION_ERROR;
    }
*/

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "==============================================\n");
    syslog(LOG_INFO, "ASE IIIe USB Reader : exisiting IFDHCreateChannel\n");       
    syslog(LOG_INFO, "==============================================\n");
#endif

    return IFD_SUCCESS;
}


/*****************************************************************************
*
*****************************************************************************/
RESPONSECODE IFDHCloseChannel ( DWORD Lun ) {
  
  /* This function should close the reader communication channel
     for the particular reader.  Prior to closing the communication channel
     the reader should make sure the card is powered down and the terminal
     is also powered down.

     returns:

     IFD_SUCCESS
     IFD_COMMUNICATION_ERROR     
  */

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "==============================================\n");
    syslog(LOG_INFO, "ASE IIIe USB Reader : entering IFDHCloseChannel Lun = 0x%04x%04x\n", (int)((Lun >> 16) & 0xFFFF), (int)(Lun & 0xFFFF));      
    syslog(LOG_INFO, "==============================================\n");
#endif
    
    closeDriver(Lun);

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "==============================================\n");
    syslog(LOG_INFO, "ASE IIIe USB Reader : exisiting IFDHCloseChannel\n");       
    syslog(LOG_INFO, "==============================================\n");
#endif

    return IFD_SUCCESS;
}


/*****************************************************************************
*
*****************************************************************************/
RESPONSECODE IFDHGetCapabilities ( DWORD Lun, DWORD Tag, 
                				   PDWORD Length, PUCHAR Value ) {
  
  /* This function should get the slot/card capabilities for a particular
     slot/card specified by Lun.  Again, if you have only 1 card slot and don't mind
     loading a new driver for each reader then ignore Lun.

     Tag - the tag for the information requested
         example: TAG_IFD_ATR - return the Atr and it's size (required).
         these tags are defined in ifdhandler.h

     Length - the length of the returned data
     Value  - the value of the data

     returns:
     
     IFD_SUCCESS
     IFD_ERROR_TAG
  */
 
    int readerNum = (Lun & 0xFFFF0000) >> 16;
    uchar socket = Lun & 0x0000FFFF;
    

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "==============================================\n");
    syslog(LOG_INFO, "ASE IIIe USB Reader : entering IFDHGetCapabilities Lun = 0x%04x%04x\n", (int)((Lun >> 16) & 0xFFFF), (int)(Lun & 0xFFFF));      
    syslog(LOG_INFO, "==============================================\n");
#endif

/*    
    if (!readerData[readerNum].fd || !DeviceIsConnected(&readerData[readerNum])) {
        closeDriver(Lun);
        return IFD_ERROR_TAG;
    }
*/
    switch (Tag) {
        case TAG_IFD_ATR: {
            /* If Length is not zero, powerICC has been performed.
             Otherwise, return NULL pointer
             Buffer size is stored in *Length */
            *Length = readerData[readerNum].cards[socket].atr.length;
#ifdef ASE_DEBUG
	    syslog(LOG_INFO, "TAG_IFD_ATR - *Length = %d\n", (int)(*Length));      
#endif
            if (*Length) {
                memcpy(Value, readerData[readerNum].cards[socket].atr.data, *Length);
            }
            break;
        }
    case TAG_IFD_SIMULTANEOUS_ACCESS: {
#ifdef ASE_DEBUG
	syslog(LOG_INFO, "TAG_IFD_SIMULTANEOUS_ACCESS\n");      
#endif
	*Length = 1;
	*Value = MAX_READER_NUM;
	break;
    }
    case TAG_IFD_SLOTS_NUMBER: {
#ifdef ASE_DEBUG
	syslog(LOG_INFO, "TAG_IFD_SLOTS_NUMBER\n");      
#endif
	*Length = 1;
	*Value = 1;
	break;
    }
    case TAG_IFD_THREAD_SAFE: {
#ifdef ASE_DEBUG
	syslog(LOG_INFO, "TAG_IFD_THREAD_SAFE\n");      
#endif
	if (*Length >= 1) {
	    *Length = 1;
	    *Value = 1;
	}
	break;
    }
	    
        default:
#ifdef ASE_DEBUG
	    syslog(LOG_INFO, "default TAG Tag = %0X\n", (int)Tag);      
#endif
            return IFD_ERROR_TAG;
    }

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "==============================================\n");
    syslog(LOG_INFO, "ASE IIIe USB Reader : exisiting IFDHGetCapabilities\n");       
    syslog(LOG_INFO, "==============================================\n");
#endif

    return IFD_SUCCESS;
}


/*****************************************************************************
*
*****************************************************************************/
RESPONSECODE IFDHSetCapabilities ( DWORD Lun, DWORD Tag, 
			       DWORD Length, PUCHAR Value ) {

  /* This function should set the slot/card capabilities for a particular
     slot/card specified by Lun.  Again, if you have only 1 card slot and don't mind
     loading a new driver for each reader then ignore Lun.

     Tag - the tag for the information needing set

     Length - the length of the returned data
     Value  - the value of the data

     returns:
     
     IFD_SUCCESS
     IFD_ERROR_TAG
     IFD_ERROR_SET_FAILURE
     IFD_ERROR_VALUE_READ_ONLY
  */

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "==============================================\n");
    syslog(LOG_INFO, "ASE IIIe USB Reader : entering IFDHSetCapabilities Lun = 0x%04x%04x\n", (int)((Lun >> 16) & 0xFFFF), (int)(Lun & 0xFFFF));      
    syslog(LOG_INFO, "==============================================\n");
#endif

/*
    if (!readerData[readerNum].fd || !DeviceIsConnected(&readerData[readerNum])) {
        closeDriver(Lun);
        return IFD_ERROR_SET_FAILURE;
    }
*/

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "==============================================\n");
    syslog(LOG_INFO, "ASE IIIe USB Reader : exisiting IFDHSetCapabilities\n");       
    syslog(LOG_INFO, "==============================================\n");
#endif

    return IFD_SUCCESS;
}


/*****************************************************************************
*
*****************************************************************************/
RESPONSECODE IFDHSetProtocolParameters ( DWORD Lun, DWORD Protocol, 
                          			     UCHAR Flags, UCHAR PTS1,
				                         UCHAR PTS2, UCHAR PTS3) {

  /* This function should set the PTS of a particular card/slot using
     the three PTS parameters sent

     Protocol  - 0 .... 14  T=0 .... T=14
     Flags     - Logical OR of possible values:
     IFD_NEGOTIATE_PTS1 IFD_NEGOTIATE_PTS2 IFD_NEGOTIATE_PTS3
     to determine which PTS values to negotiate.
     PTS1,PTS2,PTS3 - PTS Values.

     returns:

     IFD_SUCCESS
     IFD_ERROR_PTS_FAILURE
     IFD_COMMUNICATION_ERROR
     IFD_PROTOCOL_NOT_SUPPORTED
  */

    int retVal,  PPS1present, PPS2present, PPS3present;
    int readerNum = (Lun & 0xFFFF0000) >> 16;
    uchar socket = Lun & 0x0000FFFF, ppsProt;
    

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "==============================================\n");
    syslog(LOG_INFO, "ASE IIIe USB Reader : entering IFDHSetProtocolParameters Lun = 0x%04x%04x\n", (int)((Lun >> 16) & 0xFFFF), (int)(Lun & 0xFFFF));      
    syslog(LOG_INFO, "==============================================\n");
#endif
    
/*
    if (!readerData[readerNum].fd || !DeviceIsConnected(&readerData[readerNum])) {
        closeDriver(Lun);
        return IFD_COMMUNICATION_ERROR;
    }
*/

    /* works only for T=0 and T=1  */
    if (Protocol != SCARD_PROTOCOL_T0 && Protocol != SCARD_PROTOCOL_T1) {
        return IFD_PROTOCOL_NOT_SUPPORTED;
    }

    /* check if the card is absent */
    if (!readerData[readerNum].cards[socket].status)
        return IFD_COMMUNICATION_ERROR;

    PPS1present = Flags & IFD_NEGOTIATE_PTS1;
    PPS2present = Flags & IFD_NEGOTIATE_PTS2;
    PPS3present = Flags & IFD_NEGOTIATE_PTS3;

    /* in the meanwhile only protocol could be set  */
    if (PPS1present || PPS2present || PPS3present)
        return IFD_ERROR_PTS_FAILURE; 

    if (Protocol == SCARD_PROTOCOL_T0)
    	ppsProt = 0;
    else
    	ppsProt = 1;

    /* do a cold reset followed by PPS (if needed) only if the current protocol is to be changed*/
	if (readerData[readerNum].cards[socket].status == 2 && 
		((readerData[readerNum].cards[socket].activeProtocol == 0 && ppsProt != 0) ||
		(readerData[readerNum].cards[socket].activeProtocol == 1 && ppsProt != 1))) {

		retVal = InitCard(&readerData[readerNum], socket, 1, (char*)(&ppsProt));
		if (retVal < 0) {
#ifdef ASE_DEBUG
	        syslog(LOG_INFO, "IFDHSetProtocolParameters - Error! retVal = %d\n", retVal);
#endif
		    return IFD_ERROR_PTS_FAILURE;
		}
	}

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "==============================================\n");
    syslog(LOG_INFO, "ASE IIIe USB Reader : exisiting IFDHSetProtocolParameters\n");       
    syslog(LOG_INFO, "==============================================\n");
#endif

    return IFD_SUCCESS;
}


/*****************************************************************************
*
*****************************************************************************/
RESPONSECODE IFDHPowerICC ( DWORD Lun, DWORD Action, 
            			    PUCHAR Atr, PDWORD AtrLength ) {

  /* This function controls the power and reset signals of the smartcard reader
     at the particular reader/slot specified by Lun.

     Action - Action to be taken on the card.

     IFD_POWER_UP - Power and reset the card if not done so 
     (store the ATR and return it and it's length).
 
     IFD_POWER_DOWN - Power down the card if not done already 
     (Atr/AtrLength should
     be zero'd)
 
    IFD_RESET - Perform a quick reset on the card.  If the card is not powered
     power up the card.  (Store and return the Atr/Length)

     Atr - Answer to Reset of the card.  The driver is responsible for caching
     this value in case IFDHGetCapabilities is called requesting the ATR and it's
     length.  This should not exceed MAX_ATR_SIZE.

     AtrLength - Length of the Atr.  This should not exceed MAX_ATR_SIZE.

     Notes:

     Memory cards without an ATR should return IFD_SUCCESS on reset
     but the Atr should be zero'd and the length should be zero

     Reset errors should return zero for the AtrLength and return 
     IFD_ERROR_POWER_ACTION.

     returns:

     IFD_SUCCESS
     IFD_ERROR_POWER_ACTION
     IFD_COMMUNICATION_ERROR
     IFD_NOT_SUPPORTED
  */
    int retVal, readerNum = (Lun & 0xFFFF0000) >> 16;
    uchar socket = Lun & 0x0000FFFF;
    

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "==============================================\n");
    syslog(LOG_INFO, "ASE IIIe USB Reader : entering IFDHPowerICC Lun = 0x%04x%04x\n", (int)((Lun >> 16) & 0xFFFF), (int)(Lun & 0xFFFF));      
    syslog(LOG_INFO, "==============================================\n");
#endif
    
    /* zero everything */
    *AtrLength = 0;
    memset(Atr, 0, MAX_ATR_SIZE);

/*
    if (!readerData[readerNum].fd || !DeviceIsConnected(&readerData[readerNum])) {
        closeDriver(Lun);
        return IFD_COMMUNICATION_ERROR;
    }
*/

    if (readerData[readerNum].cards[socket].activeProtocol == ATR_PROTOCOL_TYPE_RAW && Action == IFD_RESET) 
    	Action = IFD_POWER_UP;

    switch (Action) {
        case IFD_POWER_UP:
            /* check if already powered up */
            if (readerData[readerNum].cards[socket].status != 2) {
                retVal = InitCard(&readerData[readerNum], socket, 1, NULL);
                if (retVal < 0) {
#ifdef ASE_DEBUG
                    syslog(LOG_INFO, "IFDHPowerICC: Error! in InitCard status = %d\n", readerData[readerNum].cards[socket].status);
#endif
                    return IFD_ERROR_POWER_ACTION;
                }
            }
            *AtrLength = readerData[readerNum].cards[socket].atr.length;
            if (*AtrLength) {
                memcpy(Atr, readerData[readerNum].cards[socket].atr.data, *AtrLength);
            }
            break;
        case IFD_RESET:
	        if (readerData[readerNum].cards[socket].activeProtocol == ATR_PROTOCOL_TYPE_RAW) {
#ifdef ASE_DEBUG
		        syslog(LOG_INFO, "IFDHPowerICC - Error! while powering on mem card with warm reset\n");
#endif
                return IFD_ERROR_POWER_ACTION; /* warm reset is not allowed for memort cards */
	        }
            if (readerData[readerNum].cards[socket].status == 2) { /* powered up -> warm reset */
                retVal = InitCard(&readerData[readerNum], socket, 0, NULL);
                if (retVal < 0) {
#ifdef ASE_DEBUG
                    syslog(LOG_INFO, "IFDHPowerICC - Error! while powering on card with warm reset\n");
#endif
                    return IFD_COMMUNICATION_ERROR;
                }
            }
            else { /* not powered up -> cold reset */
                retVal = InitCard(&readerData[readerNum], socket, 1, NULL);
                if (retVal < 0) {
#ifdef ASE_DEBUG
                    syslog(LOG_INFO, "IFDHPowerICC - Error! while powering on card with cold reset\n");
#endif
                    return IFD_COMMUNICATION_ERROR;
                }
            }
            *AtrLength = readerData[readerNum].cards[socket].atr.length;
            if (*AtrLength) {
                memcpy(Atr, readerData[readerNum].cards[socket].atr.data, *AtrLength);
            }
            break;
        case IFD_POWER_DOWN:
            /* check if the card is not already powered off */
            if (readerData[readerNum].cards[socket].status == 2) {
                retVal = CardPowerOff(&readerData[readerNum], socket);
                if (retVal < 0) {
#ifdef ASE_DEBUG
                    syslog(LOG_INFO, "IFDHPowerICC - Error! while powering off card\n");
#endif
                    return IFD_COMMUNICATION_ERROR;
                }
            }
            readerData[readerNum].cards[socket].atr.length = 0;
            break;
        default:
            return IFD_NOT_SUPPORTED;
    }

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "==============================================\n");
    syslog(LOG_INFO, "ASE IIIe USB Reader : exiting IFDHPowerICC\n");       
    syslog(LOG_INFO, "==============================================\n");
#endif
    
    return IFD_SUCCESS;
}


/*****************************************************************************
*
*****************************************************************************/
RESPONSECODE IFDHTransmitToICC ( DWORD Lun, SCARD_IO_HEADER SendPci, 
				                 PUCHAR TxBuffer, DWORD TxLength, 
				                 PUCHAR RxBuffer, PDWORD RxLength, 
				                 PSCARD_IO_HEADER RecvPci ) {
  
  /* This function performs an APDU exchange with the card/slot specified by
     Lun.  The driver is responsible for performing any protocol specific exchanges
     such as T=0/1 ... differences.  Calling this function will abstract all protocol
     differences.

     SendPci
     Protocol - 0, 1, .... 14
     Length   - Not used.

     TxBuffer - Transmit APDU example (0x00 0xA4 0x00 0x00 0x02 0x3F 0x00)
     TxLength - Length of this buffer.
     RxBuffer - Receive APDU example (0x61 0x14)
     RxLength - Length of the received APDU.  This function will be passed
     the size of the buffer of RxBuffer and this function is responsible for
     setting this to the length of the received APDU.  This should be ZERO
     on all errors.  The resource manager will take responsibility of zeroing
     out any temporary APDU buffers for security reasons.
  
     RecvPci
     Protocol - 0, 1, .... 14
     Length   - Not used.

     Notes:
     The driver is responsible for knowing what type of card it has.  If the current
     slot/card contains a memory card then this command should ignore the Protocol
     and use the MCT style commands for support for these style cards and transmit 
     them appropriately.  If your reader does not support memory cards or you don't
     want to then ignore this.

     RxLength should be set to zero on error.

     returns:
     
     IFD_SUCCESS
     IFD_COMMUNICATION_ERROR
     IFD_RESPONSE_TIMEOUT
     IFD_ICC_NOT_PRESENT
     IFD_PROTOCOL_NOT_SUPPORTED
  */
    int retVal, actual, readerNum = (Lun & 0xFFFF0000) >> 16;
    UCHAR response[65535], socket = Lun & 0x0000FFFF;
	UCHAR TxBuffer2[5];
    

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "==============================================\n");
    syslog(LOG_INFO, "ASE IIIe USB Reader : entering IFDHTransmitToICC Lun = 0x%04x%04x\n", (int)((Lun >> 16) & 0xFFFF), (int)(Lun & 0xFFFF));      
    syslog(LOG_INFO, "==============================================\n");
#endif

/*    
    if (!readerData[readerNum].fd || !DeviceIsConnected(&readerData[readerNum])) {
        closeDriver(Lun);
        if (RxLength)
            *RxLength = 0;
        return IFD_COMMUNICATION_ERROR;
    }
*/
    if (TxBuffer == NULL || TxLength == 0) {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "ASE IIIe USB Reader : IFDHTransmitToICC - Error - no data in command!!!\n");
#endif
        if (RxLength)
	    	*RxLength = 0;
		return IFD_COMMUNICATION_ERROR;
    }

    switch (SendPci.Protocol) {
        case ATR_PROTOCOL_TYPE_T0:
            if (readerData[readerNum].cards[socket].status == 1) {
                if (RxLength)
                    *RxLength = 0;
                return IFD_COMMUNICATION_ERROR; /* not powered on */
            }
            else if (readerData[readerNum].cards[socket].status == 0) {
                if (RxLength)
                    *RxLength = 0;
                return IFD_ICC_NOT_PRESENT;
            }
            else if (readerData[readerNum].cards[socket].activeProtocol != 0) { /* current protocol isnt T=0 */
                if (RxLength)
                    *RxLength = 0;
                return IFD_PROTOCOL_NOT_SUPPORTED; 
            }
            else { /* card is powered on */
                if (RecvPci)
                    RecvPci->Protocol = ATR_PROTOCOL_TYPE_T0;
                if (TxLength > 5) {
                    // check if it's a case 4 command that should be splitted into write + get response
                    DWORD tmpLen=4 + 1 + TxBuffer[4] + 1;
                    if (TxLength == tmpLen) {

                        retVal = T0Write(&readerData[readerNum], socket, TxBuffer, TxLength - 1, response, &actual);

                        if (retVal == ASE_OK && actual == 2 && response[0] == 0x61) {
                            // now send a Get Response apdu
                            unsigned short newLen = (TxBuffer[TxLength - 1] == 0 ? 256 : TxBuffer[TxLength - 1]);
			                UCHAR Le = (newLen < response[1] ? (newLen == 256 ? 0 : newLen) : response[1]);
                            UCHAR getResponseCmd[5] = {0x00, 0xC0, 0x00, 0x00, 0x00};
			   // getResponseCmd[0] = TxBuffer[0];
			    getResponseCmd[4] = Le;
                            
                            retVal = T0Read(&readerData[readerNum], socket, getResponseCmd, 5, response, &actual);
                        }
                    }
                    else {
                        retVal = T0Write(&readerData[readerNum], socket, TxBuffer, TxLength, response, &actual);
                    }

                    if (retVal < 0 || *RxLength < (DWORD)actual) { 
                        if (RxLength)
                            *RxLength = 0;
                        return IFD_COMMUNICATION_ERROR;
                    }
                    *RxLength = actual;
                    if (*RxLength)
                        memcpy(RxBuffer, response, *RxLength);
                }
                else if (TxLength == 4) { /* case 1 */
					memcpy(TxBuffer2, TxBuffer, 4);
					TxBuffer2[4] = 0x00;
                    retVal = T0Write(&readerData[readerNum], socket, TxBuffer2, 5, response, &actual);
                    if (retVal < 0 || *RxLength < (DWORD)actual) { 
                        if (RxLength)
                            *RxLength = 0;
                        return IFD_COMMUNICATION_ERROR;
                    }
                    *RxLength = actual;
                    if (*RxLength)
                        memcpy(RxBuffer, response, *RxLength);
				}
                else {
                    retVal = T0Read(&readerData[readerNum], socket, TxBuffer, TxLength, response, &actual);
                    if (retVal < 0 || *RxLength < (DWORD)actual) { 
                        if (RxLength)
                            *RxLength = 0;
                        return IFD_COMMUNICATION_ERROR;
                    }
                    *RxLength = actual;
                    if (*RxLength)
                        memcpy(RxBuffer, response, *RxLength);
                }
            }
            break;
        case ATR_PROTOCOL_TYPE_T1:
            if (readerData[readerNum].cards[socket].status == 1) {
                if (RxLength)
                    *RxLength = 0;
                return IFD_COMMUNICATION_ERROR; /* not powered on */
            }
            else if (readerData[readerNum].cards[socket].status == 0) {
                if (RxLength)
                    *RxLength = 0;
                return IFD_ICC_NOT_PRESENT;
            }
            else if (readerData[readerNum].cards[socket].activeProtocol != 1) { /* current protocol isnt T=1 */
                if (RxLength)
                    *RxLength = 0;
                return IFD_PROTOCOL_NOT_SUPPORTED; 
            }
            else { /* card is powered on */
                if (RecvPci)
                    RecvPci->Protocol = ATR_PROTOCOL_TYPE_T1;
    	        retVal = T1Command (&readerData[readerNum], socket, TxBuffer, TxLength, response, &actual);
		        if (retVal < 0 || *RxLength < (DWORD)actual) { 
                    if (RxLength)
	    	            *RxLength = 0;
		            return IFD_COMMUNICATION_ERROR;
		        }
		        *RxLength = actual;
                if (*RxLength)
		            memcpy(RxBuffer, response, *RxLength);
            }
            break;
        default:
            if (RxLength)
                *RxLength = 0;
            return IFD_PROTOCOL_NOT_SUPPORTED;
    }

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "==============================================\n");
    syslog(LOG_INFO, "ASE IIIe USB Reader : exisiting IFDHTransmitToICC\n");       
    syslog(LOG_INFO, "==============================================\n");
#endif

    return IFD_SUCCESS;
}


/*****************************************************************************
*
*****************************************************************************/
RESPONSECODE IFDHControl ( DWORD Lun, 
                           PUCHAR TxBuffer, DWORD TxLength, 
                           PUCHAR RxBuffer, PDWORD RxLength ) {

  /* This function performs a data exchange with the reader (not the card)
     specified by Lun.  Here XXXX will only be used.
     It is responsible for abstracting functionality such as PIN pads,
     biometrics, LCD panels, etc.  You should follow the MCT, CTBCS 
     specifications for a list of accepted commands to implement.

     TxBuffer - Transmit data
     TxLength - Length of this buffer.
     RxBuffer - Receive data
     RxLength - Length of the received data.  This function will be passed
     the length of the buffer RxBuffer and it must set this to the length
     of the received data.

     Notes:
     RxLength should be zero on error.
  */

    int retVal, actual, readerNum = (Lun & 0xFFFF0000) >> 16;
    uchar socket = Lun & 0x0000FFFF;
    UCHAR response[700];

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "==============================================\n");
    syslog(LOG_INFO, "ASE IIIe USB Reader : entering IFDHControl Lun = 0x%04x%04x\n", (int)((Lun >> 16) & 0xFFFF), (int)(Lun & 0xFFFF));      
    syslog(LOG_INFO, "==============================================\n");
#endif

/*
    if (!readerData[readerNum].fd || !DeviceIsConnected(&readerData[readerNum])) {
        closeDriver(Lun);
        if (RxLength)
           *RxLength = 0;
        return IFD_COMMUNICATION_ERROR;
    }
*/

    /* check if its an ioctl for the reader */
    if (TxLength >= 4 && TxBuffer[0] == ASE_IOCTL_CLASS && TxBuffer[1] == ASE_IOCTL_XI2C_INS) {
        struct card_params params = readerData[readerNum].cards[socket].cardParams;
        params.protocol = 0x22;
        retVal = SetCardParameters(&readerData[readerNum], socket, params);
	    if (retVal < 0) { 
            if (RxLength)
    	        *RxLength = 0;
	        return IFD_COMMUNICATION_ERROR;
	    }
    }
    else if (TxLength >= 4 && TxBuffer[0] == ASE_IOCTL_CLASS && TxBuffer[1] == ASE_IOCTL_CLOCK_INS) {
        /* change clock in reader - not supported yet */
    }
    
    // other ioctls sent to the reader
    else if (TxLength >= 4 && TxBuffer[0] == ASE_IOCTL_RAW_CLASS && TxBuffer[1] == ASE_IOCTL_RAW_INS) {
        actual = (RxLength ? *RxLength : 0);
        retVal = SendIOCTL(&readerData[readerNum], socket, &TxBuffer[2], TxLength - 2, response, &actual);
	    if (retVal < 0 || *RxLength < (DWORD)actual) { 
            if (RxLength)
    	        *RxLength = 0;
	        return IFD_COMMUNICATION_ERROR;
	    }
	    *RxLength = actual;
        if (*RxLength)
	        memcpy(RxBuffer, response, *RxLength);
    }

    /* card eject */
    else if (TxLength == 5 && TxBuffer[0] == 0x20 && TxBuffer[1] == 0x15 &&
                              TxBuffer[3] == 0x00 && TxBuffer[4] == 0x00) {
        retVal = CardPowerOff(&readerData[readerNum], socket);
        if (retVal < 0) {
#ifdef ASE_DEBUG
            syslog(LOG_INFO, "IFDHControl - Error! while ejecting card\n");
#endif
            if (RxLength)
    	        *RxLength = 0;
            return IFD_COMMUNICATION_ERROR;
        }
        readerData[readerNum].cards[socket].atr.length = 0;
        // return success status
	    *RxLength = 2;
	    RxBuffer[0] = 0x90;
	    RxBuffer[1] = 0x00;

        // try to eject the card (dont care if fail)
        {
            unsigned char EXTRACT_CARD[] = {0x50, 0x63, 0x0, 0x33};

            actual = 2;
            retVal = SendIOCTL(&readerData[readerNum], socket, EXTRACT_CARD, sizeof(EXTRACT_CARD), response, &actual);
        }
    }

    else if (readerData[readerNum].cards[socket].activeProtocol == ATR_PROTOCOL_TYPE_RAW) {
        if (readerData[readerNum].cards[socket].status == 1 || readerData[readerNum].cards[socket].status == 0) {
            if (RxLength)
                *RxLength = 0;
            return IFD_COMMUNICATION_ERROR; /* not powered on or absent */
        }
	
        retVal = MemoryCardCommand (&readerData[readerNum], socket, TxBuffer, TxLength, response, &actual);
	    if (retVal < 0 || *RxLength < (DWORD)actual) { 
            if (RxLength)
    	        *RxLength = 0;
	        return IFD_COMMUNICATION_ERROR;
	    }
	    *RxLength = actual;
        if (*RxLength)
	        memcpy(RxBuffer, response, *RxLength);
    }

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "==============================================\n");
    syslog(LOG_INFO, "ASE IIIe USB Reader : exisiting IFDHControl\n");       
    syslog(LOG_INFO, "==============================================\n");
#endif

    return IFD_SUCCESS;
}


/*****************************************************************************
*
*****************************************************************************/
RESPONSECODE IFDHICCPresence( DWORD Lun ) {

  /* This function returns the status of the card inserted in the 
     reader/slot specified by Lun.  It will return either:

     returns:
     IFD_ICC_PRESENT
     IFD_ICC_NOT_PRESENT
     IFD_COMMUNICATION_ERROR
  */
  
    UCHAR data[300];
    int retVal, len, readerNum = (Lun & 0xFFFF0000) >> 16;
    uchar socket = Lun & 0x0000FFFF;
    
    /*
    syslog(LOG_INFO, "==============================================\n");
    syslog(LOG_INFO, "ASE IIIe USB Reader : entering IFDHICCPresence Lun = 0x%04x%04x\n", (Lun >> 16) & 0xFFFF, Lun & 0xFFFF);      
    syslog(LOG_INFO, "==============================================\n");
    */

/*
    if (!readerData[readerNum].fd || !DeviceIsConnected(&readerData[readerNum])) {
        closeDriver(Lun);
        return IFD_COMMUNICATION_ERROR;
    }
*/

    retVal = GetStatus(&readerData[readerNum], (char*)data, &len);
    if (retVal < 0) {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "IFDHICCPresence: Error! - in GetStatus\n");
#endif
        return IFD_COMMUNICATION_ERROR;
    }

    /*
    syslog(LOG_INFO, "==============================================\n");
    syslog(LOG_INFO, "ASE IIIe USB Reader : exisiting IFDHICCPresence\n");       
    syslog(LOG_INFO, "==============================================\n");
    */
#ifdef ASE_DEBUG
    syslog(LOG_INFO, "IFDHICCPresence: status = %d\n", readerData[readerNum].cards[socket].status);
#endif
    return (readerData[readerNum].cards[socket].status ? IFD_ICC_PRESENT : IFD_ICC_NOT_PRESENT);
}


