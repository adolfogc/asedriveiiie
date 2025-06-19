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
    PUBLIC ${LIBPCSCLITE_INCLUDE_DIRS}
)

if(APPLE)
    target_link_libraries(ASEDriveIIIe-USB
        Threads::Threads
        ${LIBUSB_LIBRARIES}
    )
    target_link_libraries(ASEDriveIIIe-USB ${PCSC_LIBRARIES})

    if(LIBUSB_LIBRARY_DIRS)
        target_link_directories(ASEDriveIIIe-USB PRIVATE ${LIBUSB_LIBRARY_DIRS})
    endif()
else()
    target_link_libraries(ASEDriveIIIe-USB
        Threads::Threads
        ${LIBUSB_LIBRARIES}
        ${LIBPCSCLITE_LIBRARIES}
    )
endif()

set(USB_BUNDLE ifd-ASEDriveIIIe-USB.bundle)

# Set driver directory based on platform
if(APPLE)
    #set(USB_DRIVER_DIR /usr/libexec/SmartCardServices/drivers)
    set(USB_DRIVER_DIR /usr/local/lib/pcsc/drivers)
    set(BUNDLE_SUBDIR Contents/MacOS)
else()
    set(USB_DRIVER_DIR /usr/local/pcsc/drivers)
    set(BUNDLE_SUBDIR Contents/Linux)
endif()

add_custom_command(TARGET ASEDriveIIIe-USB POST_BUILD
    COMMAND ${CMAKE_COMMAND} ARGS -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/${USB_BUNDLE} ${CMAKE_CURRENT_BINARY_DIR}/${USB_BUNDLE}
    COMMAND ${CMAKE_COMMAND} ARGS -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/${USB_BUNDLE}/${BUNDLE_SUBDIR}
    COMMAND ${CMAKE_COMMAND} ARGS -E copy $<TARGET_FILE:ASEDriveIIIe-USB> ${CMAKE_CURRENT_BINARY_DIR}/${USB_BUNDLE}/${BUNDLE_SUBDIR}/$<TARGET_FILE_NAME:ASEDriveIIIe-USB>
)

install(DIRECTORY ${USB_BUNDLE}
    DESTINATION ${USB_DRIVER_DIR}
    FILE_PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
    DIRECTORY_PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
    PATTERN */${BUNDLE_SUBDIR}/*.so
    PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ
)

# Only install udev rules on Linux
if(NOT APPLE)
    install(FILES 50-pcscd-asedriveiiie.rules
        DESTINATION /etc/udev/rules.d/
        PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ
    )
endif()
