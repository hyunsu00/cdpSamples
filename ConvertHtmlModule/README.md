
# rockylinux 8.x
# https://rhel.pkgs.org/8/epel-x86_64/chromium-headless-126.0.6478.182-1.el8.x86_64.rpm.html
# chromium-headless-126.0.6478.182-1.el8.x86_64.rpm
# /usr/lib64/chromium-browser/ 경로에 위치
$ sudo dnf install chromium-headless

# 직접설치
$ sudo dnf install -y nss \
    nspr \
    atk \
    cups-libs \
    libdrm \
    at-spi2-atk \
    libXcomposite \
    libXdamage \
    libXext \
    libXfixes \
    libXrandr \
    mesa-libgbm \
    libxkbcommon \
    pango \
    cairo \
    alsa-lib



# ubuntu 22.04
# 직접설치
$ sudo apt install -qq -y --no-install-recommends libnss3 \
    libnspr4 \
    libatk1.0-0 \
    libatk-bridge2.0-0 \  
    libcups2 \
    libdrm2 \
    libatspi2.0-0 \
    libxcomposite1 \
    libxdamage1 \
    libxext6 \
    libxfixes3 \
    libxrandr2 \
    libgbm1 \
    libxkbcommon0 \
    libpango-1.0-0 \
    libcairo2 \
    libasound2

# 확인요망
# https://github.com/chromedp/docker-headless-shell/tree/master
$ apt-get install -y libnspr4 libnss3 libexpat1 libfontconfig1 libuuid1 socat