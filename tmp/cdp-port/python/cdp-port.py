import json
import base64
import http.client
import argparse
import websocket # pip3 install websocket-client
import asyncio
import websockets # pip3 install websockets
from io import BytesIO
from PIL import Image # pip3 install pillow

def get_websocket_debugger_url(addr, port):
    conn = http.client.HTTPConnection(addr, port)
    conn.request('GET', '/json/version')
    res = conn.getresponse()
    data = res.read().decode('utf-8')
    jsonData = json.loads(data)
    return jsonData['webSocketDebuggerUrl']

def sync_screenshot(url):
    # 크롬 실행시 --remote-allow-origins=* 나 --remote-allow-origins=http://127.0.0.1:9222 필수
    try:
        dev_tools_websocket_url = get_websocket_debugger_url('127.0.0.1', 9222)
        print(dev_tools_websocket_url)

        ws = websocket.WebSocket()
        ws.connect(dev_tools_websocket_url)

        # 페이지 생성
        ws.send(json.dumps({
            "id": 1,
            "method": "Target.createTarget",
            "params": {
                "url": url
            }
        }))

        while True:
            message = json.loads(ws.recv())
            print(f'message : {json.dumps(message)}')

            # 페이지 생성 응답
            if message.get('id') == 1 and 'result' in message and 'targetId' in message['result']:
                # 페이지 연결
                ws.send(json.dumps({
                    "id": 2,
                    "method": "Target.attachToTarget",
                    "params": {
                        "targetId": message['result']['targetId'],
                        "flatten": True
                    }
                }))
            # 페이지 연결 응답
            elif message.get('id') == 2 and 'result' in message and 'sessionId' in message['result']:
                # Page.enable 호출
                ws.send(json.dumps({
                    "id": 3,
                    "method": "Page.enable",
                    "sessionId": message['result']['sessionId']
                }))
            # Page.enable 응답 (로드 이벤트 대기)
            elif "method" in message and message["method"] == "Page.loadEventFired":
                # 페이지 캡쳐
                ws.send(json.dumps({
                    "sessionId": message['sessionId'],
                    "id": 4,
                    "method": "Page.captureScreenshot",
                    "params": {
                        "format": "png",
                        "quality": 100,
                        "fromSurface": True
                    }
                }))
            # Page.captureScreenshot 응답
            elif message.get('id') == 4 and 'result' in message and 'data' in message['result']:
                screenshot_data = message['result']['data']
                print('스크린샷 데이터 수신 완료')

                with open('cdp-port_sync-screenshot.png', 'wb') as f:
                    f.write(base64.b64decode(screenshot_data))

                print('스크린샷이 성공적으로 저장되었습니다.')
                break

    except Exception as e:
        print(e)

async def async_screenshot(url):
    try:
        dev_tools_websocket_url = get_websocket_debugger_url('127.0.0.1', 9222)
        print(dev_tools_websocket_url)

        async with websockets.connect(dev_tools_websocket_url) as ws:
            # 페이지 생성
            await ws.send(json.dumps({
                "id": 1,
                "method": "Target.createTarget",
                "params": {
                    "url": url
                }
            }))

            while True:
                message = json.loads(await ws.recv())
                print(f'message : {json.dumps(message)}')

                # 페이지 생성 응답
                if message.get('id') == 1 and 'result' in message and 'targetId' in message['result']:
                    # 페이지 연결
                    await ws.send(json.dumps({
                        "id": 2,
                        "method": "Target.attachToTarget",
                        "params": {
                            "targetId": message['result']['targetId'],
                            "flatten": True
                        }
                    }))
                # 페이지 연결 응답
                elif message.get('id') == 2 and 'result' in message and 'sessionId' in message['result']:
                    # Page.enable 호출
                    await ws.send(json.dumps({
                        "id": 3,
                        "method": "Page.enable",
                        "sessionId": message['result']['sessionId']
                    }))
                # Page.enable 응답 (로드 이벤트 대기)
                elif "method" in message and message["method"] == "Page.loadEventFired":
                    # 페이지 캡쳐
                    await ws.send(json.dumps({
                        "sessionId": message['sessionId'],
                        "id": 4,
                        "method": "Page.captureScreenshot",
                        "params": {
                            "format": "png",
                            "quality": 100,
                            "fromSurface": True
                        }
                    }))
                # Page.captureScreenshot 응답
                elif message.get('id') == 4 and 'result' in message and 'data' in message['result']:
                    screenshot_data = message['result']['data']
                    print('스크린샷 데이터 수신 완료')

                    with open('cdp-port_async-screenshot.png', 'wb') as f:
                        f.write(base64.b64decode(screenshot_data))

                    print('스크린샷이 성공적으로 저장되었습니다.')
                    break

    except Exception as e:
        print(e)

