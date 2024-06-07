#!/usr/bin/env bash
export CHROME_LOG_FILE=/dev/stdout

# Start Chrome in headless mode
# ./chrome/chrome-headless-shell-linux64/chrome-headless-shell --headless --remote-debugging-port=9222 --enable-logging=stderr --v=1 --no-sandbox --disable-gpu


/usr/bin/chrome --enable-features=UseOzonePlatform --ozone-platform=wayland --remote-debugging-port=9222 --enable-logging=stderr --v=1 --no-sandbox --disable-gpu