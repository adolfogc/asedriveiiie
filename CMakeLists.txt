cmake_minimum_required(VERSION 3.5)

project(asedriveiie C)

# Find dependencies

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)

find_package(Threads REQUIRED)

find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBUSB REQUIRED libusb)

pkg_check_modules(LIBPCSCLITE REQUIRED libpcsclite)

# Build USB and serial libraries

add_subdirectory(asedriveiiie-usb)

add_subdirectory(asedriveiiie-serial)