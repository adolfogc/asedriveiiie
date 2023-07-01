add_library(ASESerial SHARED)

target_sources(ASESerial PRIVATE
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

target_compile_options(ASESerial PRIVATE -DIFDHANDLERv2 -D_REENTRANT)

target_include_directories(ASESerial
    PUBLIC ${LIBPCSCLITE_INCLUDE_DIRS})

target_link_libraries(ASESerial
    Threads::Threads
    ${LIBPCSCLITE_LIBRARIES})