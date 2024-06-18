#!/bin/bash

# # 명명된 파이프 생성
# mkfifo /tmp/FD_3 /tmp/FD_4

# # 크롬을 파이프 모드로 실행하고 입출력을 명명된 파이프로 리디렉션
# /usr/bin/chrome --enable-features=UseOzonePlatform --ozone-platform=wayland --remote-debugging-pipe 3</tmp/FD_3 4>/tmp/FD_4 &
# CHROME_PID=$!

# DevTools 프로토콜 메시지 예제

REQUEST='{"id": 1, "method": "Target.createTarget", "params": {"url": "https://www.naver.com"}}\n\0'
 
# 명명된 파이프에 메시지 쓰기
echo -en "$REQUEST" > /tmp/FD_3

# # 명명된 파이프에서 응답 읽기
# read -r RESPONSE < FD_4
# echo "Response: $RESPONSE"

# read -r RESPONSE < cdp_pipeout
# # 크롬 프로세스 종료
# kill $CHROME_PID
# wait $CHROME_PID

# # 명명된 파이프 삭제
# rm -f pipein pipeout
