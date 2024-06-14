import asyncio
import websockets
import json
import base64
import http.client


def get_websocket_debugger_url(addr, port):
    conn = http.client.HTTPConnection(addr, port)
    conn.request("GET", "/json/version")
    res = conn.getresponse()
    data = res.read().decode("utf-8")
    jsonData = json.loads(data)
    return jsonData["webSocketDebuggerUrl"]


async def recv_response_message(ws: websockets.WebSocketClientProtocol, id: int):
    while True:
        response = await ws.recv()
        message = json.loads(response)
        print(message)
        if message.get("id") == id:
            return message


async def capture_full_page_screenshot(url):
    try:
        # WebSocket 연결 설정
        browser_ws_endpoint = get_websocket_debugger_url("127.0.0.1", 9222)

        async with websockets.connect(browser_ws_endpoint) as ws:
            # 탭 생성
            messageId = 1  # 메시지 ID
            await ws.send(
                json.dumps(
                    {
                        "id": messageId,
                        "method": "Target.createTarget",
                        "params": {"url": "about:blank"},
                    }
                )
            )
            message = await recv_response_message(ws, messageId)
            target_id = message["result"]["targetId"]

            # 탭 연결
            messageId += 1
            await ws.send(
                json.dumps(
                    {
                        "id": messageId,
                        "method": "Target.attachToTarget",
                        "params": {"targetId": target_id, "flatten": True},
                    }
                )
            )
            message = await recv_response_message(ws, messageId)
            session_id = message["result"]["sessionId"]

            # 페이지 로드
            messageId += 1
            await ws.send(
                json.dumps(
                    {
                        "id": messageId,
                        "method": "Page.navigate",
                        "params": {"url": url},
                        "sessionId": session_id,
                    }
                )
            )
            message = await recv_response_message(ws, messageId)

            # 페이지 로드 완료 대기
            await asyncio.sleep(2)

            messageId += 1
            await ws.send(
                json.dumps(
                    {
                        "id": messageId,
                        "method": "Page.getLayoutMetrics",
                        "sessionId": session_id,
                    }
                )
            )
            message = await recv_response_message(ws, messageId)
            width = message["result"]["contentSize"]["width"]
            height = message["result"]["contentSize"]["height"]

            messageId += 1
            await ws.send(
                json.dumps(
                    {
                        "id": messageId,
                        "method": "Emulation.setDeviceMetricsOverride",
                        "params": {
                            "width": width,
                            "height": height,
                            "deviceScaleFactor": 1,
                            "mobile": False,
                        },
                        "sessionId": session_id,
                    }
                )
            )
            # 페이지 로드 완료 대기
            await asyncio.sleep(5)

            message = await recv_response_message(ws, messageId)

            messageId += 1
            await ws.send(
                json.dumps(
                    {
                        "id": messageId,
                        "method": "Page.captureScreenshot",
                        "params": {
                            "format": "png",
                            "clip": {
                                "x": 0,
                                "y": 0,
                                "width": width,
                                "height": height,
                                "scale": 1,
                            },
                        },
                        "sessionId": session_id,
                    }
                )
            )
            message = await recv_response_message(ws, messageId)
            screenshot_data = message["result"]["data"]
            print("스크린샷 데이터 수신 완료")
            with open("cdp-port_sync-screenshot.png", "wb") as f:
                f.write(base64.b64decode(screenshot_data))
            print("스크린샷이 성공적으로 저장되었습니다.")
    except Exception as e:
        print(e)


# asyncio 이벤트 루프 실행
url = "https://www.naver.com"
asyncio.get_event_loop().run_until_complete(capture_full_page_screenshot(url))
