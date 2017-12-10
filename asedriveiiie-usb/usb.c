/*****************************************************************
/
/ File   :   usb.c
/ Date   :   April 9, 2001
/ Purpose:   This provides reader specific low-level calls
/            for the ASE-III (USB) reader of Athena Smartcard Solutions. 
/ License:   See file LICENSE
/
******************************************************************/

#include <stdio.h>
#include <string.h> 
#include <errno.h>
#include "Ase.h"


/*****************************************************************************
*
*****************************************************************************/
#define USB_INEP    0
#define USB_OUTEP   1 /* ok */

#define USB_TIMEOUT 60000	/* 1 minute timeout */

#define MAX_USB_RESPONSE 300 /* the maximal size of any response coming from the reader */

/*****************************************************************************
*
*****************************************************************************/
typedef struct {
	int kMyVendorID;
	int kMyProductID;
} _usbID;

static _usbID UsbIDs[] = {
    { 0x0DC3, 0x0802 }, 
    { 0x0DC3, 0x1104 }, 
};


/*****************************************************************************
*
*****************************************************************************/
bool OpenUSB(reader* allReaders, reader* globalData)
{
    static struct usb_bus*  busses = NULL;
    int                     id, id_number;
    struct usb_bus*         bus;
    struct usb_dev_handle*  dev_handle;
    
#ifdef ASE_DEBUG
    int k;
    syslog(LOG_INFO, "OpenUSB: ");
#endif
    
    if (busses == NULL)	{
	usb_init();
    } 
    
    usb_find_busses();
    usb_find_devices();
    
    busses = usb_get_busses();
    
    if (busses == NULL)	{
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "No USB busses found");
#endif
	return FALSE;
    }
    
    if (globalData->io.handle != NULL) {
#ifdef ASE_DEBUG
        syslog(LOG_INFO, "USB driver in use");
#endif
	return FALSE;
    }
    
    /* find any device corresponding to a UsbIDs entry */
    id_number = sizeof(UsbIDs)/sizeof(UsbIDs[0]);
    
    /* for any supported reader */
    for (id = 0; id < id_number ; id++)	{
	/* on any USB buses */
        for (bus = busses; bus; bus = bus->next) {
	    struct usb_device *dev;
	    
	    /* any device on this bus */
	    for (dev = bus->devices; dev; dev = dev->next)
		{
		    if (dev->descriptor.idVendor == UsbIDs[id].kMyVendorID
			&& dev->descriptor.idProduct == UsbIDs[id].kMyProductID) {
			int r, already_used;
			char bus_device[BUS_DEVICE_STRSIZE];
			
			if (snprintf(bus_device, BUS_DEVICE_STRSIZE, "%s/%s",
				     bus->dirname, dev->filename) < 0) {
#ifdef ASE_DEBUG
			    syslog(LOG_INFO, "Device name too long: %s", bus_device);
#endif
			    return FALSE;
			}
			
			
			already_used = FALSE;
   		    for (r = 0; r < MAX_READER_NUM ; r++) {
#ifdef ASE_DEBUG
			  syslog(LOG_INFO, "Checking new device '%s' against old '%s'", bus_device, allReaders[r].io.bus_device);
#endif
			  if (strcmp(allReaders[r].io.bus_device, bus_device) == 0)
			      already_used = TRUE;
			}
			if (!already_used) {
			    dev_handle = usb_open(dev);
			    if (dev_handle)	{
				int interface;
				int retVal;
				int interfaceNum = 0;

#ifdef ASE_DEBUG
				syslog(LOG_INFO, "Trying to open USB bus/device: %s", bus_device);
#endif
				if (dev->config == NULL) {
#ifdef ASE_DEBUG
				    syslog(LOG_INFO, "No dev->config found for %s", bus_device);
#endif
				    return FALSE;
				}
				
				interface = dev->config[0].interface[0].altsetting[0].bInterfaceNumber;
				retVal = usb_claim_interface(dev_handle, interface);
				if ((retVal < 0) && (EPERM == errno))
				{
					usb_close(dev_handle);
					return FALSE;
				}
				if (retVal < 0 || dev->config[0].interface[0].altsetting[0].bNumEndpoints != 2)	{
#ifdef ASE_DEBUG
				    syslog(LOG_INFO, "Can't claim interface 0 %s: %s (or not the correct one (KB))", globalData->io.bus_device, strerror(errno));
#endif
					if (retVal == 0)
						usb_release_interface(dev_handle, interface);

					// try the second interface
					interface = dev->config[0].interface[1].altsetting[0].bInterfaceNumber;

					if (usb_claim_interface(dev_handle, interface) < 0)	{
#ifdef ASE_DEBUG
					    syslog(LOG_INFO, "Can't claim interface 1 %s: %s", globalData->io.bus_device, strerror(errno));
#endif
					    return FALSE;
					}

#ifdef ASE_DEBUG
				    syslog(LOG_INFO, "Using interface 1 (dev->config[0].interface[1].altsetting[0].bInterfaceNumber = %d)", dev->config[0].interface[1].altsetting[0].bInterfaceNumber);
#endif
					interfaceNum = 1;
				}
				
#ifdef ASE_DEBUG
				syslog(LOG_INFO, "Using USB bus/device: %s", bus_device);

				for (k = 0 ; k < dev->config[0].interface[interfaceNum].altsetting[0].bNumEndpoints ; k++)
                    syslog(LOG_INFO, "ep[%d] -> bDescriptorType = %X bEndpointAddress = %X bmAttributes = %X wMaxPacketSize = %X bLength = %x\n", 
                           k, dev->config[0].interface[interfaceNum].altsetting[0].endpoint[k].bDescriptorType,
                           dev->config[0].interface[interfaceNum].altsetting[0].endpoint[k].bEndpointAddress,
                           dev->config[0].interface[interfaceNum].altsetting[0].endpoint[k].bmAttributes,
                           dev->config[0].interface[interfaceNum].altsetting[0].endpoint[k].bmAttributes,
			   dev->config[0].interface[interfaceNum].altsetting[0].endpoint[k].bLength);

#endif
				
				/* store device information */
                globalData->io.curPos = globalData->io.lastPos = globalData->io.stopReading = 0;
				globalData->io.handle = dev_handle;
				globalData->io.dev = dev;
				strncpy(globalData->io.bus_device, bus_device, BUS_DEVICE_STRSIZE);
				
				globalData->io.interface = interface;
				globalData->io.bulk_in = globalData->io.dev->config[0].interface[interfaceNum].altsetting[0].endpoint[USB_INEP].bEndpointAddress;
				globalData->io.bulk_out = globalData->io.dev->config[0].interface[interfaceNum].altsetting[0].endpoint[USB_OUTEP].bEndpointAddress;

#ifdef ASE_DEBUG
				syslog(LOG_INFO, "globalData->io.bulk_in = 0x%x globalData->io.bulk_out = 0x%x", globalData->io.bulk_in, globalData->io.bulk_out);
#endif

				return TRUE;
			    }
			    else {
#ifdef ASE_DEBUG
				syslog(LOG_INFO, "Can't usb_open(%s): %s", bus_device, strerror(errno));
#endif
			    }
			}
			else {
#ifdef ASE_DEBUG
			    syslog(LOG_INFO, "USB device %s already in use. Checking next one.",	bus_device);
#endif
			}
		    }
		}
	}
    }
    
    if (globalData->io.handle == NULL)
	return FALSE;
    
    return TRUE;
    
} 


