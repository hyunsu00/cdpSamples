import subprocess
import json
import os

CHROME_PATH = "/usr/bin/chrome"  # Chrome binary path

try:
    fdr1, fdw1 = os.pipe()
    fdr2, fdw2 = os.pipe()
    print(f"[fdr1] : {fdr1}, [fdw1] : {fdw1}, [fdr2] : {fdr2}, [fdw2] : {fdw2}")

    # Spawn Chrome with the appropriate flags
    chrome = subprocess.Popen(
        [
            CHROME_PATH,
            "--enable-features=UseOzonePlatform",
            "--ozone-platform=wayland",
            "--enable-logging",
            "--v=2",
            "--no-sandbox",
            "--disable-gpu",
            "--remote-debugging-pipe",
            "3<{fdr1} 4>{fdw2}".format(fdr1=fdr1, fdw2=fdw2),
        ]
    )

    # Send a request to Chrome to create a new tab
    request = json.dumps(
        {
            "id": 1,
            "method": "Target.createTarget",
            "params": {"url": "https://www.naver.com"},
        }
    )
    os.write(fdw1, request.encode())
    os.write(fdw1, b'\0')

    # Read response from Chrome
    response = os.read(fdr2, 100)
    print('Received:', response.decode())

    # Wait for Chrome to exit and print exit code
    exit_code = chrome.wait()
    print(f'Chrome process exited with code {exit_code}')

except Exception as e:
    print("[exception][message] : ", str(e))
