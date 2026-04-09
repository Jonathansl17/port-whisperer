# port-whisperer

**A fast CLI tool to see what's running on your ports.**

Stop guessing which process is hogging port 3000. `port-whisperer` gives you a color-coded table of every dev server, database, and background process listening on your machine -- with framework detection, Docker container identification, and interactive process management.

Written in C++20 for maximum speed. On Linux, reads `/proc` directly -- no external commands needed for port scanning.

## What it looks like

```
$ ports

 ┌─────────────────────────────────────┐
 │  Port Whisperer                     │
 │  listening to your ports...         │
 └─────────────────────────────────────┘

┌───────┬─────────┬───────┬──────────────────────┬────────────┬────────┬───────────┐
│ PORT  │ PROCESS │ PID   │ PROJECT              │ FRAMEWORK  │ UPTIME │ STATUS    │
├───────┼─────────┼───────┼──────────────────────┼────────────┼────────┼───────────┤
│ :3000 │ node    │ 42872 │ frontend             │ Next.js    │ 1d 9h  │ ● healthy │
│ :3001 │ node    │ 95380 │ preview-app          │ Next.js    │ 2h 40m │ ● healthy │
│ :4566 │ docker  │ 58351 │ backend-localstack-1 │ LocalStack │ 10d 3h │ ● healthy │
│ :5432 │ docker  │ 58351 │ backend-postgres-1   │ PostgreSQL │ 10d 3h │ ● healthy │
│ :6379 │ docker  │ 58351 │ backend-redis-1      │ Redis      │ 10d 3h │ ● healthy │
└───────┴─────────┴───────┴──────────────────────┴────────────┴────────┴───────────┘

  5 ports active  ·  Run ports <number> for details  ·  --all to show everything
```

## Install

```bash
curl -fsSL https://raw.githubusercontent.com/Jonathansl17/port-whisperer/master/install.sh | sh
```

This detects your OS and architecture, downloads the correct binary, and installs it to `/usr/local/bin`.

### Build from source

Requires CMake 3.20+ and a C++20 compiler (GCC 12+, Clang 15+).

```bash
git clone https://github.com/Jonathansl17/port-whisperer.git
cd port-whisperer
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

## Usage

### Show dev server ports

```bash
ports
```

Shows dev servers, Docker containers, and databases. System apps (Spotify, Slack, etc.) are filtered out by default.

### Show all listening ports

```bash
ports --all
```

### Inspect a specific port

```bash
ports 3000
# or
whoisonport 3000
```

Detailed view: full process tree, repository path, current git branch, memory usage, and an interactive prompt to kill the process.

### Kill a process

```bash
ports kill 3000                # kill by port
ports kill 3000 5173 8080      # kill multiple
ports kill 42872               # kill by PID
ports kill -f 3000             # force kill (SIGKILL)
```

### Show all dev processes

```bash
ports ps
```

A beautiful `ps aux` for developers. Shows CPU%, memory, framework detection, and a smart description column. Docker processes are collapsed into a single summary row.

```bash
ports ps --all    # show all processes, not just dev
```

### Clean up orphaned processes

```bash
ports clean
```

Finds and kills orphaned or zombie dev server processes. Only targets dev runtimes (node, python, etc.).

### Watch for port changes

```bash
ports watch
```

Real-time monitoring that notifies you whenever a port starts or stops listening.

## How it works

On **Linux**, port scanning reads `/proc/net/tcp` directly and resolves socket inodes to PIDs via `/proc/<pid>/fd/` -- no external commands needed. This is significantly faster than the traditional `lsof` approach.

On **macOS**, uses `lsof` for port scanning (no `/proc` filesystem available).

Both platforms use `ps` for process details and `docker ps` for container identification.

Framework detection reads `package.json` dependencies and inspects process command lines. Recognizes 30+ frameworks including Next.js, Vite, Express, Angular, Django, Rails, FastAPI, and more.

## Platform support

| Platform | Status |
|----------|--------|
| Linux    | Supported (fastest -- direct `/proc` reads) |
| macOS    | Supported |
| Windows  | Not planned |

## License

[MIT](LICENSE)
