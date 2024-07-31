cmake_minimum_required(VERSION 3.10)
project(ConvertHtmlModule)

message(STATUS "-------------------------")
# 삭제 작업을 수행하는 명령 추가
message(STATUS "Starting to remove chrome directory...")
execute_process(
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/chrome
  RESULT_VARIABLE remove_result
)
if(NOT ${remove_result} EQUAL 0)
  message(WARNING "Failed to remove directory using CMake command. Trying alternative method.")
  
  if(WIN32)
    execute_process(
      COMMAND cmd /c if exist "${CMAKE_BINARY_DIR}/chrome" rmdir /s /q "${CMAKE_BINARY_DIR}/chrome"
      RESULT_VARIABLE remove_result
    )
  else()
    execute_process(
      COMMAND sh -c "rm -rf '${CMAKE_BINARY_DIR}/chrome'"
      RESULT_VARIABLE remove_result
    )
  endif()
  
  if(NOT ${remove_result} EQUAL 0)
    message(FATAL_ERROR "Failed to remove chrome directory: ${remove_result}")
  else()
    message(STATUS "Successfully remove chrome directory.")
  endif()
else()
  message(STATUS "Successfully remove chrome directory.")
endif()

# 복사 작업을 수행하는 명령 추가
message(STATUS "Starting to copy chrome directory...")
execute_process(
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/chrome ${CMAKE_BINARY_DIR}/chrome
  RESULT_VARIABLE copy_result
)
if(NOT ${copy_result} EQUAL 0)
  message(FATAL_ERROR "Failed to copy chrome directory: ${copy_result}")
else()
  message(STATUS "Successfully copied chrome directory.")
endif()
message(STATUS "-------------------------")

if(CMAKE_BUILD_TYPE MATCHES Debug)
    add_definitions(-DDEBUG -D_DEBUG)
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

# 실행 파일 생성
add_executable(ConvertHtmlModule ${SOURCES})
# 실행 파일을 설치하도록 지정
install(TARGETS ConvertHtmlModule DESTINATION bin)
# chrome 폴더를 설치하도록 지정
install(DIRECTORY ${CMAKE_BINARY_DIR}/chrome DESTINATION bin)

# CPack 설정
set(CPACK_PACKAGE_NAME "ConvertHtmlModule")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_CONTACT "Kim HyunSu <hyun955807@naver.com>")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Kim HyunSu <hyun955807@naver.com>") # Debian 패키지에 필요
set(CPACK_PACKAGE_DESCRIPTION "A module to convert HTML")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A module to convert HTML")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/License.txt")

# CPACK_GENERATOR 설정
set(CPACK_GENERATOR "TGZ")
if(WIN32)
  # set(CPACK_GENERATOR "ZIP")
  set(CPACK_GENERATOR "NSIS")
endif()

# CPack을 포함
include(CPack)