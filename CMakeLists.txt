cmake_minimum_required(VERSION 3.9)
project(tg2sip)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(libtgvoip)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(Td 1.7.10 REQUIRED)
find_package(spdlog 0.17 REQUIRED)
pkg_check_modules(PJSIP libpjproject>=2.8 REQUIRED)
pkg_check_modules(OPUS opus REQUIRED)

add_executable(tg2sip
        tg2sip/main.cpp
        tg2sip/tg.cpp
        tg2sip/tg.h
        tg2sip/sip.cpp
        tg2sip/sip.h
        tg2sip/settings.cpp
        tg2sip/settings.h
        tg2sip/logging.cpp
        tg2sip/logging.h
        tg2sip/utils.cpp
        tg2sip/utils.h
        tg2sip/queue.h
        tg2sip/gateway.cpp
        tg2sip/gateway.h
        )

add_custom_command(
        TARGET tg2sip PRE_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/settings.ini
        $<TARGET_FILE_DIR:tg2sip>)

target_include_directories(tg2sip PRIVATE
        ${PJSIP_INCLUDE_DIRS}
        ${PROJECT_SOURCE_DIR}
        ${OPUS_INCLUDE_DIRS}
        include)

target_link_libraries(tg2sip PRIVATE
        ${PJSIP_LIBRARIES}
        ${OPUS_LIBRARIES}
        libtgvoip
        Threads::Threads
        Td::TdStatic)

add_executable(gen_db
        tg2sip/gen_db.cpp
        tg2sip/settings.cpp
        tg2sip/settings.h)

target_include_directories(gen_db PRIVATE
        include)

target_link_libraries(gen_db PRIVATE
        Td::TdStatic)