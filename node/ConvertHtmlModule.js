// CDPPipe 클래스를 기본 클래스를 만든다.
const { spawn } = require('child_process');
const fs = require('fs');

class CDPPipe {

    m_Chrome = null;
    m_WriteFD = null;
    m_ReadFD = null;
    m_MessageQueue = [];
    m_Timeout = null;
    constructor() {
      // 생성자에서 초기화할 변수들을 정의합니다.
    }
  
    Launch() {
        const CHROME_PATH = '/opt/google/chrome/chrome';
        // const CHROME_PATH = '/home/hyunsu00/dev/chromium/src/out/Debug/chrome';
        // const CHROME_PATH = './chrome/chrome-headless-shell-linux64/chrome-headless-shell';
        const args = ['--enable-features=UseOzonePlatform', '--ozone-platform=wayland','--enable-logging', '--v=2', 
            '--no-sandbox', '--disable-gpu', '--remote-debugging-pipe'];
        const chrome = spawn(CHROME_PATH, args, {
            stdio: ['ignore', 'ignore', 'ignore', 'pipe', 'pipe']
        });

        if (chrome == null) {
            return false;
        }

        this.m_Chrome = chrome;
        this.m_WriteFD = chrome.stdio[3]._handle.fd;
        this.m_ReadFD = chrome.stdio[4]._handle.fd;
        return true;
    }
  
    Exit() {
        // m_WriteFD 닫기
        if (this.m_WriteFD !== null) {
            fs.closeSync(this.m_WriteFD);
            this.m_WriteFD = null;
        }
        // m_ReadFD 닫기
        if (this.m_ReadFD !== null) {
            fs.closeSync(this.m_ReadFD);
            this.m_ReadFD = null;
        }
        if (this.m_Chrome) {
            this.m_Chrome.kill(); // Chrome 프로세스 종료
            this.m_Chrome = null;
        }
    }

    SetTimeout(milliseconds) {
        this.m_Timeout = milliseconds;
    }

    Write(command) {
        const fd = this.m_WriteFD;
        const writeBuf = Buffer.from(command + '\0');
        let totalWritten = 0;
        let bytesToWrite = writeBuf.length;
        
        while (totalWritten < bytesToWrite) {
            try {
                let written = fs.writeSync(fd, writeBuf, totalWritten, bytesToWrite - totalWritten, null);
                totalWritten += written;
            } catch (e) {
                console.log('fs.writeSync() Error : ', e);
                return false;
            }
        }

        return true;

    }

    Read() {
        if (this.m_MessageQueue.length > 0) {
            return this.m_MessageQueue.shift();
        }

        const fd = this.m_ReadFD;
        const BUF_LEN = 4096;
        let readBuf = Buffer.alloc(BUF_LEN);
        let byteBuf = Buffer.alloc(0); // 빈 버퍼 초기화
        let readBytes = 0;

        while (true) {
            try {
                // 반환값은 읽은 바이트 수이며 반드시 0 이상이어야 한다.
                // 왜냐하면 0 미만이면 에러가 발생했음을 의미한다.
                readBytes = fs.readSync(fd, readBuf, 0, BUF_LEN, null);
            } catch (e) {
                if (e.code === 'EAGAIN') {
                    console.log('fs.readSync Error : ', e.code);
                    // Sleep 100ms
                    Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, 100)
                    continue;
                } else {
                    return null;
                }
            }

            if (readBytes > 0) {
                // 읽은 데이터를 byteBuf에 추가
                byteBuf = Buffer.concat([byteBuf, readBuf.slice(0, readBytes)]);
            }

            // 읽은 데이터가 없거나, 마지막 문자가 널 문자라면 루프 종료
            if (readBytes === 0 || readBuf[readBytes - 1] === 0) {
                break;
            }
        }
        
        if (byteBuf.length === 0) {
            return null;
        }

        // toString() 메서드를 이용하여 버퍼를 문자열로 변환
        // 널 문자를 제외하기 위해 버퍼의 길이에서 1을 뺀다.
        // 반드시 문자열이 반환되며 버퍼가 비어있을 경우 빈 문자열("")이 반환된다.
        // 실패하면 에러가 발생한다.
        let message = null;
        try {
            message = byteBuf.toString('utf8', 0, byteBuf.length - 1);
        } catch (e) {
            console.log('byteBuf.toString() Error : ', e);
            return null;
        }
        
        let messages = message.split('\0');
        for (let i = 0; i < messages.length; i++) {
            if (messages[i].length === 0) {
                continue;
            }
            this.m_MessageQueue.push(messages[i]);

            // Debugging
            console.log('[CDPPipe.Read()] : ', JSON.parse(messages[i]));
        }

        return this.m_MessageQueue.shift();
    }
}

class CDPManager {
    m_Pipe = null;
    m_ID = 0;
    m_TargetID = null;
    m_SessionID = null;
    constructor() {
        this.m_Pipe = new CDPPipe();
    }

    Launch() {
        return this.m_Pipe.Launch();
    }

