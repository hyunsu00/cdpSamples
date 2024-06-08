cmake_minimum_required(VERSION 3.10)
project(cdp-port)

set(CMAKE_CXX_STANDARD 11)

add_executable(cdp-port boost-beast.cpp)

# add_executable(cdp-port _NoDepMain.cpp)
# target_link_libraries(cdp-port ssl crypto)

# add_executable(cdp-port libwebsockets.cpp)
# target_link_libraries(cdp-port ssl crypto)