async def async_full_screenshot(url):
    try :
        # WebSocket 연결 설정
        dev_tools_websocket_url = get_websocket_debugger_url('127.0.0.1', 9222)
        print(dev_tools_websocket_url)
        
        async with websockets.connect(dev_tools_websocket_url) as ws:
            # 탭 생성
            await ws.send(json.dumps({
                "id": 1,
                "method": "Target.createTarget",
                "params": {"url": "about:blank"}
            }))

            # Page.navigate 까지 실행
            while True:
                message = json.loads(await ws.recv())
                print(f'message : {json.dumps(message)}')

                # 탭 생성 응답
                if message.get('id') == 1 and 'result' in message and 'targetId' in message['result']:
                    target_id = message['result']['targetId']
                    # 탭 연결
                    await ws.send(json.dumps({
                        "id": 2,
                        "method": "Target.attachToTarget",
                        "params": {"targetId": target_id, "flatten": True}
                    }))
                elif message.get('id') == 2 and 'result' in message and 'sessionId' in message['result']:
                    session_id = message["result"]["sessionId"] 
                    # 이벤트 활성화
                    await ws.send(json.dumps({
                        "id": 3,
                        "method": "Page.enable",
                        "sessionId": session_id
                    }))
                    # 페이지 로드
                    await ws.send(json.dumps({
                        "id": 4,
                        "method": "Page.navigate",
                        "params": {"url": url},
                        "sessionId": session_id
                    }))
                # Page.enable 응답 (로드 이벤트 대기)
                elif "method" in message and message["method"] == "Page.loadEventFired":
                    session_id = message['sessionId']
                    # 브라우저 크기 설정 (1920x1080)
                    await ws.send(json.dumps({
                        "id": 5,
                        "method": "Emulation.setDeviceMetricsOverride",
                        "params": {
                            "width": 1920,
                            "height": 1080,
                            "deviceScaleFactor": 1,
                            "mobile": False
                        },
                        "sessionId": session_id
                    }))
                    break
            
            await asyncio.sleep(2)

            while True:
                message = json.loads(await ws.recv())
                print(f'message : {json.dumps(message)}')     

                # 스크롤바 숨기기 응답
                if message.get('id') == 5 and 'sessionId' in message:
                    session_id = message['sessionId']
                    # 스크롤바 숨기기
                    await ws.send(json.dumps({
                        "id": 6,
                        "method": "Runtime.evaluate",
                        "params": {"expression": "document.body.style.overflow = 'hidden';"},
                        "sessionId": session_id
                    }))    
                elif message.get('id') == 6 and 'sessionId' in message:
                    session_id = message["sessionId"]
                     # 페이지 높이 가져오기
                    await ws.send(json.dumps({
                        "id": 7,
                        "method": "Runtime.evaluate",
                        "params": {"expression": "document.body.scrollHeight"},
                        "sessionId": session_id
                    }))
                # 페이지 높이 응답
                elif message.get('id') == 7 and 'result' in message and 'result' in message['result'] and 'value' in message['result']['result']:
                    session_id = message["sessionId"]
                    page_height = message["result"]["result"]["value"]
                    # 뷰포트 크기 설정
                    viewport_height = 1080  # 원하는 뷰포트 높이
                    num_screenshots = (page_height // viewport_height) + 1

                    # 전체 페이지 스크린샷 캡처
                    screenshots = []
                    for i in range(num_screenshots):
                        offset = i * viewport_height
                        # 스크롤 이동
                        await ws.send(json.dumps({
                            "id": 8 + i * 2,
                            "method": "Runtime.evaluate",
                            "params": {"expression": f"window.scrollTo(0, {offset});", "awaitPromise": True},
                            "sessionId": session_id
                        }))
                        await ws.recv()

                        # 스크린샷 촬영
                        await ws.send(json.dumps({
                            "id": 9 + i * 2,
                            "method": "Page.captureScreenshot",
                            "params": {"fromSurface": True},
                            "sessionId": session_id
                        }))
                        _message = json.loads(await ws.recv())
                        screenshot = _message["result"]["data"]
                        screenshots.append(screenshot)

                    # 스크린샷 결합
                    images = [Image.open(BytesIO(base64.b64decode(screenshot))) for screenshot in screenshots]
                    total_height = sum(image.height for image in images)
                    combined_image = Image.new('RGB', (images[0].width, total_height))
                
                    current_height = 0
                    for image in images:
                        combined_image.paste(image, (0, current_height))
                        current_height += image.height
                
                    combined_image.save('full_page_screenshot.png')
                    break
    except Exception as e:
        print(e)

# 아규먼트 파서 생성
parser = argparse.ArgumentParser()
parser.add_argument("--mode", help="sync_screenshot or async_screenshot", choices=['sync_screenshot', 'async_screenshot', 'async_full_screenshot'], default='async_screenshot')

args = parser.parse_args()

# 아규먼트에 따라 함수 실행
if args.mode == 'sync_screenshot':
    sync_screenshot("https://www.google.com/")
elif args.mode == 'async_screenshot':
    asyncio.run(async_screenshot("https://www.naver.com/"))
elif args.mode == 'async_full_screenshot':
    asyncio.run(async_full_screenshot("https://www.naver.com/"))