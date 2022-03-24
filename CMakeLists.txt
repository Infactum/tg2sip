# This file was written from scratch by Nicholas Guriev for the libtgvoip package.
# You may use this code as you wish under the terms of UNLICENSE, see https://unlicense.org/

cmake_minimum_required(VERSION 3.16)
project(libtgvoip)
include(GNUInstallDirs)
set(DESKTOP_APP_USE_PACKAGED TRUE)

include(cmake/validate_special_target.cmake)
include(cmake/init_target.cmake)
include(cmake/nice_target_sources.cmake)
include(cmake/options.cmake)
add_subdirectory(cmake/external/opus)
add_subdirectory(cmake/external/webrtc)

set(DESKTOP_APP_USE_PACKAGED FALSE)
set(TGVOIP_FOUND FALSE)
set(tgvoip_loc "${CMAKE_SOURCE_DIR}")
include(lib_tgvoip.cmake)
set_target_properties(lib_tgvoip_bundled PROPERTIES OUTPUT_NAME tgvoip PUBLIC_HEADER TgVoip.h)
install(TARGETS lib_tgvoip_bundled PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/tgvoip")

set(LIBTGVOIP_VERSION 2.4.4)
configure_file(tgvoip.pc.in tgvoip.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/tgvoip.pc DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
