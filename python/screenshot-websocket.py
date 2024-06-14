import json
import base64
import http.client
import asyncio
import websockets  # pip3 install websockets
from io import BytesIO
from PIL import Image  # pip3 install pillow


def get_websocket_debugger_url(addr: str, port: int) -> str:
    conn = http.client.HTTPConnection(addr, port)
    conn.request("GET", "/json/version")
    res = conn.getresponse()
    data = res.read().decode("utf-8")
    jsonData = json.loads(data)
    return jsonData["webSocketDebuggerUrl"]


async def recv_response_message(
    ws: websockets.WebSocketClientProtocol, id: int
) -> dict:
    while True:
        response = await ws.recv()
        message = json.loads(response)
        print("[recv_response_message] : " + json.dumps(message, indent=4))
        if message.get("id") == id:
            return message


async def wait_for_messages(ws: websockets.WebSocketClientProtocol) -> None:
    Id = 0
    while True:
        response = await ws.recv()
        message = json.loads(response)
        print(f"[wait_for_messages][{Id}] : " + json.dumps(message, indent=4))
        Id += 1


async def wait_for_page_load(ws: websockets.WebSocketClientProtocol) -> None:
    Id = 0
    while True:
        response = await ws.recv()
        message = json.loads(response)
        print(f"[wait_for_page_load][{Id}] : " + json.dumps(message, indent=4))
        if "method" in message and message["method"] == "Page.loadEventFired":
            break
        Id += 1


async def screenshot(url: str) -> None:
    try:
        # WebSocket 연결 설정
        dev_tools_websocket_url = get_websocket_debugger_url("127.0.0.1", 9222)
        print(dev_tools_websocket_url)

        async with websockets.connect(dev_tools_websocket_url, max_size=10 * 2**20) as ws:
            # 탭 생성
            messageId = 1
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
            sessionId = message["result"]["sessionId"]

            # 이벤트 활성화
            messageId += 1
            await ws.send(
                json.dumps(
                    {"id": messageId, "method": "Page.enable", "sessionId": sessionId}
                )
            )
            # 브라우저 크기 설정 (1280 x 720)
            deviceWidth = 1280
            deviceHeight = 720
            messageId += 1
            await ws.send(
                json.dumps(
                    {
                        "id": messageId,
                        "method": "Emulation.setDeviceMetricsOverride",
                        "params": {
                            "mobile": False,
                            "width": deviceWidth,
                            "height": deviceHeight,
                            "screenWidth": deviceWidth,
                            "screenHeight": deviceHeight,
                            "deviceScaleFactor": 1,
                            "screenOrientation": {
                                "angle": 0,
                                "type": "landscapePrimary",
                            },
                        },
                        "sessionId": sessionId,
                    }
                )
            )
            # 페이지 로드
            messageId += 1
            await ws.send(
                json.dumps(
                    {
                        "id": messageId,
                        "method": "Page.navigate",
                        "params": {"url": url},
                        "sessionId": sessionId,
                    }
                )
            )
            # 페이지 로드 완료 대기
            await wait_for_page_load(ws)
            # 레이아웃 메트릭스 가져오기
            messageId += 1
            await ws.send(
                json.dumps(
                    {
                        "id": messageId,
                        "method": "Page.getLayoutMetrics",
                        "sessionId": sessionId,
                    }
                )
            )
            message = await recv_response_message(ws, messageId)
            contentWidth = message["result"]["contentSize"]["width"]
            contentHeight = message["result"]["contentSize"]["height"]
            # 스크린샷 캡처
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
                                "width": contentWidth,
                                "height": contentHeight,
                                "scale": 1,
                            },
                            "captureBeyondViewport": True
                        },
                        "sessionId": sessionId,
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
        print("[exception][message]" + str(e))


asyncio.get_event_loop().run_until_complete(screenshot("https://www.naver.com"))
