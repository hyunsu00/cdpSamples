# import asyncio
# import subprocess
# import json
# import base64

# async def read_message(pipe):
#     length_bytes = await pipe.read(4)
#     length = int.from_bytes(length_bytes, byteorder='little')
#     message_bytes = await pipe.read(length)
#     return json.loads(message_bytes.decode('utf-8'))

# async def send_message(pipe, message):
#     message_bytes = json.dumps(message).encode('utf-8')
#     length_bytes = len(message_bytes).to_bytes(4, byteorder='little')
#     pipe.write(length_bytes)
#     pipe.write(message_bytes)
#     await pipe.drain()

# async def capture_page_as_image(url):
#     # Chrome 브라우저를 Pipe 모드로 실행
#     proc = await asyncio.create_subprocess_exec(
#         '/usr/bin/chrome', '--headless', '--disable-gpu', '--remote-debugging-pipe',
#         stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE
#     )

#     reader = proc.stdout
#     writer = proc.stdin

#     # 기본적인 CDP 메시지 ID 설정
#     message_id = 1

#     # 탭 생성
#     await send_message(writer, {
#         "id": message_id,
#         "method": "Target.createTarget",
#         "params": {"url": "about:blank"}
#     })
#     response = await read_message(reader)
#     target_id = response["result"]["targetId"]
#     message_id += 1

#     # 탭 연결
#     await send_message(writer, {
#         "id": message_id,
#         "method": "Target.attachToTarget",
#         "params": {"targetId": target_id, "flatten": True}
#     })
#     response = await read_message(reader)
#     session_id = response["result"]["sessionId"]
#     message_id += 1

#     # 이벤트 활성화
#     await send_message(writer, {
#         "id": message_id,
#         "method": "Page.enable",
#         "sessionId": session_id
#     })
#     await read_message(reader)
#     message_id += 1

#     await send_message(writer, {
#         "id": message_id,
#         "method": "Network.enable",
#         "sessionId": session_id
#     })
#     await read_message(reader)
#     message_id += 1

#     # 페이지 로드
#     await send_message(writer, {
#         "id": message_id,
#         "method": "Page.navigate",
#         "params": {"url": url},
#         "sessionId": session_id
#     })
#     message_id += 1

#     # 페이지 로드 완료 대기
#     page_loaded = False
#     network_idle = False
#     while not (page_loaded and network_idle):
#         message = await read_message(reader)
#         if "method" in message:
#             if message["method"] == "Page.loadEventFired":
#                 page_loaded = True
#             elif message["method"] == "Network.loadingFinished":
#                 network_idle = True

#     # 뷰포트 설정
#     await send_message(writer, {
#         "id": message_id,
#         "method": "Page.getLayoutMetrics",
#         "sessionId": session_id
#     })
#     response = await read_message(reader)
#     layout_metrics = response["result"]
#     width = layout_metrics["contentSize"]["width"]
#     height = layout_metrics["contentSize"]["height"]
#     message_id += 1

#     await send_message(writer, {
#         "id": message_id,
#         "method": "Emulation.setDeviceMetricsOverride",
#         "params": {
#             "width": width,
#             "height": height,
#             "deviceScaleFactor": 1,
#             "mobile": False
#         },
#         "sessionId": session_id
#     })
#     await read_message(reader)
#     message_id += 1

#     # 스크린샷 캡처
#     await send_message(writer, {
#         "id": message_id,
#         "method": "Page.captureScreenshot",
#         "params": {
#             "format": "png",
#             "clip": {
#                 "x": 0,
#                 "y": 0,
#                 "width": width,
#                 "height": height,
#                 "scale": 1
#             }
#         },
#         "sessionId": session_id
#     })
#     response = await read_message(reader)
#     screenshot_data = response["result"]["data"]

#     # 스크린샷 파일로 저장
#     with open('page.png', 'wb') as f:
#         f.write(base64.b64decode(screenshot_data))

#     # 프로세스 종료
#     proc.terminate()
#     await proc.wait()

# # asyncio 이벤트 루프 실행
# url = 'https://www.naver.com'
# asyncio.run(capture_page_as_image(url))

import subprocess
import json
import os

# 크롬을 파이프로 실행
proc = subprocess.Popen(
    [
        '/usr/bin/chrome',
        '--headless',
        '--disable-gpu',    
        '--remote-debugging-pipe',
        '3<pipe_write',
        '4>pipe_read'
    ],
    text=True
)

# 파이프 파일 열기
try:
    pipe_write_fd = os.open('pipe_write', os.O_WRONLY | os.O_NONBLOCK)
    pipe_read_fd = os.open('pipe_read', os.O_RDONLY | os.O_NONBLOCK)
except FileNotFoundError as e:
    print("FileNotFoundError:", e)

# DevTools 프로토콜 메시지 예제
# 여기서는 간단히 'Target.getBrowserContexts' 요청을 보냅니다.
message = json.dumps({
    'id': 1,
    'method': 'Target.getBrowserContexts'
})

# 파이프에 메시지 쓰기
try :
    os.write(pipe_write_fd, message + '\n' + '\0')
except BrokenPipeError as e:
    print("BrokenPipeError:", e)

# 파이프로부터 응답 읽기
while True:
    # 파이프에서 데이터 읽기
    response = os.read(pipe_read_fd,  1024)  # 1024 바이트씩 읽음 (조정 가능)

    if not data:
        break
    
    # 읽은 데이터 출력
    print("Received:", response)

# 프로세스 종료
proc.terminate()
proc.wait()
