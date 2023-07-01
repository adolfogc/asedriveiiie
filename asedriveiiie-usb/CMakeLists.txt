add_library(ASEDriveIIIe-USB SHARED)

target_sources(ASEDriveIIIe-USB PRIVATE
    Ase.h
    atr.h
    LinuxDefines.h
    MemoryCards.h
    T1Protocol.h
    atr.c
    CommandTypes.c
    DriverIO.c
    ifdhandler.c
    InitCardParams.c
    MemoryCards.c
    ReaderCommands.c
    T1Protocol.c
    usb.c
)

target_compile_options(ASEDriveIIIe-USB PRIVATE -DIFDHANDLERv2 -D_REENTRANT)

target_include_directories(ASEDriveIIIe-USB
    PUBLIC ${LIBUSB_INCLUDE_DIRS}
    PUBLIC ${LIBPCSCLITE_INCLUDE_DIRS})

target_link_libraries(ASEDriveIIIe-USB
    Threads::Threads
    ${LIBUSB_LIBRARIES}
    ${LIBPCSCLITE_LIBRARIES})