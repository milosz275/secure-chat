FROM debian:latest

RUN apt-get update && apt-get install -y \
    g++ \
    git \
    build-essential \
    gcc \
    make \
    wget \
    cmake \
    sqlite3 \
    libsqlite3-dev \
    libz-dev \
    libc6-dev \
    libssl-dev \
    libuv1-dev \
    libcurl4-openssl-dev \
    netcat-openbsd

WORKDIR /usr/src/app

COPY server /usr/src/app/server
COPY common /usr/src/app/common

WORKDIR /usr/src/app/common
RUN make lib

WORKDIR /usr/src/app/server
RUN make

EXPOSE 12345

CMD ["/usr/src/app/server/build/bin/server"]
