import subprocess
import json
import os

# # Chrome binary path (replace with your actual path)
# CHROME_PATH = "/usr/bin/chrome"

# # Chrome launch flags
# chrome_flags = [
#     "--enable-features=UseOzonePlatform",
#     "--ozone-platform=wayland",
#     "--enable-logging",
#     "--v=2",
#     "--no-sandbox",
#     "--disable-gpu",
#     "--remote-debugging-pipe",
# ]
CHROME_PATH = "/opt/google/chrome/chrome"
chrome_flags = [
    "--enable-logging",
    "--v=2",
    "--no-sandbox",
    "--disable-gpu",
    "--remote-debugging-pipe",
]

try:
    fdr1, fdw1 = os.pipe()
    fdr2, fdw2 = os.pipe()
    print(f"[fdr1] : {fdr1}, [fdw1] : {fdw1}, [fdr2] : {fdr2}, [fdw2] : {fdw2}")

    # Spawn Chrome with the appropriate flags
    # chrome_process = subprocess.Popen(
    #     [
    #         CHROME_PATH,
    #         "--enable-features=UseOzonePlatform",
    #         "--ozone-platform=wayland",
    #         "--enable-logging",
    #         "--v=2",
    #         "--no-sandbox",
    #         "--disable-gpu",
    #         "--remote-debugging-pipe",
    #         "3<{fdr1} 4>{fdw2}".format(fdr1=fdr1, fdw2=fdw2),
    #     ]
    # )
    chrome_process = subprocess.Popen(
        [CHROME_PATH] + chrome_flags,
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    # Send a request to Chrome to create a new tab
    command = json.dumps(
        {
            "id": 1,
            "method": "Target.createTarget",
            "params": {"url": "https://www.naver.com"},
        }
    )
    # Encode command as JSON and send to Chrome
    request = json.dumps(command).encode('utf-8')
    chrome_process.stdin.write(request)
    chrome_process.stdin.write(b'\0')  # Null byte to terminate the command

    # Read response from Chrome
    response = chrome_process.stdout.read()
    print('Received:', response.decode('utf-8'))

    # Wait for Chrome process to finish and get the exit code
    exit_code = chrome_process.wait()
    print(f'Chrome process exited with code {exit_code}')

except Exception as e:
    print("[exception][message] : ", str(e))
