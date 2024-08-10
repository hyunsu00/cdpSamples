#!/usr/bin/env bash

# *.debug 파일 삭제
function remove_debug_files {
    set -e

    # 인자로 받은 tar.gz 파일의 이름
    local tar_gz_file=$1

    # tar 파일의 이름 (gzip 압축을 해제한 후)
    local tar_file=${tar_gz_file%.gz}

    # 압축 해제
    gunzip $tar_gz_file

    # *.debug 파일 삭제
    tar --delete --wildcards --file=$tar_file '*.debug'

    # 다시 압축
    gzip $tar_file
}

# *.debug  이외 파일 삭제
function remove_non_debug_files {
    set -e

    local TAR_FILE="$1"
    
    # 삭제할 파일의 리스트를 생성 ('.debug'로 끝나지 않는 파일 목록을 찾음)
    local DELETE_LIST=$(tar -tzf "$TAR_FILE" | grep -vE '\.debug$')
    
    # 파일 삭제
    if [ -n "$DELETE_LIST" ]; then
        tar --delete -f "$TAR_FILE" $DELETE_LIST
        echo "Non-debug files deleted successfully from $TAR_FILE."
    else
        echo "No non-debug files found in $TAR_FILE."
    fi
}

function split_package_parallel() {
    set -e
    
    local package_file_name=$1
    local debug_package_file_name="123.debug.tar"

    echo "package_file_name=$package_file_name"
    echo "debug_package_file_name=$debug_package_file_name"

    # package_file_name 복사
    cp "${package_file_name}" "${debug_package_file_name}"
    # *.debug 파일 삭제
    tar -I pigz --delete --file=${package_file_name} *.debug
    # *.debug  이외 파일 삭제
    # tar -I pigz --delete --file="${debug_package_file_name}" '*' --exclude '*.debug'
}

function split_package_single() {
    set -e
    
    local package_file_name=$1
    local debug_package_file_name="456.xxx.tar.gz"

    echo "package_file_name=$package_file_name"
    echo "debug_package_file_name=$debug_package_file_name"

    # package_file_name 복사
    cp "${package_file_name}" "${debug_package_file_name}"

    # *.debug 파일 삭제
    # remove_debug_files "${package_file_name}"

    # *.debug  이외 파일 삭제
    remove_non_debug_files "${debug_package_file_name}"
}

# split_package_single $1

mkdir -p ./build
cd build/
cmake ..
make DESTDIR=./_package_build install

# 명령어 존재 여부 확인
command -v tar >/dev/null 2>&1 || { echo >&2 "tar 명령어를 찾을 수 없습니다. 설치 후 다시 시도해주세요."; exit 1; }
command -v pigz >/dev/null 2>&1 || { echo >&2 "pigz 명령어를 찾을 수 없습니다. 설치 후 다시 시도해주세요."; exit 1; }

PACKAGE_FOLDER="./_package_build/usr/local/"
# .debug 파일 제외 tar.gz 압축
time tar --exclude='*.debug' -czvf remove_debug_files_single.tar.gz -C $PACKAGE_FOLDER .
time tar --exclude='*.debug' -I pigz -cvf remove_debug_files_parallel.tar.gz -C $PACKAGE_FOLDER .

# .debug 파일만 tar.gz 압축
DEBUG_FILE_LIST="debug_file_list.txt"
find $PACKAGE_FOLDER -name "*.debug" -print0 | xargs -0 -I {} echo {} | sed "s|^\\$PACKAGE_FOLDER||" > $DEBUG_FILE_LIST
time tar -czvf debug_files_single.tar.gz -C $PACKAGE_FOLDER -T $DEBUG_FILE_LIST
time tar -I pigz -cvf debug_files_parallel.tar.gz -C $PACKAGE_FOLDER -T $DEBUG_FILE_LIST