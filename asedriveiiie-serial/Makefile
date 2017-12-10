
include Makefile.inc

DRIVER_DIR=${DESTDIR}/${USBDROPDIR}/ifd-ASEDriveIIIe-Serial

CC=${BUILD}-gcc

SOURCES=serial.c atr.c DriverIO.c CommandTypes.c ReaderCommands.c T1Protocol.c MemoryCards.c InitCardParams.c ifdhandler.c

all:	libASESerial.so etc

libASESerial.so: ${SOURCES}
	${CC} -o libASESerial.so ${SOURCES} -fPIC -D_REENTRANT -DIFDHANDLERv2 -Wall -I. ${CFLAGS} ${LDFLAGS} -shared

etc:	etc/reader.conf

etc/reader.conf:	etc/reader.conf.in
	sed "s#\%PCSC_DRIVERS_DIR\%#${USBDROPDIR}#g" < etc/reader.conf.in > etc/reader.conf

clean-all:	clean
	rm Makefile.inc || true

clean:
	rm -f *~ *.o *.so || true
	rm etc/reader.conf || true

install:	all
	install -c -d "${DRIVER_DIR}"
	install -c -m 0755 libASESerial.so "${DRIVER_DIR}"
