cmake_minimum_required(VERSION 3.20)

project(nsx_tileio LANGUAGES C)

include(GNUInstallDirs)

add_subdirectory(modules/nsx-tileio-usb)
add_subdirectory(modules/nsx-tileio-ble)
