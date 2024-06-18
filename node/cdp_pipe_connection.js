const { spawn } = require('child_process');

const CHROME_PATH = '/home/hyunsu00/dev/chromium/src/out/Debug/chrome'; // Chrome binary path

// Spawn Chrome with the appropriate flags
const chrome = spawn(CHROME_PATH, ['--enable-features=UseOzonePlatform', '--ozone-platform=wayland',
    '--enable-logging', '--v=2', '--no-sandbox', '--disable-gpu', '--remote-debugging-pipe'], {
    stdio: ['ignore', 'ignore', 'ignore', 'pipe', 'pipe']
});
// const CHROME_PATH = '/opt/google/chrome/chrome'; // Chrome binary path

// Spawn Chrome with the appropriate flags
// const chrome = spawn(CHROME_PATH, ['--enable-logging', '--v=2', '--no-sandbox', '--disable-gpu', '--remote-debugging-pipe'], {
//     stdio: ['ignore', 'pipe', 'pipe', 'pipe', 'pipe']
// });

// Send a command to Chrome
const command = JSON.stringify({ id: 1, method: "Target.createTarget", params: { url: "https://www.naver.com" }});

// Write command to fd 3
chrome.stdio[3].write(command);
chrome.stdio[3].write('\0');

// Read response from fd 4
chrome.stdio[4].on('data', (data) => {
    console.log('Received:', data.toString());
});

chrome.on('close', (code) => {
    console.log(`Chrome process exited with code ${code}`);
});
