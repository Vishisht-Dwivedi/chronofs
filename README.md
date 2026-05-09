# ChronoFS

A lightweight filesystem event tracking and snapshot daemon and listener written in C using Linux system primitives.

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
* Snapshot-based commit system *(in progress)*

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
chronofs watch /path/to/directory
```

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
src/
    chronofs_daemon.c
    chronofs.c

build/
```

---

## Build

```bash
gcc -Wall -Wextra -pthread -g src/*.c -o build/chronofs-daemon
```

Or using VSCode tasks:

```bash
Ctrl + Shift + B
```

---

## Running

### Start daemon

```bash
./build/chronofs-daemon
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

In Progress:

* [ ] Snapshot commit system
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
