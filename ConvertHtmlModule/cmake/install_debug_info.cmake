install(CODE "
    # execute_process 커맨드는 COMMAND를 병렬 수행하므로 아래 커맨드를
    # 따로따로 execute_process 커맨드로 실행해야 한다.
    # objcopy에 --only-keep-debug 옵션을 사용하지 않고 그냥 cp한다.
    # 파일 크기가 많이 차이나지 않으므로 원본 파일을 저장해두면 활용도가
    # 높을 것이다.
    execute_process(
        COMMAND cp \"${PROJECT_NAME}\" \"${PROJECT_NAME}.debug\"
        WORKING_DIRECTORY \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin\"
    )
    execute_process(
        COMMAND strip --strip-unneeded --remove-section=.comment --remove-section=.note  \"${PROJECT_NAME}\"
        WORKING_DIRECTORY \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin\"
    )
    execute_process(
        COMMAND objcopy \"--add-gnu-debuglink=${PROJECT_NAME}.debug\" \"${PROJECT_NAME}\"
        WORKING_DIRECTORY \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin\"
    )
")