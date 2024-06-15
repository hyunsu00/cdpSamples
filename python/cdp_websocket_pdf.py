import json
import base64
import asyncio
import websockets  # pip3 install websockets
import logging
import time
from cdp_common import (
    get_websocket_debugger_url,
    recv_response_message_id,
    wait_for_page_load,
    Counter
)

logging.basicConfig(level=logging.INFO)  # 로깅 레벨 설정


async def HtmlToPdf(
    url: str, path: str = "screenshot.pdf"
) -> None:
    try:
        # WebSocket 연결 설정
        dev_tools_websocket_url = get_websocket_debugger_url("127.0.0.1", 9222)
        logging.info("[dev_tools_websocket_url] : %s", dev_tools_websocket_url)

        message_id = Counter()

        async with websockets.connect(
            dev_tools_websocket_url, max_size=10 * 2**20
        ) as ws:
            # 탭 생성
            await ws.send(
                json.dumps(
                    {
                        "id": message_id.increment(),
                        "method": "Target.createTarget",
                        "params": {"url": "about:blank"},
                    }
                )
            )
            message = await recv_response_message_id(ws, message_id.get())
            target_id = message["result"]["targetId"]

            # 탭 연결
            await ws.send(
                json.dumps(
                    {
                        "id": message_id.increment(),
                        "method": "Target.attachToTarget",
                        "params": {"targetId": target_id, "flatten": True},
                    }
                )
            )
            message = await recv_response_message_id(ws, message_id.get())
            session_id = message["result"]["sessionId"]

            # 이벤트 활성화
            await ws.send(
                json.dumps(
                    {
                        "id": message_id.increment(),
                        "method": "Page.enable",
                        "sessionId": session_id,
                    }
                )
            )
            # 브라우저 크기 설정 (1280 x 720)
            DEVICE_WIDTH = 1280
            DEVICE_HEIGHT = 720
            await ws.send(
                json.dumps(
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
                    }
                )
            )
            # 페이지 로드
            await ws.send(
                json.dumps(
                    {
                        "id": message_id.increment(),
                        "method": "Page.navigate",
                        "params": {"url": url},
                        "sessionId": session_id,
                    }
                )
            )
            # 페이지 로드 완료 대기
            await wait_for_page_load(ws)

            # await asyncio.sleep(5)

            # PDF로 인쇄
            # A4 사이즈 (210 x 297 mm) 
            PAPER_WIDTH = 8.27 
            PAPER_HEIGHT = 11.7
            await ws.send(
                json.dumps(
                    {
                        "id": message_id.increment(),
                        "method": "Page.printToPDF",
                        "params": {
                            "landscape": False,
                            "displayHeaderFooter": False,
                            "printBackground": False,
                            "scale": 1,
                            "paperWidth": PAPER_WIDTH,
                            "paperHeight": PAPER_HEIGHT,
                            "marginTop": 0.4,
                            "marginBottom": 0.4,
                            "marginLeft": 0.4,
                            "marginRight": 0.4,
                            "pageRanges": "",
                            "headerTemplate": "",
                            "footerTemplate": "",
                            "preferCSSPageSize": False,   
                            "transferMode": "ReturnAsBase64",
                            "generateTaggedPDF": False,
                            "generateDocumentOutline": False,
                        },
                        "sessionId": session_id,
                    }
                )
            )
            message = await recv_response_message_id(ws, message_id.get())
            screenshot_data = message["result"]["data"]
            logging.debug("PDF 데이터 수신 완료")
            with open(path, "wb") as f:
                f.write(base64.b64decode(screenshot_data))
            logging.debug("PDF 문서가 성공적으로 저장되었습니다.")
    except Exception as e:
        logging.debug("[exception][message] : %s", str(e))


if __name__ == "__main__":
    start_time = time.time()  # 시작 시간을 측정합니다.
    asyncio.get_event_loop().run_until_complete(HtmlToPdf("https://www.naver.com", "cdp_websocket_naver.pdf"))
    end_time = time.time()  # 종료 시간을 측정합니다.
    elapsed_time = end_time - start_time  # 실행 시간을 계산합니다.
    logging.info("cdp_websocket_pdf %s seconds to run.", elapsed_time)
