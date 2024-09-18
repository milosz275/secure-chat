# Use an official image as a parent image
FROM debian:latest

# Install dependencies
RUN apt-get update && apt-get install -y \
    g++ \
    git \
    build-essential \
    gcc \
    make \
    wget \
    cmake \
    libz-dev \
    libc6-dev \
    libssl-dev \
    libuv1-dev \
    libcurl4-openssl-dev
    
# Clone the cpp-driver repository and build it
RUN git clone https://github.com/datastax/cpp-driver.git && \
    cd cpp-driver && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make && \
    make install

# Set the working directory
WORKDIR /usr/src/app

# Copy server and common directories into the container
COPY server /usr/src/app/server
COPY common /usr/src/app/common
COPY database /usr/src/app/database

# Build the common library
WORKDIR /usr/src/app/common
RUN make lib

# Build the server application
WORKDIR /usr/src/app/server
RUN make

# Make port 12345 available to the world outside this container
EXPOSE 12345
RUN chmod +x /usr/src/app/server/build/bin/server

# Run server when the container launches
CMD ["/usr/src/app/server/build/bin/server"]