/*****************************************************************************
*
*****************************************************************************/
int WriteUSB(reader* globalData, int writesize, unsigned char* data)
{
    int i, remain = writesize;

    i = usb_bulk_write(globalData->io.handle, 
		       globalData->io.bulk_out, 
		       (char*)data, remain, USB_TIMEOUT);

    if (i < 0) {
#ifdef ASE_DEBUG
	syslog(LOG_INFO, "WriteUSB: Error! (i = %d)\n", i);
#endif
	return 0;
    }

    return writesize;
} 


/*****************************************************************************
*
*****************************************************************************/
int ReadUSB(reader* globalData, unsigned long timeout, int len, unsigned char *buf)
{
    unsigned int	saveLast = globalData->io.lastPos;
    unsigned int    remain=len;
    int	ret = 0, count, rval, k;
//    unsigned long timeoutChunk = 100000; // 0.1 sec
    unsigned char temp[MAX_USB_RESPONSE];


    saveLast = globalData->io.lastPos;

    // no data available - try reading from reader
    if (globalData->io.curPos == saveLast) {
        // read at most RESPONSE_BUFFER_SIZE bytes
        rval = usb_bulk_read(globalData->io.handle, 
                             globalData->io.bulk_in, 
                             (char*)temp, MAX_USB_RESPONSE, timeout);

        // dealing with the zero data frame - resend the request if no data arrived
        if (rval <= 0)
            rval = usb_bulk_read(globalData->io.handle, 
                                 globalData->io.bulk_in, 
                                 (char*)temp, MAX_USB_RESPONSE, timeout);


        if (rval > 0) {
            // copy the data read to the buffer 
            saveLast = globalData->io.lastPos;

            for (k = 0 ; k < rval ; ++k) {        
                globalData->io.response[saveLast++] = temp[k];
                saveLast %= RESPONSE_BUFFER_SIZE;
#ifdef ASE_DEBUG
    	        syslog(LOG_INFO, "<=== 0x%02x\n", temp[k]);
#endif
            }

            globalData->io.lastPos = saveLast;
        }
    }

    saveLast = globalData->io.lastPos;

    // some data read is waiting to be returned
    if (globalData->io.curPos != saveLast) {
        if (globalData->io.curPos < saveLast) {
            count = (saveLast - globalData->io.curPos) > remain ? remain : (saveLast - globalData->io.curPos);
            memcpy((unsigned char*)buf, &(globalData->io.response[globalData->io.curPos]), count);
            ret += count;
            remain -= count;
            globalData->io.curPos += count;
            globalData->io.curPos %= RESPONSE_BUFFER_SIZE;
        }
        else {
            count = (RESPONSE_BUFFER_SIZE - globalData->io.curPos) > remain ? 
                    remain : (RESPONSE_BUFFER_SIZE - globalData->io.curPos);
            memcpy((unsigned char*)buf, &(globalData->io.response[globalData->io.curPos]), count);
		    globalData->io.curPos += count;
            globalData->io.curPos %= RESPONSE_BUFFER_SIZE;
		    ret += count;
            remain -= count;
            if (remain > 0) { // more bytes to copy (len-ret at most) 
                count = (saveLast < remain) ? saveLast : remain;
                if (count) {
                    memcpy((unsigned char*)(&buf[ret]), globalData->io.response, count);
                    ret += count;
                    remain -= count;
                }
                globalData->io.curPos = count;
                globalData->io.curPos %= RESPONSE_BUFFER_SIZE;
            }
        }
    }

#ifdef ASE_DEBUG
    syslog(LOG_INFO, "ReadUSB: ret = %d length = %d timeout = %d\n", ret, len, (int)timeout);
#endif

    return ret;
} 

//*****************************************************************************
//
//*****************************************************************************
void  CleanReadBufferUSB(reader* globalData) {
//    unsigned char temp[700];
//
//    usb_bulk_read(globalData->io.handle, 
//                  globalData->io.dev->config[0].interface[0].altsetting[0].endpoint[USB_INEP].bEndpointAddress, 
//                  temp, 300, 1000 /* 1 sec */);  

    globalData->io.curPos = globalData->io.lastPos = 0;
}


/*****************************************************************************
*
*****************************************************************************/
bool CloseUSB(reader* globalData)
{
#ifdef ASE_DEBUG
    syslog(LOG_INFO, "CloseUSB: device = %s", globalData->io.bus_device);
#endif

	usb_release_interface(globalData->io.handle, globalData->io.interface);
	usb_reset(globalData->io.handle);
	usb_close(globalData->io.handle);

	/* mark the resource unused */
	globalData->io.handle = NULL;
	globalData->io.dev = NULL;
	globalData->io.bus_device[0] = '\0';
    globalData->io.stopReading = 1;

	return TRUE;
} 

