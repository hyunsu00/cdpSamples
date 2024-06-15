# cdpSamples

## 참고
- [Chrome DevTools Protocol](https://chromedevtools.github.io/devtools-protocol/)
- [Headless Chrome C++ DevTools API](https://docs.google.com/document/d/1rlqcp8nk-ZQvldNJWdbaMbwfDbJoOXvahPCDoPGOwhQ/edit#heading=h.pbplycf9595h)

```bash
# rockylinux 8.8
$ /opt/google/chrome/chrome --remote-debugging-port=9222
$ /opt/google/chrome/chrome --remote-debugging-port=9222 --headless
$ /opt/google/chrome/chrome --remote-debugging-port=9222 --remote-allow-origins=*
$ /opt/google/chrome/chrome --remote-debugging-port=9222 --remote-allow-origins=* --headless
$ /opt/google/chrome/chrome --remote-debugging-port=9222 

$ /usr/bin/chrome --headless --remote-debugging-port=9222
# wsl ubuntu 22.04
$ /usr/bin/chrome --enable-features=UseOzonePlatform --ozone-platform=wayland --remote-debugging-port=9222
$ /usr/bin/chrome --enable-features=UseOzonePlatform --ozone-platform=wayland --remote-debugging-port=9222 --remote-allow-origins=* 
$ /usr/bin/chrome --enable-features=UseOzonePlatform --ozone-platform=wayland --remote-debugging-port=9222 --remote-allow-origins=* --headless
$ /usr/bin/chrome --enable-features=UseOzonePlatform --ozone-platform=wayland --remote-debugging-pipe 3<cdp_pipein 4>cdp_pipeout
$ /usr/bin/chrome --enable-features=UseOzonePlatform --ozone-platform=wayland \
--enable-logging --v=1 2>&1 \
--no-sandbox --disable-gpu --headless \
--remote-debugging-pipe 3<cdp_pipein 4>cdp_pipeout

PS > "C:\Program Files\Google\Chrome\Application\chrome.exe" --remote-debugging-port=9222
```

## boost-beast
```bash
# 우분투
$ sudo apt-get install libboost-all-dev

# 레드햇
$ sudo yum install boost-devel
$ sudo dnf install boost-devel
```

## libcurl
```bash
# 우분투
sudo apt-get install libcurl4-openssl-dev

# 레드햇
$ sudo yum install libcurl-devel
$ sudo dnf install libcurl-devel
```

## libwebsockets
```bash
# 우분투
sudo apt-get install libwebsockets-dev

# 레드햇
$ sudo yum install libwebsockets-devel
$ sudo dnf install libwebsockets-devel
```

