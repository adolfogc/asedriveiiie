#include <stdio.h>                       /* Standard Includes     */
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <termios.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include "Ase.h"



//*****************************************************************************
//
//*****************************************************************************
/*
 * InititalizePort -- initialize a serial port
 *	This functions opens the serial port specified and sets the
 *	line parameters baudrate, bits per byte and parity according to the
 *	parameters.  If no serial port is specified, the Communications
 *	Manager's dialog is invoked to permit the user to select not only
 *	the port, but all the other (baud/bits/parity) details as well.
 *	Selections made by the user in this case will override the
 *	parameters passed.
 */
bool IO_InitializePort(reader* globalData, int baud, int bits, char parity, char* port) {

    HANDLE          handle; 
    struct termios  newtio;

    /* Try user input depending on port */
    int tmpResult=open(port, O_RDWR | O_NOCTTY);
    if (tmpResult<0) {               
        return FALSE;
    }
    handle = tmpResult;

    /*
    * Zero the struct to make sure that we don't use any garbage
    * from the stack.
    */
    memset(&newtio, 0, sizeof(newtio));

    /*
    * Set the baudrate
    */
    switch (baud) {

        case 9600:                                               
            cfsetispeed(&newtio, B9600);
            cfsetospeed(&newtio, B9600);
            break;
        case 19200:                              
            cfsetispeed(&newtio, B19200);
            cfsetospeed(&newtio, B19200);
            break;
        case 38400:                                           /* Baudrate 38400  */
            cfsetispeed(&newtio, B38400);
            cfsetospeed(&newtio, B38400);
            break;
        case 57600:                                           /* Baudrate 57600  */
            cfsetispeed(&newtio, B57600);
            cfsetospeed(&newtio, B57600);
            break;
        case 115200:                                          /* Baudrate 115200 */
            cfsetispeed(&newtio, B115200);
            cfsetospeed(&newtio, B115200);
            break;
        case 230400:                                          /* Baudrate 230400 */
            cfsetispeed(&newtio, B230400);
            cfsetospeed(&newtio, B230400);
            break;

        default:
            close(handle);
            return FALSE;
    }

    /*
    * Set the bits.
    */
    switch (bits) {
        case 5:                                          
            newtio.c_cflag |= CS5;
            break;
        case 6:                                          
            newtio.c_cflag |= CS6;
            break;
        case 7:                                          
            newtio.c_cflag |= CS7;
            break;
        case 8:
            newtio.c_cflag |= CS8;
            break;
        default:
            close(handle);
            return FALSE;
    }

    /*
    * Set the parity (Odd Even None)
    */
    switch (parity) {

        case 'O':                                             
            newtio.c_cflag |= PARODD | PARENB;
            newtio.c_iflag |= INPCK;  
            break;

        case 'E':                                          
            newtio.c_cflag &= (~PARODD); 
            newtio.c_cflag |= PARENB;
            newtio.c_iflag |= INPCK;  
            break;

        case 'N': 
            break;

        default:
            close(handle);
            return FALSE;
    }

    /*
    * Setting Raw Input and Defaults
    */

    newtio.c_cflag |= CSTOPB;
    newtio.c_cflag |= CREAD|HUPCL|CLOCAL;
    newtio.c_iflag &= ~(IGNPAR|PARMRK|INLCR|IGNCR|ICRNL|ISTRIP);
    newtio.c_iflag |= BRKINT;  
//        newtio.c_lflag = ~(ICANON|ECHO);
    newtio.c_oflag  = 0; 
    newtio.c_lflag  = 0;

    // put in remark because they dont seem to work
    //newtio.c_cc[VMIN]  = 0; // wait for any number of bytes arriving (first byte)
    //newtio.c_cc[VTIME] = 100; // 10 seconds timeout


    /* Set the parameters */	
    if (tcsetattr(handle, TCSANOW, &newtio) < 0) {   
        close(handle);
        return FALSE;
    }

    /*
    * Wait while reader sends PNP information
    * (I'll catch this information in future
    *  versions if I get the information)
    */

    usleep(1000000);

    {
        int dtr_bit = TIOCM_DTR;
        int rts_bit = TIOCM_RTS;

        /* Set RTS low then do a DTR toggle to reset */
        ioctl (handle, TIOCMBIC, &rts_bit);
        usleep(100000); 
        ioctl (handle, TIOCMBIC, &dtr_bit);
        usleep(100000);
        ioctl (handle, TIOCMBIS, &dtr_bit);
        usleep(100000);
        ioctl (handle, TIOCMBIC, &dtr_bit);
        usleep(100000);
        ioctl (handle, TIOCMBIS, &dtr_bit);
        usleep(100000);
        ioctl (handle, TIOCMBIC, &dtr_bit);
        usleep(100000);
    }

    if (tcflush(handle, TCIOFLUSH) < 0) {
        close(handle);
        return FALSE;
    }

    sleep(1);

    /*
    * Record serial port settings
    */
    globalData->io.handle    = handle;                           
    globalData->io.baud      = baud;                            
    globalData->io.bits      = bits;                     
    globalData->io.stopbits  = 1;
    globalData->io.parity    = parity;
    globalData->io.blocktime = 1;

    return TRUE;
}

