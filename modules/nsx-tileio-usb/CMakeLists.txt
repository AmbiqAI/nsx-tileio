cmake_minimum_required(VERSION 3.20)

include(GNUInstallDirs)

if(NOT TARGET nsx::core)
    message(FATAL_ERROR "nsx-tileio-usb requires the nsx::core target.")
endif()
if(NOT TARGET nsx::usb)
    message(FATAL_ERROR "nsx-tileio-usb requires the nsx::usb target.")
endif()

add_library(nsx_tileio_usb STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/tio_usb.c
)
set_target_properties(nsx_tileio_usb PROPERTIES EXPORT_NAME tileio_usb)
add_library(nsx::tileio_usb ALIAS nsx_tileio_usb)

target_include_directories(nsx_tileio_usb
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-tileio-usb/includes-api>
)

target_link_libraries(nsx_tileio_usb
    PUBLIC
        nsx::core
        nsx::usb
)

target_compile_features(nsx_tileio_usb PUBLIC c_std_11)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-tileio-usb
)

install(TARGETS
    nsx_tileio_usb
    EXPORT nsxTargets
)
