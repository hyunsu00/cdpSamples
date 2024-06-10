import asyncio
import http.client
import json
import base64
import websockets # pip3 install websockets

def get_websocket_debugger_url(addr, port):
    conn = http.client.HTTPConnection(addr, port)
    conn.request('GET', '/json/version')
    res = conn.getresponse()
    data = res.read().decode('utf-8')
    jsonData = json.loads(data)
    return jsonData['webSocketDebuggerUrl']

async def main():
    try:
        dev_tools_websocket_url = get_websocket_debugger_url('127.0.0.1', 9222)
        print(dev_tools_websocket_url)

        async with websockets.connect(dev_tools_websocket_url) as ws:
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

                if message.get('id') == 1 and 'result' in message and 'targetId' in message['result']:
                    await ws.send(json.dumps({
                        "id": 2,
                        "method": "Target.attachToTarget",
                        "params": {
                            "targetId": message['result']['targetId'],
                            "flatten": True
                        }
                    }))

                elif message.get('id') == 2 and 'result' in message and 'sessionId' in message['result']:
                    await ws.send(json.dumps({
                        "sessionId": message['result']['sessionId'],
                        "id": 3,
                        "method": "Page.captureScreenshot",
                        "params": {
                            "format": "png",
                            "quality": 100,
                            "fromSurface": True
                        }
                    }))

                elif message.get('id') == 3 and 'result' in message and 'data' in message['result']:
                    screenshot_data = message['result']['data']
                    print('스크린샷 데이터 수신 완료')

                    with open('screenshot.png', 'wb') as f:
                        f.write(base64.b64decode(screenshot_data))

                    print('스크린샷이 성공적으로 저장되었습니다.')
                    break

    except Exception as e:
        print(e)

asyncio.run(main())
