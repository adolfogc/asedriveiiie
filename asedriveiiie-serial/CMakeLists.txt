add_library(ASEDriveIIIe-Serial SHARED)
target_sources(ASEDriveIIIe-Serial PRIVATE
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
    serial.c
    T1Protocol.c
)

target_compile_options(ASEDriveIIIe-Serial PRIVATE -DIFDHANDLERv2 -D_REENTRANT)

target_include_directories(ASEDriveIIIe-Serial
    PUBLIC ${LIBPCSCLITE_INCLUDE_DIRS}
)

target_link_libraries(ASEDriveIIIe-Serial
    Threads::Threads
    ${LIBPCSCLITE_LIBRARIES}
)

set(SERIAL_BUNDLE ifd-ASEDriveIIIe-Serial)

# Set driver directory based on platform
if(APPLE)
    #set(SERIAL_DRIVER_DIR /usr/libexec/SmartCardServices/drivers)
    set(SERIAL_DRIVER_DIR /usr/local/lib/pcsc/drivers)
    set(BUNDLE_SUBDIR Contents/MacOS)
else()
    set(SERIAL_DRIVER_DIR /usr/local/pcsc/drivers)
endif()

install(TARGETS ASEDriveIIIe-Serial LIBRARY
    DESTINATION ${SERIAL_DRIVER_DIR}/${SERIAL_BUNDLE}
    PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ
)
