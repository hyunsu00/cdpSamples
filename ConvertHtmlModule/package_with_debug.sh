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

split_package_single $1