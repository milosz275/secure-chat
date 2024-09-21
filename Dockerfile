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
    libz-dev \
    libc6-dev \
    libssl-dev \
    libuv1-dev \
    libcurl4-openssl-dev \
    netcat-openbsd

RUN git clone https://github.com/datastax/cpp-driver.git && \
    cd cpp-driver && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make && \
    make install

WORKDIR /usr/src/app

COPY server /usr/src/app/server
COPY common /usr/src/app/common
COPY database /usr/src/app/database
COPY wait-for-it.sh /usr/src/app/wait-for-it.sh

RUN chmod +x /usr/src/app/wait-for-it.sh

WORKDIR /usr/src/app/common
RUN make lib

WORKDIR /usr/src/app/server
RUN make

EXPOSE 12345

CMD ["/usr/src/app/wait-for-it.sh", "cassandra", "9042", "/usr/src/app/server/build/bin/server"]
