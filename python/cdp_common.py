import json
import http.client
import websockets  # pip3 install websockets
import logging

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
    
def get_websocket_debugger_url(addr: str, port: int) -> str:
    conn = http.client.HTTPConnection(addr, port)
    conn.request("GET", "/json/version")
    res = conn.getresponse()
    data = res.read().decode("utf-8")
    jsonData = json.loads(data)
    return jsonData["webSocketDebuggerUrl"]

async def recv_response_message_id(
    ws: websockets.WebSocketClientProtocol, message_id: int
) -> dict:
    while True:
        response = await ws.recv()
        message = json.loads(response)
        logging.debug("[recv_response_message_id] : %s", json.dumps(message, indent=4))
        if message.get("id") == message_id:
            return message

async def wait_for_page_load(ws: websockets.WebSocketClientProtocol) -> None:
    count = 0
    while True:
        response = await ws.recv()
        message = json.loads(response)
        count += 1
        logging.debug(
            f"[wait_for_page_load][{count}] : " + json.dumps(message, indent=4)
        )
        if "method" in message and message["method"] == "Page.loadEventFired":
            break