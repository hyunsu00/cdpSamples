cmake_minimum_required(VERSION 3.10)
project(cdp-port)

set(CMAKE_CXX_STANDARD 11)

# mainBoostBeast
find_package(Boost 1.58.0 REQUIRED)
if(Boost_FOUND)
    message("Boost was found!")
    message("\${Boost_INCLUDE_DIRS} = ${Boost_INCLUDE_DIRS}")
    message("\${Boost_LIBRARIES} = ${Boost_LIBRARIES}")
    include_directories(${Boost_INCLUDE_DIRS})
    add_executable(mainBoostBeast mainBoostBeast.cpp)
    target_link_libraries(mainBoostBeast ${Boost_LIBRARIES} boost_system pthread) 
else()
    message(FATAL_ERROR "Boost was not found!")
endif()

# mainSocket
add_executable(mainSocket mainSocket.cpp)

# mainCUrl
find_package(CURL REQUIRED)
if(CURL_FOUND)
    message("CURL was found!")
    message("\${CURL_INCLUDE_DIRS} = ${CURL_INCLUDE_DIRS}")
    message("\${CURL_LIBRARIES} = ${CURL_LIBRARIES}")
    include_directories(${CURL_INCLUDE_DIRS})
    add_executable(mainCUrl mainCUrl.cpp)
    target_link_libraries(mainCUrl ${CURL_LIBRARIES}) 
else()
    message(FATAL_ERROR "CURL was not found!")
endif()

# mainLibWebSockets
find_package(PkgConfig REQUIRED)
pkg_check_modules(LWS REQUIRED libwebsockets)
if(LWS_FOUND)
    message("LWS was found!")
    message("\${LWS_INCLUDE_DIRS} = ${LWS_INCLUDE_DIRS}")
    message("\${LWS_LIBRARIES} = ${LWS_LIBRARIES}")
    include_directories(${LWS_INCLUDE_DIRS})
    add_executable(mainLibWebSockets mainLibWebSockets.cpp)
    target_link_libraries(mainLibWebSockets ${LWS_LIBRARIES}) 
else()
    message(FATAL_ERROR "LWS was not found!")
endif()

# add_executable(cdp-port _NoDepMain.cpp)
# target_link_libraries(cdp-port ssl crypto)

# add_executable(cdp-port libwebsockets.cpp)
# target_link_libraries(cdp-port ssl crypto)