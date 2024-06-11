import json
import base64
import http.client
import argparse
import websocket # pip3 install websocket-client
import asyncio
import websockets # pip3 install websockets

def get_websocket_debugger_url(addr, port):
    conn = http.client.HTTPConnection(addr, port)
    conn.request('GET', '/json/version')
    res = conn.getresponse()
    data = res.read().decode('utf-8')
    jsonData = json.loads(data)
    return jsonData['webSocketDebuggerUrl']

def sync_main():
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
                "url": "https://www.google.com/"
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

async def async_main():
    try:
        dev_tools_websocket_url = get_websocket_debugger_url('127.0.0.1', 9222)
        print(dev_tools_websocket_url)

        async with websockets.connect(dev_tools_websocket_url) as ws:
            # 페이지 생성
            await ws.send(json.dumps({
                "id": 1,
                "method": "Target.createTarget",
                "params": {
                    "url": "https://www.naver.com/"
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

# 아규먼트 파서 생성
parser = argparse.ArgumentParser()
parser.add_argument("--mode", help="sync or async", choices=['sync', 'async'], default='sync')

args = parser.parse_args()

# 아규먼트에 따라 함수 실행
if args.mode == 'sync':
    sync_main()
elif args.mode == 'async':
    asyncio.run(async_main())