    Exit() {
        this.m_Pipe.Exit();
    }

    SetTimeout(milliseconds) {
        this.m_Pipe.SetTimeout(milliseconds);
    }

    _Wait(param) {
        let result = null;
        if (typeof param === 'number') {
            result = this._WaitId(param);
        } else if (typeof param === 'string') {
            result = this._WaitMethod(param);
        } else {
            console.log('Invalid parameter type');
            result = null;
        }

        return result;
    }

    _WaitId(id) {
        let message = null;
        while (true) {
            message = this.m_Pipe.Read();
            if (message === null || message.length === 0) {
                break;
            }

            try {
                const jmessage = JSON.parse(message);
                if (jmessage.error) { // error 속성(키)가 존재하면, 존재하지 않으면 undefined
                    return jmessage;
                } else if (jmessage.id === id) {
                    return jmessage;
                } else {
                    continue;
                }
            } catch (e) {
                console.log('JSON.parse() Error : ', e);
                break;
            }
        }

        return null;
    }

    _WaitMethod(method) {
        let message = null;
        while (true) {
            message = this.m_Pipe.Read();
            if (message === null || message.length === 0) {
                break;
            }

            try {
                const jmessage = JSON.parse(message);
                if (jmessage.error) { // error 속성(키)가 존재하면, 존재하지 않으면 undefined
                    return jmessage;
                } else if (jmessage.method === method) {
                    return jmessage;
                } else {
                    continue;
                }
            } catch (e) {
                console.log('JSON.parse() Error : ', e);
                break;
            }
        }

        return null;
    }

    Navigate(url, viewWidth = -1, viewHeight = -1) {
        // 탭 생성
        let result = this.m_Pipe.Write(
            JSON.stringify(
                { 
                    id: ++this.m_ID, 
                    method: "Target.createTarget", 
                    params: { 
                        url: "about:blank" 
                    }
                }
            )
        );
        if (!result) {
            return false;
        }
        let jmessage = this._Wait(this.m_ID);
        if (!jmessage || Object.keys(jmessage).length === 0 || jmessage.error) {
            return false;
        }
        const targetId = jmessage.result.targetId;

        // 탭 연결
        result = this.m_Pipe.Write(
            JSON.stringify(
                { 
                    id: ++this.m_ID, 
                    method: "Target.attachToTarget", 
                    params: { 
                        targetId: targetId,
                        flatten: true 
                    }
                }
            )
        );
        if (!result) {
            return false;
        }
        jmessage = this._Wait(this.m_ID);
        if (!jmessage || Object.keys(jmessage).length === 0 || jmessage.error) {
            return false;
        }
        const sessionId = jmessage.result.sessionId;

        // 페이지 이벤트 활성화
        result = this.m_Pipe.Write(
            JSON.stringify(
                { 
                    id: ++this.m_ID, 
                    method: "Page.enable", 
                    sessionId: sessionId
                }
            )
        );
        if (!result) {
            return false;
        }
        // 네트워크 이벤트 활성화
        // result = this.m_Pipe.Write(
        //     JSON.stringify(
        //         { 
        //             id: ++this.m_ID, 
        //             method: "Network.enable", 
        //             sessionId: sessionId
        //         }
        //     )
        // );
        // if (!result) {
        //     return false;
        // }

        const DEFAULT_WIDTH = 1280;
        const DEFAULT_HEIGHT = 720;
        // 브라우저 크기 설정 (1280 x 720)
        const width = (viewWidth === -1) ? DEFAULT_WIDTH : viewWidth;
        const height = (viewHeight === -1) ? DEFAULT_HEIGHT : viewHeight;
        const screenWidth = width, screenHeight = height;
        result = this.m_Pipe.Write(
            JSON.stringify(
                { 
                    id: ++this.m_ID, 
                    method: "Emulation.setDeviceMetricsOverride", 
                    params: { 
                        mobile: false,
                        width: width, 
                        height: height, 
                        screenWidth: screenWidth,
                        screenHeight: screenHeight,
                        deviceScaleFactor: 1, 
                        screenOrientation: {
                            angle: 0,
                            type: "landscapePrimary"
                        }
                    },
                    sessionId: sessionId
                }
            )
        );
        if (!result) {
            return false;
        }

        // 페이지 로드
        result = this.m_Pipe.Write(
            JSON.stringify(
                { 
                    id: ++this.m_ID, 
                    method: "Page.navigate", 
                    params: { 
                        url: url
                    },
                    sessionId: sessionId
                }
            )
        );
        if (!result) {
            return false;
        }

        jmessage = this._Wait('Page.loadEventFired');
        if (!jmessage || Object.keys(jmessage).length === 0 || jmessage.error) {
            return false;
        }

        this.m_TargetID = targetId;
        this.m_SessionID = sessionId;
        return true;
    }

