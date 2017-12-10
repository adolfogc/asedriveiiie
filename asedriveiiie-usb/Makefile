
include Makefile.inc

DRIVER_DIR=${DESTDIR}/${USBDROPDIR}/ifd-ASEDriveIIIe-USB.bundle

CC=${BUILD}-gcc

SOURCES=usb.c atr.c DriverIO.c CommandTypes.c ReaderCommands.c T1Protocol.c MemoryCards.c InitCardParams.c ifdhandler.c

all:	libASEDriveIIIe-USB.so

libASEDriveIIIe-USB.so: ${SOURCES}
	${CC} -o libASEDriveIIIe-USB.so ${SOURCES} -fPIC -D_REENTRANT -DIFDHANDLERv2 -Wall -I. ${CFLAGS} ${LDFLAGS} -lusb -shared

clean-all:	clean
	rm Makefile.inc || true

clean:
	rm -f *~ *.o *.so || true

install:	all
	install -c -d "${DRIVER_DIR}/Contents"
	install -c -m 0644 ifd-ASEDriveIIIe-USB.bundle/Contents/Info.plist ifd-ASEDriveIIIe-USB.bundle/Contents/PkgInfo "${DRIVER_DIR}/Contents"
	install -c -d "${DRIVER_DIR}/Contents/Resources/English.lproj"
	install -c -m 0644 ifd-ASEDriveIIIe-USB.bundle/Contents/Resources/English.lproj/InfoPlist.strings "${DRIVER_DIR}/Contents/Resources/English.lproj"
	install -c -d "${DRIVER_DIR}/Contents/Linux"
	install -c -m 0755 libASEDriveIIIe-USB.so "${DRIVER_DIR}/Contents/Linux"
	install -c -m 0644 50-pcscd-asedriveiiie.rules /etc/udev/rules.d/.
