cmake_minimum_required(VERSION 3.10)
project(mainPipeConnection)

LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_PROJECT_DIR}/cmake")

# mainPipeConnection
# find_package(Boost 1.58.0 REQUIRED)
# if(Boost_FOUND)
#     message("Boost was found!")
#     message("\${Boost_INCLUDE_DIRS} = ${Boost_INCLUDE_DIRS}")
#     message("\${Boost_LIBRARIES} = ${Boost_LIBRARIES}")
#     include_directories(${Boost_INCLUDE_DIRS})
#     add_executable(mainPipeConnection mainPipeConnection.cpp)
#     # target_link_libraries(mainPipeConnection ${Boost_LIBRARIES} boost_system boost_filesystem boost_process) 
# else()
#     message(FATAL_ERROR "Boost was not found!")
# endif()

find_package(PkgConfig REQUIRED)
# Handle case where libuv is not found
if(NOT PkgConfig_FOUND)
    message(FATAL_ERROR "PkgConfig not found, cannot proceed.")
endif()
pkg_check_modules(LIBUV REQUIRED IMPORTED_TARGET libuv)
if(LIBUV_FOUND)
    add_executable(mainPipeConnection mainPipeConnection.cpp)
    target_include_directories(mainPipeConnection PRIVATE ${LIBUV_INCLUDE_DIRS})
    target_link_libraries(mainPipeConnection PRIVATE PkgConfig::LIBUV) 
else()
    message(FATAL_ERROR "libuv was not found!")
endif()