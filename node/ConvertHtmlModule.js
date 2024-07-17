// CDPPipe 클래스를 기본 클래스를 만든다.
const { spawn } = require('child_process');
const fs = require('fs');

class CDPPipe {

    m_Chrome = null;
    m_WriteFD = null;
    m_ReadFD = null;
    m_MessageQueue = [];
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
        this.m_Chrome.kill(); // Chrome 프로세스 종료
    }

    SetTimeout(milliseconds) {
        // 메서드 구현
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

    _Wait(id) {
        let command = null;
        while (true) {
            command = this.m_Pipe.Read();
            if (command === null || command.length === 0) {
                break;
            }

            try {
                const message = JSON.parse(command);
                if (message.error) { // error 속성(키)가 존재하면, 존재하지 않으면 undefined
                    return message;
                } else if (message.id === id) {
                    return message;
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

    Navigate(url) {
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
        let message = this._Wait(this.m_ID);
        if (Object.keys(message).length === 0 || message.error) {
            return false;
        }
        const targetId = message.result.targetId;

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
        message = this._Wait(this.m_ID);
        if (Object.keys(message).length === 0 || message.error) {
            return false;
        }
        const sessionId = message.result.sessionId;

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

        return true;
    }
}

let cdpManager = new CDPManager();
cdpManager.Launch();
cdpManager.Navigate('https://www.naver.com');
