# cdpSamples

## 참고
- [Chrome DevTools Protocol](https://chromedevtools.github.io/devtools-protocol/)
- [Headless Chrome C++ DevTools API](https://docs.google.com/document/d/1rlqcp8nk-ZQvldNJWdbaMbwfDbJoOXvahPCDoPGOwhQ/edit#heading=h.pbplycf9595h)

```bash
# rockylinux 8.8
$ /opt/google/chrome/chrome --remote-debugging-port=9222
$ /usr/bin/chrome --headless --remote-debugging-port=9222
# wsl ubuntu 22.04
$ /usr/bin/chrome --enable-features=UseOzonePlatform --ozone-platform=wayland --remote-debugging-port=9222

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

sudo apt-get install libcurl4-openssl-dev

