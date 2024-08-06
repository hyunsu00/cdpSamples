cmake_minimum_required(VERSION 3.10)
project(ConvertHtmlModule)

message(STATUS "-------------------------")
# 삭제 작업을 수행하는 명령 추가
message(STATUS "Starting to remove chrome directory...")
execute_process(
  COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}/chrome
  RESULT_VARIABLE remove_result
)
if(NOT ${remove_result} EQUAL 0)
  message(WARNING "Failed to remove directory using CMake command. Trying alternative method.")
  
  if(WIN32)
    execute_process(
      COMMAND cmd /c if exist "${CMAKE_CURRENT_BINARY_DIR}/chrome" rmdir /s /q "${CMAKE_CURRENT_BINARY_DIRCMAKE_CURRENT_BINARY_DIR}/chrome"
      RESULT_VARIABLE remove_result
    )
  else()
    execute_process(
      COMMAND sh -c "rm -rf '${CMAKE_CURRENT_BINARY_DIR}/chrome'"
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
if(WIN32)
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/chrome/chrome-headless-shell-win32 ${CMAKE_CURRENT_BINARY_DIR}/chrome/chrome-headless-shell-win32
    RESULT_VARIABLE copy_result
  )
else()
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/chrome/chrome-headless-shell-linux64 ${CMAKE_CURRENT_BINARY_DIR}/chrome/chrome-headless-shell-linux64
    RESULT_VARIABLE copy_result
  )
endif()
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
add_executable(${PROJECT_NAME} ${SOURCES})
# 실행 파일을 설치하도록 지정
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
# .debug 파일을 생성하고 실행 파일과 함께 설치하도록 지정
install(CODE "
    # execute_process 커맨드는 COMMAND를 병렬 수행하므로 아래 커맨드를
    # 따로따로 execute_process 커맨드로 실행해야 한다.
    # objcopy에 --only-keep-debug 옵션을 사용하지 않고 그냥 cp한다.
    # 파일 크기가 많이 차이나지 않으므로 원본 파일을 저장해두면 활용도가
    # 높을 것이다.
    execute_process(
        COMMAND cp \"${PROJECT_NAME}\" \"${PROJECT_NAME}.debug\"
        WORKING_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/bin\"
    )
    execute_process(
        COMMAND strip --strip-unneeded --remove-section=.comment --remove-section=.note  \"${PROJECT_NAME}\"
        WORKING_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/bin\"
    )
    execute_process(
        COMMAND objcopy \"--add-gnu-debuglink=${PROJECT_NAME}.debug\" \"${PROJECT_NAME}\"
        WORKING_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/bin\"
    )
")
# chrome 폴더를 설치하도록 지정
if(WIN32)
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/chrome/chrome-headless-shell-win32 DESTINATION bin/chrome USE_SOURCE_PERMISSIONS)
else()
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/chrome/chrome-headless-shell-linux64 DESTINATION bin/chrome USE_SOURCE_PERMISSIONS)
endif()

# CPack 설정
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
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

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    # /etc/os-release 파일을 확인하여 distribution 이름을 설정한다.
    # 패키지 네임에 이를 추가하여 패키지 파일 이름만 가지고도 어떤 배포판용인지
    # 확인을 쉽게 할 수 있도록 한다.
    execute_process(
        COMMAND bash -c "(eval cat /etc/os-release; case $NAME in *CentOS*) echo centos ;; *Debian*) echo debian ;; *Fedora*) echo fedora ;; *Ubuntu*) echo ubuntu ;; *Amazon*) echo amazon ;; *Rocky*) echo rockylinux ;; *) echo unknown ;; esac)"
        OUTPUT_VARIABLE DISTRO_NAME
    )
    message(STATUS "Distribution name: ${DISTRO_NAME}")

    # string(STRIP ${DISTRO_NAME} DISTRO_NAME)
    # set(OS_NAME ${CPACK_SYSTEM_NAME})
    # string(TOLOWER ${OS_NAME} OS_NAME)

    # set(package_file_name "${CPACK_PACKAGE_NAME}${hwp_PRODUCT_NAME}-${CPACK_PACKAGE_VERSION}-${OS_NAME}-${DISTRO_NAME}.zip")
    # set(debug_package_file_name "${CPACK_PACKAGE_NAME}${hwp_PRODUCT_NAME}-debug-${CPACK_PACKAGE_VERSION}-${OS_NAME}-${DISTRO_NAME}.zip")
    # add_custom_target(package_with_debug
    #     # package 타겟은 add_dependencies로 제대로된 dependecy가 걸리지 않는다.
    #     # 그래서 직접 package 빌드 커맨드를 실행한후 debug 파일을 분리한다.
    #     #
    #     # make의 package 타겟을 사용하면 편리하지만, 압축에 시간이 많이 걸리는 것 같다.
    #     # 패키징에만 40분 정도 소요된다.
    #     # 그래서 임시 디렉토리에 install한 후 직접 7z 커맨드로 압축하는 룰을 작성한다.
    #     # 기존 package 타겟을 사용하려면 아래 커맨드를 사용하라.
    #     # COMMAND ${CMAKE_COMMAND} --build "${CMAKE_CURRENT_BINARY_DIR}" -- package
    #     COMMAND ${CMAKE_COMMAND} -E remove_directory _package_build
    #     COMMAND ${CMAKE_COMMAND} -E make_directory _package_build
    #     COMMAND env DESTDIR="${CMAKE_CURRENT_BINARY_DIR}/_package_build"
    #             ${CMAKE_COMMAND} --build "${CMAKE_CURRENT_BINARY_DIR}" -- install
    #     COMMAND ${CMAKE_COMMAND} -E chdir _package_build/opt/hnc
    #             zip -9r "${CMAKE_CURRENT_BINARY_DIR}/${package_file_name}" hoffice${hwp_VERSION_MAJOR}
    #     COMMAND cp "${package_file_name}" "${debug_package_file_name}"
    #     COMMAND zip -d "${package_file_name}" -r '*.debug'
    #     COMMAND zip -d "${debug_package_file_name}" '*' -x '*.debug'
    #     WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    # )
endif() # if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")