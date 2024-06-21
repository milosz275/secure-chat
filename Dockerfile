# Use an official image as a parent image
FROM debian:latest

# Set the working directory
WORKDIR /usr/src/app

# Install dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    libcassandra-dev \
    libcassandra2 \
    libc6-dev \
    wget \
    && rm -rf /var/lib/apt/lists/*

# Copy server and common directories into the container
COPY server /usr/src/app/server
COPY common /usr/src/app/common

# Build the server application
WORKDIR /usr/src/app/server
RUN make

# Make port 12345 available to the world outside this container
EXPOSE 12345

# Run the server
CMD ["./server"]
