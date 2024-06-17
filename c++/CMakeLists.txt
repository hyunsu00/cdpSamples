cmake_minimum_required(VERSION 3.10)
project(mainPipeConnection)

# mainPipeConnection
find_package(Boost 1.58.0 REQUIRED)
if(Boost_FOUND)
    message("Boost was found!")
    message("\${Boost_INCLUDE_DIRS} = ${Boost_INCLUDE_DIRS}")
    message("\${Boost_LIBRARIES} = ${Boost_LIBRARIES}")
    include_directories(${Boost_INCLUDE_DIRS})
    add_executable(mainPipeConnection mainPipeConnection.cpp)
    # target_link_libraries(mainPipeConnection ${Boost_LIBRARIES} boost_system boost_filesystem boost_process) 
else()
    message(FATAL_ERROR "Boost was not found!")
endif()

