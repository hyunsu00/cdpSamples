set(package_file_name "${CPACK_PACKAGE_FILE_NAME}.tar.gz")
set(debug_package_file_name "${CPACK_PACKAGE_FILE_NAME}.debug.tar.gz")
message(STATUS "package_file_name: ${package_file_name}")
message(STATUS "debug_package_file_name: ${debug_package_file_name}")
message(STATUS "CMAKE_CURRENT_BINARY_DIR: ${CMAKE_CURRENT_BINARY_DIR}")

execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy "${package_file_name}" "${debug_package_file_name}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

message(STATUS "패키지 파일을 새 이름으로 복사했습니다: ${CPACK_PACKAGE_FILE_NAME}.debug.${CPACK_PACKAGE_FILE_EXTENSION}")