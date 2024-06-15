import asyncio
import json
import io
import time
import base64
import logging

logging.basicConfig(level=logging.DEBUG)  # 로깅 레벨 설정

class Counter:
    def __init__(self):
        self.count = 0

    def increment(self) -> int:
        self.count += 1
        return self.count

    def decrement(self) -> int:
        self.count -= 1
        return self.count

    def __call__(self) -> int:
        return self.count

    def get(self) -> int:
        return self.count


async def send_request_message(pipein: io.TextIOWrapper, message: dict) -> int:
    num_bytes = pipein.write(json.dumps(message) + '\n' + '\0')
    # num_bytes += pipein.write("\n")
    # num_bytes += pipein.write("\0")
    # pipein.flush()
    return num_bytes


async def recv_response_message_id(pipeout: io.TextIOWrapper, message_id: int) -> dict:
    while True:
        response = pipeout.readline()
        message = json.loads(response)
        logging.debug("[recv_response_message_id] : %s", json.dumps(message, indent=4))
        if message.get("id") == message_id:
            return message


async def wait_for_page_load(pipeout: io.TextIOWrapper) -> None:
    count = 0
    while True:
        response = pipeout.readline()
        message = json.loads(response)
        count += 1
        logging.debug(
            f"[wait_for_page_load][{count}] : " + json.dumps(message, indent=4)
        )
        if "method" in message and message["method"] == "Page.loadEventFired":
            break


async def HtmlToImage(
    url: str, path: str = "screenshot.png", format: str = "png"
) -> None:
    try:
        message_id = Counter()
        pipein = open("cdp_pipein", "w")
        pipeout = open("cdp_pipeout", "r")

        # 탭 생성
        await send_request_message(
            pipein,
            {
                "id": message_id.increment(),
                "method": "Target.createTarget",
                "params": {"url": "about:blank"},
            },
        )
        message = await recv_response_message_id(pipeout, message_id.get())
        target_id = message["result"]["targetId"]

        # 탭 연결
        await send_request_message(
            pipein,
            {
                "id": message_id.increment(),
                "method": "Target.attachToTarget",
                "params": {"targetId": target_id, "flatten": True},
            },
        )
        message = await recv_response_message_id(pipeout, message_id.get())
        session_id = message["result"]["sessionId"]

        # 이벤트 활성화
        await send_request_message(
            pipein,
            {
                "id": message_id.increment(),
                "method": "Page.enable",
                "sessionId": session_id,
            },
        )
        # 브라우저 크기 설정 (1280 x 720)
        DEVICE_WIDTH = 1280
        DEVICE_HEIGHT = 720
        await send_request_message(
            pipein,
            {
                "id": message_id.increment(),
                "method": "Emulation.setDeviceMetricsOverride",
                "params": {
                    "mobile": False,
                    "width": DEVICE_WIDTH,
                    "height": DEVICE_HEIGHT,
                    "screenWidth": DEVICE_WIDTH,
                    "screenHeight": DEVICE_HEIGHT,
                    "deviceScaleFactor": 1,
                    "screenOrientation": {
                        "angle": 0,
                        "type": "landscapePrimary",
                    },
                },
                "sessionId": session_id,
            },
        )
        # 페이지 로드
        await send_request_message(
            pipein,
            {
                "id": message_id.increment(),
                "method": "Page.navigate",
                "params": {"url": url},
                "sessionId": session_id,
            },
        )
        # 페이지 로드 완료 대기
        await wait_for_page_load(pipeout)
        # 레이아웃 메트릭스 가져오기
        await send_request_message(
            pipein,
            {
                "id": message_id.increment(),
                "method": "Page.getLayoutMetrics",
                "sessionId": session_id,
            },
        )
        message = await recv_response_message_id(pipeout, message_id.get())
        contentWidth = message["result"]["contentSize"]["width"]
        contentHeight = message["result"]["contentSize"]["height"]

        # await asyncio.sleep(5)

        # 스크린샷 캡처
        await send_request_message(
            pipein,
            {
                "id": message_id.increment(),
                "method": "Page.captureScreenshot",
                "params": {
                    "format": format,
                    "clip": {
                        "x": 0,
                        "y": 0,
                        "width": contentWidth,
                        "height": contentHeight,
                        "scale": 1,
                    },
                    "captureBeyondViewport": True,
                },
                "sessionId": session_id,
            },
        )
        message = await recv_response_message_id(pipeout, message_id.get())
        screenshot_data = message["result"]["data"]
        logging.debug("스크린샷 데이터 수신 완료")
        with open(path, "wb") as f:
            f.write(base64.b64decode(screenshot_data))
        logging.debug("스크린샷이 성공적으로 저장되었습니다.")
    except Exception as e:
        logging.debug("[exception][message] : %s", str(e))

if __name__ == "__main__":
    start_time = time.time()  # 시작 시간을 측정합니다.
    asyncio.get_event_loop().run_until_complete(
        HtmlToImage("https://www.naver.com", "cdp_pipe_naver.png", "png")
    )
    end_time = time.time()  # 종료 시간을 측정합니다.
    elapsed_time = end_time - start_time  # 실행 시간을 계산합니다.
    logging.info("cdp_websocket_screenshot %s seconds to run.", elapsed_time)