//*****************************************************************************
//
//*****************************************************************************
// returns the number of bytes available
int IO_Read(reader* globalData, unsigned long timeout, int len, unsigned char *buf ) {
  
    fd_set          rfds;
    struct timeval  tv;
    int             rval;
    int             readBytesNum = 0;
    HANDLE          handle;

#ifdef ASE_DEBUG
    int i;
#endif

    handle     = globalData->io.handle;

    tv.tv_sec  = timeout / 1000000;//globalData->io.blocktime;
    tv.tv_usec = timeout % 1000000;//0;

    FD_ZERO(&rfds);

    FD_SET(handle, &rfds);
    rval = select(handle+1, &rfds, NULL, NULL, &tv);

    if (rval == -1) {
#ifdef ASE_DEBUG
    	syslog(LOG_INFO, "IO_Read: Error! in initial select\n");
#endif
        return 0;
    }
    else if (rval == 0) {
#ifdef ASE_DEBUG
    	syslog(LOG_INFO, "IO_Read: Error! timeout (sec = %d usec = %d) in initial select\n", (int)tv.tv_sec, (int)tv.tv_usec);
#endif
        return 0;
    }

    if (FD_ISSET(handle, &rfds)) {
        
        rval = read(handle, buf, len);
        
        if (rval < 0) {
#ifdef ASE_DEBUG
            syslog(LOG_INFO, "IO_Read: Error! read() is negative (1)\n");
#endif
            return 0;
        }

#ifdef ASE_DEBUG
        for (i = 0 ; i < rval ; ++i) 
		    syslog(LOG_INFO, "<=== 0x%02x\n", buf[i + readBytesNum]);
#endif

        readBytesNum += rval;

        while (readBytesNum < len) {

            tv.tv_sec  = timeout / 1000000;//globalData->io.blocktime;
            tv.tv_usec = timeout % 1000000;//0;

            FD_ZERO(&rfds);

            FD_SET(handle, &rfds);
            rval = select(handle+1, &rfds, NULL, NULL, &tv);

            if (rval == -1) {
#ifdef ASE_DEBUG
    	        syslog(LOG_INFO, "IO_Read: Error! in secondary select\n");
#endif
                return 0;
            }
            else if (rval == 0) {
#ifdef ASE_DEBUG
    	        syslog(LOG_INFO, "IO_Read: Error! timeout (sec = %d usec = %d) in secondary select\n", (int)tv.tv_sec, (int)tv.tv_usec);
#endif
                return 0;
            }

            if (FD_ISSET(handle, &rfds)) {
                rval = read(handle, buf + readBytesNum, len - readBytesNum);

	            if (rval < 0) {
#ifdef ASE_DEBUG
    	            syslog(LOG_INFO, "IO_Read: Error! readBytesNum is negative (2)\n");
#endif
                    return 0;
	            }

#ifdef ASE_DEBUG
                for (i = 0 ; i < rval ; ++i) 
		            syslog(LOG_INFO, "<=== 0x%02x\n", buf[i + readBytesNum]);
#endif

                readBytesNum += rval;
            }
            else {
#ifdef ASE_DEBUG
    	        syslog(LOG_INFO, "IO_Read: Error! our handle not ready in secondary select\n");
#endif
                return 0;
            }
        }
    }
    else {
#ifdef ASE_DEBUG
    	syslog(LOG_INFO, "IO_Read: Error! our handle not ready in initial select\n");
#endif
        return 0;
    }

    if (readBytesNum < len) {
#ifdef ASE_DEBUG
    	syslog(LOG_INFO, "IO_Read: Error! not all bytes read\n");
#endif
    }

    return readBytesNum;

}


