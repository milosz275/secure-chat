# Use an official image as a parent image
FROM debian:latest

# Set the working directory
WORKDIR /usr/src/app

# Install dependencies
RUN apt-get update && apt-get install -y \
    g++ \
    git \
    gcc \
    make \
    wget \
    cmake \
    libz-dev \
    libc6-dev \
    libssl-dev \
    libuv1-dev \
    libcurl4-openssl-dev

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
