// CDPPipe 클래스를 기본 클래스를 만든다.
const { spawn } = require('child_process');
const fs = require('fs');
const wait = require('wait');

class CDPPipe {

    m_Chrome = null;
    m_WriteFD = null;
    m_ReadFD = null;
    
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
        this.m_WriteFD = chrome.stdio[3];
        this.m_ReadFD = chrome.stdio[4];
        return true;
    }
  
    Exit() {
        this.m_Chrome.kill(); // Chrome 프로세스 종료
    }

    SetTimeout(milliseconds) {
        // 메서드 구현
    }

    Write(command) {
        const fd = this.m_WriteFD._handle.fd;
        const writeBuf = Buffer.from(command + '\0');
        let totalWritten = 0;
        let bytesToWrite = writeBuf.length;
        
        while (totalWritten < bytesToWrite) {
            try {
                let written = fs.writeSync(fd, writeBuf, totalWritten, bytesToWrite - totalWritten, null);
                totalWritten += written;
            } catch (err) {
                console.log(err);
                return false;
            }
        }

        return true;

    }

    Read() {
        const fd = this.m_ReadFD._handle.fd;
        const BUF_LEN = 4096;
        let readBuf = Buffer.alloc(BUF_LEN);
        let byteBuf = Buffer.alloc(0); // 빈 버퍼 초기화
        let readBytes = 0;

        do {
            readBytes = fs.readSync(fd, readBuf, 0, BUF_LEN, null);
            if (readBytes > 0) {
                // 읽은 데이터를 byteBuf에 추가
                byteBuf = Buffer.concat([byteBuf, readBuf.slice(0, readBytes)]);
            }
        } while (readBytes > 0 && readBuf[readBytes - 1] != 0); // '\0'의 바이트 값은 0입니다.
        
        const message = byteBuf.toString('utf8', 0, byteBuf.length - 1);
        return message;
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

    _IsEmpty(jsonObj) {
        // jsonObj가 객체인지 확인
        if (typeof jsonObj !== 'object' || jsonObj === null) {
            return true;
        }

        // 객체의 키와 값들을 순회하면서 빈 값 체크
        for (const key in jsonObj) {
        if (jsonObj.hasOwnProperty(key)) {
            const value = jsonObj[key];
            if (value !== null && value !== undefined && value !== '') {
                return false; // 하나라도 비어있지 않으면 false 반환
            }
        }
        }

        return true; // 모든 멤버가 비어있다면 true 반환
    }

    _Wait(id) {
        let command = null;
        while (true) {
            command = this.m_Pipe.Read();
            if (command === '' || command === null) {
                break;
            }

            const message = JSON.parse(command);
            if (message.error) {
                return message;
            } else if (message.id == id) {
                return message;
            } else {
                continue;
            }
        }

        return null;
    }

    Navigate(url) {
        let ret = this.m_Pipe.Write(
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
        if (ret === false) {
            return false;
        }

        const message = this._Wait(this.m_ID);
        if (this._IsEmpty(message) || message.error) {
            return false;
        }

        console.log(message);

        return true;
    }
}

let cdpManager = new CDPManager();
cdpManager.Launch();
cdpManager.Navigate('https://www.naver.com');
