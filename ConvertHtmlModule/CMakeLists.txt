cmake_minimum_required(VERSION 3.10)
project(ConvertHtmlModule)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG)
    add_definitions(-D_DEBUG)
endif()

# 운영 체제에 따른 분기
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Configuring for Windows")

    # Windows-specific settings
    add_definitions(-DWINDOWS)
    set(SOURCES main.cpp ConvertHtmlModule.cpp)

elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Configuring for Linux")

    # Linux-specific settings
    add_definitions(-DLINUX)
    set(SOURCES main.cpp ConvertHtmlModule.cpp)
    
else()
    message(FATAL_ERROR "Unsupported system: ${CMAKE_SYSTEM_NAME}")
endif()

add_executable(ConvertHtmlModule ${SOURCES})