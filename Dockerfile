FROM debian:latest

RUN apt-get update && apt-get install -y \
    git \
    g++ \
    gcc \
    make \
    wget \
    cmake \
    sqlite3 \
    libz-dev \
    xorg-dev \
    libc6-dev \
    libxi-dev \
    libssl-dev \
    libuv1-dev \
    libx11-dev \
    libxrandr-dev \
    libsqlite3-dev \
    netcat-openbsd \
    libasound2-dev \
    libwayland-dev \
    build-essential \
    mesa-common-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libcurl4-openssl-dev

RUN git clone https://github.com/raysan5/raylib.git raylib
WORKDIR /raylib
RUN make PLATFORM=PLATFORM_DESKTOP
RUN make install

WORKDIR /usr/src/app

COPY server /usr/src/app/server
COPY common /usr/src/app/common

WORKDIR /usr/src/app/common
RUN make lib

WORKDIR /usr/src/app/server
RUN make

EXPOSE 12345

CMD ["/usr/src/app/server/build/bin/server"]