//*****************************************************************************
//
//*****************************************************************************
int IO_Write(reader* globalData, int writesize, unsigned char* data) {

    HANDLE handle = globalData->io.handle;
    unsigned int remain = writesize, chunk, offset = 0;
    int i;

    while (remain) {
        chunk = (remain > SSIZE_MAX ? SSIZE_MAX : remain);

        i = write(handle, data + offset, chunk);

        if (i < 0) {
#ifdef ASE_DEBUG
            syslog(LOG_INFO, "IO_Write: Error!\n");
#endif
            return offset;
        }

        offset += i;
        remain -= i;
    }

    return writesize;
}


//*****************************************************************************
//
//*****************************************************************************
bool IO_Close(reader* globalData) {

    HANDLE handle = globalData->io.handle;

    /* Close the handle */
    if ( close (handle) == 0 ) {   
        return TRUE;
    } 
    else {
        return FALSE;
    }

}


//*****************************************************************************
//
//*****************************************************************************
void  IO_CleanReadBuffer(reader* globalData) {
    unsigned char temp[700];

    IO_Read(globalData, 1000000 /* 1 sec */, 300, temp); 
}


//*****************************************************************************
//
//*****************************************************************************
bool IO_FlushBuffer(reader* globalData) {
  
    HANDLE handle = globalData->io.handle;

    if (tcflush(handle, TCIFLUSH) == 0)
        return TRUE;
    return FALSE;

}

//*****************************************************************************
//
//*****************************************************************************
char IO_UpdateParity(reader* globalData, char parity) {
 
  struct termios newtio;

  // Gets current settings
  tcgetattr(globalData->io.handle,&newtio);
 
  /*
   * Set the parity (Odd Even None)
   */     
  switch (parity) {
 
    /* Odd Parity */ 
    case 'O':                                
      newtio.c_cflag |= PARODD;
      break;
 
    /* Even Parity */
    case 'E':                                 
      newtio.c_cflag &= (~PARODD);
      break;
 
    /* No Parity */
    case 'N':                                 
      break;
  }

  /* Flush the serial port */
  if(tcflush(globalData->io.handle, TCIFLUSH) < 0) {          
    close(globalData->io.handle);
    return -1;
   }

  /* Set the parameters */
  if(tcsetattr(globalData->io.handle, TCSANOW, &newtio) < 0) { 
    close(globalData->io.handle);
    return -1;
  }

  /* Record the parity */
  globalData->io.parity = parity;
         
  return(globalData->io.parity);
}

//*****************************************************************************
//
//*****************************************************************************
int IO_UpdateStopBits(reader* globalData, int stopbits) {

  struct termios newtio;

  // Gets current settings
  tcgetattr(globalData->io.handle,&newtio);

  /*
   * Set number of stop bits (1,2)
   */
  switch (stopbits) {

    /* 2 stop bits */
    case 2:                                
      newtio.c_cflag |= CSTOPB;
      break;

    /* 1 stop bit */
    case '1': 
      newtio.c_cflag &= (~CSTOPB);
      break;
  }

  /* Flush the serial port */
  if(tcflush(globalData->io.handle, TCIFLUSH) < 0) {          
    close(globalData->io.handle);
    return -1;
  }

  /* Set the parameters */
  if(tcsetattr(globalData->io.handle, TCSANOW, &newtio) < 0) {
    close(globalData->io.handle);
    return -1;
  }

  /* Record the number of stop bits */
  globalData->io.stopbits = stopbits; 

  return(globalData->io.stopbits);
}

//*****************************************************************************
//
//*****************************************************************************
int IO_UpdateReturnBlock(reader* globalData, int blocktime) {             

  globalData->io.blocktime = (long)blocktime;            
  return globalData->io.blocktime;   
}

//*****************************************************************************
//
//*****************************************************************************
HANDLE IO_ReturnHandle(reader* globalData) {
  /* Return the current used handle */
  return globalData->io.handle;                           
}

//*****************************************************************************
//
//*****************************************************************************
int IO_ReturnBaudRate(reader* globalData) {
  /* Return the current baudrate */
  return globalData->io.baud;                              
}

//*****************************************************************************
//
//*****************************************************************************
char IO_ReturnParity(reader* globalData) {
  /* Return the current parity */
  return globalData->io.parity;                      
}
 
//*****************************************************************************
//
//*****************************************************************************
int IO_ReturnStopBits(reader* globalData) {
  /* Return the stop bits */
  return(globalData->io.stopbits);  
}

