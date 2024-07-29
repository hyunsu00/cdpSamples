cmake_minimum_required(VERSION 3.10)
project(ConvertHtmlModule)

if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG -D_DEBUG)
endif()

# jsoncpp 라이브러리 찾기
find_package(jsoncpp REQUIRED)
if(jsoncpp_FOUND)
    message(STATUS "jsoncpp found")
    include_directories(${jsoncpp_INCLUDE_DIRS})
else()
    message(FATAL_ERROR "jsoncpp not found")
endif()

# 운영 체제에 따른 분기
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Configuring for Windows")

    # Windows-specific settings
    add_definitions(-DOS_WIN)
    set(SOURCES main.cpp ConvertHtmlModule.cpp)

elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Configuring for Linux")

    # Linux-specific settings
    add_definitions(-DOS_LINUX)
    set(SOURCES main.cpp ConvertHtmlModule.cpp)
    
else()
    message(FATAL_ERROR "Unsupported system: ${CMAKE_SYSTEM_NAME}")
endif()

add_executable(ConvertHtmlModule ${SOURCES})

# 타겟에 jsoncpp 라이브러리 링크
target_link_libraries(${PROJECT_NAME} jsoncpp_lib)