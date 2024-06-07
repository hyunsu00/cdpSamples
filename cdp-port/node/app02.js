const http = require('http');
const WebSocket = require('ws');

// chrome-headless-shell --headless --disable-gpu --remote-debugging-port=9222 --enable-logging --log-level=0 https://www.naver.com
// Chrome DevTools 웹소켓 주소
// const devToolsWebSocketUrl = 'ws://127.0.0.1:9222/devtools/browser/f2dc5b3e-72b4-4905-b333-85ee3bbba762';

function getWebSocketDebuggerUrl(addr, port) {
    return new Promise((resolve, reject) => {
        const options = {
            hostname: addr,
            port: port,
            path: '/json/version',
            method: 'GET',
        };

        const req = http.request(options, res => {
            let data = '';

            res.on('data', chunk => {
                data += chunk;
            });

            res.on('end', () => {
                const jsonData = JSON.parse(data);
                resolve(jsonData.webSocketDebuggerUrl);
            });
        });

        req.on('error', error => {
            reject(error);
        });

        req.end();
    });
}

(async function() {
    try {
        const devToolsWebSocketUrl = await getWebSocketDebuggerUrl('127.0.0.1', 9222);
        console.log(devToolsWebSocketUrl);

        const ws = new WebSocket(devToolsWebSocketUrl);

        ws.on('open', () => {
            console.log('WebSocket 연결 성공');
            ws.send(JSON.stringify({
                id: 1,
                method: "Target.createTarget",
                params: {
                    url: "https://www.naver.com/"
                }
            }));
        });

        ws.on('message', (data) => {
            const message = JSON.parse(data);
            console.log(`message : ${JSON.stringify(message)}`);

            if (message.id == 1 && message.result && message.result.targetId) {
               // Target.attachToTarget 호출
               ws.send(JSON.stringify({
                    id: 2,
                    method: 'Target.attachToTarget',
                    params: {
                        targetId: message.result.targetId,
                        flatten: true
                    }
                }));
            }

            if (message.id == 2 && message.result && message.result.sessionId) {
                // Page.captureScreenshot 호출
                ws.send(JSON.stringify({
                    sessionId: message.result.sessionId,
                    id: 3,
                    method: 'Page.captureScreenshot',
                    params: {
                        format: 'png',
                        quality: 100,
                        fromSurface: true
                    }
                }));
            }

            if (message.id === 3 && message.result && message.result.data) {
                // 스크린샷 데이터 수신 및 처리
                const screenshotData = message.result.data;
                console.log('스크린샷 데이터 수신 완료');

                // 여기서 스크린샷 데이터를 원하는 방식으로 저장 또는 처리할 수 있습니다.
                // 예: 파일로 저장
                const fs = require('fs');
                fs.writeFileSync('screenshot.png', Buffer.from(screenshotData, 'base64'));

                console.log('스크린샷이 성공적으로 저장되었습니다.');
                ws.close(); // 웹소켓 연결 종료
            }
        });

        ws.on('error', (error) => {
            console.error('WebSocket 오류:', error);
        });
    } catch (error) {
        console.error(error);
    }
})();