    Screenshot(
        resultFilePath,
        imageType = 'png',
        clipX = -1,
        clipY = -1,
        clipWidth = -1,
        clipHeight = -1
    ) {
        // 레이아웃 메트릭스 가져오기
        let result = this.m_Pipe.Write(
            JSON.stringify(
                { 
                    id: ++this.m_ID, 
                    method: "Page.getLayoutMetrics", 
                    sessionId: this.m_SessionID
                }
            )
        );
        if (!result) {
            return false;
        }
        let jmessage = this._Wait(this.m_ID);
        if (!jmessage || Object.keys(jmessage).length === 0 || jmessage.error) {
            return false;
        }

        // 스크린샷 캡처
        const contentWidth = jmessage.result.contentSize.width;
        const contentHeight = jmessage.result.contentSize.height;
        clipX = (clipX == -1) ? 0 : clipX;
        clipY = (clipY == -1) ? 0 : clipY;
        clipWidth = (clipWidth == -1) ? contentWidth : clipWidth;
        clipHeight = (clipHeight == -1) ? contentHeight : clipHeight;

        result = this.m_Pipe.Write(
            JSON.stringify(
                { 
                    id:  ++this.m_ID, 
                    method: "Page.captureScreenshot",
                    params: {
                        format: imageType,
                        clip: {
                            x: clipX,
                            y: clipY,
                            width: clipWidth,
                            height: clipHeight,
                            scale: 1
                        },
                        captureBeyondViewport: true
                    },
                    sessionId: this.m_SessionID
                }
            )
        );
        if (!result) {
            return false;
        }
        jmessage = this._Wait(this.m_ID);
        if (!jmessage || Object.keys(jmessage).length === 0 || jmessage.error) {
            return false;
        }

        const data = jmessage.result.data;
        result = this._SaveFile(resultFilePath, data);
        if (!result) {
            return false;
        }

        return true;
    }

    PrintToPDF(
        resultFilePath,
        margin = 0.4,
        landscape = false
    ) {
        // PDF로 인쇄
        // A4 사이즈 (210 x 297 mm)
        const PAPER_WIDTH = 8.27;
        const PAPER_HEIGHT = 11.7;
        let result = this.m_Pipe.Write(
            JSON.stringify(
                { 
                    id: ++this.m_ID, 
                    method: "Page.printToPDF",
                    params: {
                        landscape: landscape,
                        displayHeaderFooter: false,
                        printBackground: false,
                        scale: 1,
                        paperWidth: PAPER_WIDTH,
                        paperHeight: PAPER_HEIGHT,
                        marginTop: margin,
                        marginBottom: margin,
                        marginLeft: margin,
                        marginRight: margin,
                        pageRanges: "",
                        headerTemplate: "",
                        footerTemplate: "",
                        preferCSSPageSize: false,
                        transferMode: "ReturnAsBase64",
                        generateTaggedPDF: false,
                        generateDocumentOutline: false
                    },
                    sessionId: this.m_SessionID
                }
            )
        );
        if (!result) {
            return false;
        }
        let jmessage = this._Wait(this.m_ID);
        if (!jmessage || Object.keys(jmessage).length === 0 || jmessage.error) {
            return false;
        }

        const data = jmessage.result.data;
        result = this._SaveFile(resultFilePath, data);
        if (!result) {
            return false;
        }

        return true;
    }

    _SaveFile(resultFilePath, base64Str) {
        try {
            const decodedData = Buffer.from(base64Str, 'base64');
            fs.writeFileSync(resultFilePath, decodedData);
        } catch (e) {
            console.log('fs.writeFileSync() Error : ', e)
            return false;
        }

        return true;
    }
}

class ConvertHtmlModule {
    static HtmltoImage(
        htmlURL, resultFilePath, imageType, clipX, clipY, clipWidth, clipHeight, viewportWidth, viewportHeight
    ) {
        let cdpManager = new CDPManager();
        try {
            if (!cdpManager.Launch()) {
                return false;
            }
            cdpManager.SetTimeout(5000);
            if (!cdpManager.Navigate(htmlURL, viewportWidth, viewportHeight)) {
                return false;
            }
            if (!cdpManager.Screenshot(resultFilePath, imageType, clipX, clipY, clipWidth, clipHeight)) {
                return false;
            }

            return true;
        }  finally {
            cdpManager.Exit();
        }
    }

    static HtmlToPdf(
        htmlURL, resultFilePath, margin, isLandScape
    ) {
        let cdpManager = new CDPManager();
        try {
            if (!cdpManager.Launch()) {
                return false;
            }
            cdpManager.SetTimeout(5000);
            if (!cdpManager.Navigate(htmlURL, -1, -1)) {
                return false;
            }
    
            let marginValue = 0.4;
            if (margin) {
                marginValue = parseFloat(margin);
            }
            const landscape = (isLandScape === 1) ? true : false;
            if (!cdpManager.PrintToPDF(resultFilePath, marginValue, landscape)) {
                return false;
            }

            return true;
        } finally {
            cdpManager.Exit();
        }
    }
}

module.exports = ConvertHtmlModule;
