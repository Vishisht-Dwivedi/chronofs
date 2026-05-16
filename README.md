# ChronoFS

A lightweight filesystem event tracking and snapshot daemon written in C using Linux system primitives.

ChronoFS monitors directories recursively using `inotify`, tracks filesystem activity in realtime, and maintains threaded watchers through a Unix Domain Socket IPC architecture.

---

## Features

* Recursive directory watching
* Realtime filesystem event logging
* Multi-directory concurrent watchers
* Unix Domain Socket IPC communication
* Thread-per-watch execution model
* Dynamic watcher registration/unregistration
* Automatic recursive watcher expansion for newly created directories
* Persistent activity logs
* Snapshot-based commit system *
* Rollback support (in progress) *

---

## Architecture

```text
Client Process
    ↓
Unix Domain Socket
    ↓
ChronoFS Daemon
    ↓
Watcher Threads
    ↓
inotify Event Streams
```

Each watched directory is assigned:

* a dedicated watcher thread
* its own recursive inotify tree
* an isolated event log

---

## IPC Commands

### Watch a directory

```bash
./build/chronofs_listener watch testdir
```

### Remove watcher

```bash
./build/chronofs_listener unwatch testdir
```

### Create snapshot commit

````bash
./build/chronofs_listener commit testdir
```bash
chronofs watch /path/to/directory
````

### Remove watcher

```bash
chronofs unwatch /path/to/directory
```

### Create commit snapshot *(WIP)*

```bash
chronofs commit /path/to/directory
```

---

## Event Types Tracked

* File creation
* File deletion
* File modification
* File move operations
* Directory creation/removal
* Recursive directory additions

---

## Project Structure

```text
.
├── LICENSE
├── README.md
├── build
│   ├── chronofs_daemon
│   └── chronofs_listener
├── current.log
└── src
    ├── chronofs_daemon.c
    └── chronofs_listener.c
```

### Components

#### chronofs_daemon

The background daemon process responsible for:

* handling IPC connections
* managing watcher threads
* recursively attaching inotify watchers
* logging filesystem events
* processing watch/unwatch requests

#### chronofs_listener

A lightweight client utility that:

* parses CLI commands
* builds IPC packets
* communicates with the daemon over Unix Domain Sockets
* sends filesystem management requests

## Build

```bash
gcc -Wall -Wextra -pthread -g src/chronofs_daemon.c -o build/chronofs_daemon

gcc -Wall -Wextra -pthread -g src/chronofs_listener.c -o build/chronofs_listener
```

Or using VSCode tasks:

```bash
Ctrl + Shift + B
```

---

## Running

### Start daemon

```bash
./build/chronofs_daemon
```

### Watch a directory

```bash
./build/chronofs watch testdir
```

---

## Current Log Output

Each watched directory generates:

```text
current.log
```

Example:

```text
1746765000 Created test.txt
1746765005 Modified test.txt
1746765010 Deleted test.txt
```

---

## Current Status

Implemented:

* [x] IPC daemon
* [x] Recursive inotify watchers
* [x] Dynamic thread management
* [x] Watch/unwatch operations
* [x] Cleanup handlers
* [x] Recursive directory discovery
* [x] Snapshot commit system

In Progress:
* [ ] Rollback support
* [ ] Diff-based commits
* [ ] Persistent commit metadata
* [ ] Mutex synchronization
* [ ] Graceful shutdown handling

---

## Technologies Used

* C
* POSIX Threads
* Linux inotify API
* Unix Domain Sockets
* Low-level filesystem APIs

---

## Motivation

ChronoFS started as an exploration of Linux systems programming concepts including:

* IPC mechanisms
* daemon architecture
* recursive filesystem monitoring
* thread lifecycle management
* event-driven systems
* memory management in concurrent environments

The long-term goal is building a lightweight local filesystem versioning system entirely with low-level Linux primitives.
