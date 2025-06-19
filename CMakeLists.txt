cmake_minimum_required(VERSION 3.5)

project(asedriveiie C)

# Find dependencies

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)

find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)

if(APPLE)
    set(ENV{PKG_CONFIG_PATH} "/opt/homebrew/lib/pkgconfig:/usr/local/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
endif()

if(APPLE)
    pkg_check_modules(LIBUSB REQUIRED libusb)
    message(STATUS "LIBUSB_LIBRARIES: ${LIBUSB_LIBRARIES}")
    message(STATUS "LIBUSB_LIBRARY_DIRS: ${LIBUSB_LIBRARY_DIRS}")

    find_library(PCSC_LIBRARIES PCSC)
    if(PCSC_LIBRARIES)
        set(LIBPCSCLITE_FOUND TRUE)
        set(LIBPCSCLITE_LIBRARIES ${PCSC_LIBRARIES})

        #set(LIBPCSCLITE_INCLUDE_DIRS "${PCSC_LIBRARIES}/Headers")
        set(LIBPCSCLITE_INCLUDE_DIRS "/opt/homebrew/opt/pcsc-lite/include/PCSC")   
        message(STATUS "PCSC Framework: ${PCSC_LIBRARIES}")
        message(STATUS "PCSC Headers: ${LIBPCSCLITE_INCLUDE_DIRS}")
    else()
        message(FATAL_ERROR "PCSC framework not found")
    endif()
else()
    pkg_check_modules(LIBUSB REQUIRED libusb)
    pkg_check_modules(LIBPCSCLITE REQUIRED libpcsclite)
endif()

# Build USB and serial libraries

add_subdirectory(asedriveiiie-usb)

add_subdirectory(asedriveiiie-serial)
