cmake_minimum_required(VERSION 3.20)

include(GNUInstallDirs)

if(NOT TARGET nsx::core)
    message(FATAL_ERROR "nsx-tileio-ble requires the nsx::core target.")
endif()
if(NOT TARGET nsx::ble)
    message(FATAL_ERROR "nsx-tileio-ble requires the nsx::ble target.")
endif()

add_library(nsx_tileio_ble STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/tio_ble.c
)
set_target_properties(nsx_tileio_ble PROPERTIES EXPORT_NAME tileio_ble)
add_library(nsx::tileio_ble ALIAS nsx_tileio_ble)

target_include_directories(nsx_tileio_ble
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-tileio-ble/includes-api>
)

target_link_libraries(nsx_tileio_ble
    PUBLIC
        nsx::core
        nsx::ble
)

target_compile_features(nsx_tileio_ble PUBLIC c_std_11)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-tileio-ble
)

install(TARGETS
    nsx_tileio_ble
    EXPORT nsxTargets
)
