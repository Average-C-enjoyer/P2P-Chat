---

# Encrypted Chat (C)

Scalable encrypted client-server chat written in pure **C**.

The project focuses on clean architecture, separation of concerns and efficient non-blocking I/O on the server side.

---

## Features

* Multiple concurrent clients
* Non-blocking server built on **epoll (edge-triggered mode)**
* TLS encryption via OpenSSL
* Clean separation between client and server APIs
* Modular client architecture (core + UI)
* Cross-platform client (Linux / Windows)
* Linux-first server implementation

---

## Architecture

### Client

The client is split into two logical layers:

* **Core**

  * Networking
  * TLS handling
  * Protocol logic
  * Internal state management

* **UI**

  * User interaction
  * Input/output
  * Communicates with the core via a defined interface

This eliminates tight coupling and makes the client easier to extend or replace (e.g. GUI in the future).

---

### Server

* Fully non-blocking
* Built on **epoll**
* Edge-triggered event loop
* Scalable connection handling
* No thread-per-client model

The server is designed specifically for Linux.

---

## Technologies

| Component    | Used                                   |
| ------------ | -------------------------------------- |
| Language     | C                                      |
| Encryption   | OpenSSL                                |
| Server I/O   | epoll (Linux)                          |
| Client I/O   | select / platform sockets              |
| Build System | CMake                                  |
| Platforms    | Linux (server), Linux/Windows (client) |

---

## Project Structure

```
.
├── cli_client      # Ui wrapper for command line. Gui will be added later
│   ├── EChat_cli.c
│   ├── include
│   │   ├── menu.h
│   │   └── terminal.h
│   └── src
│       ├── menu.c
│       └── terminal.c
├── client_core     # Core with main functionality
│   ├── include
│   │   ├── TLS.h
│   │   ├── client_core.h
│   │   └── cross_platform_api.h
│   └── src
│       └── client_core.c
├── server          # Server directory
│   ├── include
│   │   ├── server.h
|   |   └── workers.h
│   ├── src
|   |   └── server_core.c
│   └── server.c
├── tests
│   └── test_menu.c
└── utils           # Data structeres and utility funcs
    ├── String.h
    ├── queue.h
    └── darray.h
```

---

## Build

Using CMake:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

---

## Run

### Linux

Start server:

```bash
./server
```

Start client:

```bash
./client <server_ip>
```

### Windows

```bash
client.exe <server_ip>
```

(Server is intended to run on Linux.)

---

## Client Commands

Exit client:

```
exit
```

Change nickname:

```
[NAME]NewName
```

---

## Goals of This Project

* Practice low-level network programming
* Implement scalable non-blocking server architecture
* Build clean modular C codebase
* Separate transport, protocol and presentation layers

---