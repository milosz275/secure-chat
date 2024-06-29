# Secure Chat

[![Docker image](https://github.com/mldxo/secure-chat/actions/workflows/docker-image.yml/badge.svg)](https://github.com/mldxo/secure-chat/actions/workflows/docker-image.yml)
[![CodeQL scan](https://github.com/mldxo/secure-chat/actions/workflows/codeql.yml/badge.svg)](https://github.com/mldxo/secure-chat/actions/workflows/codeql.yml)
[![Doxygen Pages](https://github.com/mldxo/secure-chat/actions/workflows/doxygen-pages.yml/badge.svg)](https://github.com/mldxo/secure-chat/actions/workflows/doxygen-pages.yml)
[![GitHub release](https://img.shields.io/github/v/release/mldxo/secure-chat)](https://github.com/mldxo/secure-chat/releases)
[![GitHub issues](https://img.shields.io/github/issues/mldxo/secure-chat)](https://github.com/mldxo/secure-chat/issues)
[![GitHub license](https://img.shields.io/github/license/mldxo/secure-chat)](LICENSE)

![Logo](assets/logo.png)

Secure Chat is a C program that allows you to chat securely with your friends. It uses the RSA algorithm to encrypt and decrypt messages and the Diffie-Hellman algorithm to exchange keys. Messages are stored in Cassandra database and can be read by the recipient only. Sending messages in optimized for maximum performance and resource usage.

> [!IMPORTANT]
> This project is still in development and is not ready for production use.

# Table of Contents

- [Getting Started](#getting-started)
<!-- - [Prerequisites](#prerequisites)
- [Installation](#installation) -->
- [Components](#components)
- [Usage](#usage)
- [Server](#server)
- [Client](#client)
- [Common](#common)
- [Database](#database)
- [License](#license)
- [Authors](#authors)
- [Contributing](#contributing)

# Getting Started

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes.

```bash
git clone https://github.com/mldxo/secure-chat
cd secure-chat
make
```

Run the server and client executables in separate terminals.

```bash
server/build/bin/server
```

```bash
client/build/bin/client
```

# Components

## Server

Server is responsible for handling client connections, retrieving messages from the database and sending messages to the recipients.

## Client

Client connects to the server, sends messages and receives messages from the server.

## Common

Common generates static library that is used by both server and client, i.e. communication protocol, encryption and decryption functions.

## Database

Database is a [Cassandra](https://cassandra.apache.org/) database that stores messages.

# License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

# Authors

- [mldxo](https://github.com/mldxo)

# Contributing

Please refer to [CONTRIBUTING.md](CONTRIBUTING.md). We appreciate your help!
