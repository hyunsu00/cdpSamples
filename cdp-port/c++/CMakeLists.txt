cmake_minimum_required(VERSION 3.10)
project(cdp-port)

set(CMAKE_CXX_STANDARD 11)

find_package(Boost 1.58.0 REQUIRED)
if(Boost_FOUND)
    message("Boost was found!")
    message("\${Boost_INCLUDE_DIRS} = ${Boost_INCLUDE_DIRS}")
    message("\${Boost_LIBRARIES} = ${Boost_LIBRARIES}")
    include_directories(${Boost_INCLUDE_DIRS})
    add_executable(boost-beast boost-beast.cpp)
    target_link_libraries(boost-beast ${Boost_LIBRARIES} boost_system pthread) 
else()
    message(FATAL_ERROR "Boost was not found!")
endif()

add_executable(NoDepMain NoDepMain.cpp websocket-parser-1.0.2/websocket_parser.c)

# add_executable(cdp-port _NoDepMain.cpp)
# target_link_libraries(cdp-port ssl crypto)

# add_executable(cdp-port libwebsockets.cpp)
# target_link_libraries(cdp-port ssl crypto)