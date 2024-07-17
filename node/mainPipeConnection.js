const { spawn } = require('child_process');

// Chrome binary path
const CHROME_PATH = '/opt/google/chrome/chrome';
// const CHROME_PATH = '/home/hyunsu00/dev/chromium/src/out/Debug/chrome';
// const CHROME_PATH = './chrome/chrome-headless-shell-linux64/chrome-headless-shell';

// Google Chrome 브라우저를 새로운 프로세스로 시작
// 부모 프로세스와 파이프를 통해 통신 (stdin, stdout, stderr, fd 3, fd 4), stdin, stdout, stderr는 무시
const chrome = spawn(CHROME_PATH, ['--enable-features=UseOzonePlatform', '--ozone-platform=wayland',
    '--enable-logging', '--v=2', '--no-sandbox', '--disable-gpu', '--remote-debugging-pipe'], {
    stdio: ['ignore', 'ignore', 'ignore', 'pipe', 'pipe']
});

// Send a command to Chrome
const command = JSON.stringify({ id: 1, method: "Target.createTarget", params: { url: "https://www.naver.com" } });

// Write command to fd 3
chrome.stdio[3].write(command);
chrome.stdio[3].write('\0');

// Read response from fd 4
chrome.stdio[4].on('data', (data) => {
    console.log('부모 프로세스가 받은 데이터: ', data.toString());
});

// 자식 프로세스가 종료될 때까지 기다림
chrome.on('close', (code) => {
    console.log(`Chrome process exited with code ${code}`);
});
