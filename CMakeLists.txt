# CMakeList.txt : CMake project for main, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project ("ulog_sqlite")

#IF(CMAKE_SYSTEM_NAME MATCHES Darwin)
#    set(CMAKE_C_FLAGS "-DCOCOA=1")
#    link_libraries("-framework WebKit -framework AppKit")
#elseif (CMAKE_SYSTEM_NAME MATCHES Windows)
#    add_definitions(-DWINAPI=1)
#ENDIF(CMAKE_SYSTEM_NAME MATCHES Darwin)

include_directories("src")

set(SOURCES "src/test_ulog_sqlite.c")
set(SOURCES ${SOURCES} "src/ulog_sqlite.c")
set(SOURCES ${SOURCES} "src/ulog_sqlite.h")

# Add source to this project's executable.
add_executable (ulog_sqlite ${SOURCES})

# TODO: Add tests and install targets if needed.
