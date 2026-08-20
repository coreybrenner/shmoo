# Plan 9 Interoperability — Design Study

**Last updated:** 2026-08-20  
**Purpose:** Document the case for Shmoo interoperability with Plan 9 (specifically the 9front community distribution), and outline how Shmoo can become a bridge for spreading Plan 9's design philosophy to modern systems.

---

## 1. The Vision

> "It's a little like spreading Plan 9 around, which is laudable."

This study proposes that **Shmoo should be a bridge between Plan 9 and modern systems** — a build system that speaks 9P natively, runs Perl natively, and makes Plan 9's design philosophy accessible to builders everywhere.

**The goal:** Make Shmoo a tool that works on Linux, macOS, Windows, OS/2, **and** Plan 9, where the Protocol 9P is the universal transport layer for filesystem operations.

---

## 2. Plan 9: A Brief Overview

### 2.1 What is Plan 9?

Plan 9 from Bell Labs (1988) was a distributed operating system designed by the same team that created Unix (Ken Thompson, Rob Pike, etc.). Its key innovations:

1. **Everything is a file** — filesystems, devices, processes, networks, the kernel itself
2. **Protocol 9P** — a simple request-response protocol for remote filesystem access
3. **Composable namespaces** — every process has its own view of the filesystem
4. **Network transparency** — remote and local resources are accessed identically
5. **UTF-8 by default** — 20 years before Linux adopted it
6. **The 8/9-bit architecture** — a clean, orthogonal instruction set

### 2.2 Plan 9 Today

The original Bell Labs team disbanded in the late 1990s, but the community continues through **9front** (`9front.org`), a living distribution of Plan 9 with active development.

Other derivatives include:
- **9front** — Community continuation (most active)
- **Plan 9 from User Space (9fans)** — Run Plan 9 on Linux/macOS/Windows
- **Mk — Plan 9's build system** — A recursive, data-driven build system that inspires Shmoo
- **rc shell** — A simple, elegant Unix shell
- **Samply (sam, akavi, etc.)** — Advanced text editors
- **Drawterm** — Mount 9P filesystems from any platform

### 2.3 9P (Protocol 9)

9P is the **native filesystem protocol** of Plan 9. It defines a simple, request-response interface for remote filesystem operations:

| Operation | Description |
|-----------|-------------|
| `Tversion` / `Rversion` | Negotiate protocol version |
| `Tauth` / `Rauth` | Authentication (username/password, or shared secret) |
| `Tattach` / `Rattach` | Attach to a filesystem root (like `mount`) |
| `Twalk` / `Rwalk` | Navigate the directory tree (return a file descriptor) |
| `Tstat` / `Rstat` | Get file attributes (size, mtime, mode, UID, GID) |
| `Twstat` / `Rwstat` | Set file attributes |
| `Topen` / `Ropen` | Open a file (get a file descriptor) |
| `Tclose` / `Rclose` | Close a file descriptor |
| `Tread` / `Rread` | Read data from a file |
| `Twrite` / `Rwrite` | Write data to a file |
| `Tcreate` / `Rcreate` | Create a file or directory |
| `Tremove` / `Rremove` | Delete a file or directory |
| `Tflush` / `Rflush` | Cancel a pending request |
| `Tmkdir` / `Rmkdir` | Create a directory |
| `Tsymlink` / `Rsymlink` | Create a symbolic link |
| `Tmknod` / `Rmknod` | Create a special file |
| `Trename` / `Rrename` | Rename a file or directory |
| `Treadlink` / `Rreadlink` | Read the target of a symbolic link |

**9P2000** is the current version, supported by:
- Linux kernel (`9p.ko`, v9fs client)
- Plan 9 and 9front (native)
- 9fans (plan9port — Plan 9 on Linux/macOS/Windows)
- Drawterm (mount 9P filesystems from any platform)
- `lib9p` (C library, cross-platform)
- `9P.net` (Go implementation, cross-platform)

---

## 3. The Synergy: Shmoo × Plan 9

### 3.1 Why Plan 9 is a Natural Fit

| Shmoo Concept | Plan 9 Equivalent | Notes |
|---------------|-------------------|-------|
| 9P as the VFS protocol | 9P is the native filesystem protocol | Perfect alignment |
| Per-process mount maps | Per-process namespaces | Identical model |
| Network-transparent builds | Network-transparent filesystems | Exactly the same goal |
| "Everything is a file" | Everything is a file | Shared philosophy |
| Recursive build system | `mk` — recursive build system | Direct inspiration |
| Perl-based build system | Perl 5 on 9front | Native support |
| Standard Root isolation | Namespace composition | Identical mechanism |
| Daemon/Interceptor model | 9P server/client | Same architecture |

### 3.2 The "Spread Plan 9 Around" Vision

Shmoo can act as a **bridge** — bringing Plan 9's design philosophy to modern systems while making Plan 9 a practical platform for modern development:

**For modern systems (Linux, macOS, Windows):**
- Learn from Plan 9's clean design
- Use 9P as the universal filesystem protocol
- Adopt Plan 9's namespace model for build isolation
- Use Plan 9's build tools (`mk`) as inspiration for Shmoo

**For Plan 9:**
- Modern build system (Shmoo) written in Perl (native to 9front)
- 9P interoperability with non-Plan 9 systems
- Shmoo can be the "gateway drug" — getting builders interested in Plan 9's approach

---

## 4. Perl on Plan 9

### 4.1 Perl 5 on 9front

Perl 5 is actively maintained on 9front and is a **first-class scripting language** on the platform:

```bash
# On 9front, Perl is installed and ready
$ perl -v

This is perl 5, version 38, subversion 0 (v5.38.0)
...
```

### 4.2 Perl Modules on Plan 9

Key modules available on 9front:
- **Net::9P** — Native 9P client and server in Perl
- **IO::P9** — Filesystem operations via 9P
- **IO::Pseudo** — Pseudofiles (like Plan 9's `/dev` files)
- **IO::Pipe** — Standard pipes, plus 9P pipes
- **Socket** — Network sockets
- **POSIX** — POSIX compatibility layer
- **File::Temp**, **File::Path**, etc. — Standard core modules

### 4.3 Net::9P: Perl's 9P Interface

The **Net::9P** module (by David Golden, on CPAN and in the Plan 9 community) provides a complete Perl interface to the 9P protocol:

```perl
use Net::9P;

# Connect to a 9P server (Daemon)
my $client = Net::9P->connect('tcp!localhost!5640');

# Authenticate with a shared secret
$client->auth('corey', 'my-secret-password');

# Attach to the 9P root (like mount)
my $root = $client->attach('/');

# Navigate the namespace
my $include_dir = $root->walk('include');

# Stat a file
my $stat = $include_dir->stat('math.h');

# Read a file
my $content = $include_dir->read('math.h');

# Write a file
$include_dir->write('output.h', $content . "\n// added by Shmoo");

# Walk the tree
my @files = $include_dir->walk('');

# Close
$client->close();
```

This means **Shmoo can talk 9P natively in Perl** — no adapter layer needed. The build orchestrator is Perl, the build scripts are Perl, and the VFS comms are 9P in Perl.

---

## 5. Plan 9's `mk` Build System

### 5.1 What is mk?

`mk` is Plan 9's recursive build system, designed by Tom Duff. It is **the direct ancestor** of Shmoo's build model:

| Feature | `mk` | Shmoo |
|---------|------|-------|
| **Rule format** | Plain text data files | Plain text data files |
| **Dependency tracking** | Recursive, file-based | Recursive, VFS-aware |
| **Build model** | Top-down, recursive | Top-down, recursive, network-aware |
| **Environment** | Unix commands | VFS-aware commands |
| **Language** | Shell commands + data files | Perl + VFS data files |
| **Portability** | Unix | Linux, macOS, Windows, OS/2, Plan 9 |

### 5.2 How `mk` Works

```mk
# mkfile (the build rules)
all:
    mk $SRCDIR

$SRCDIR: *.c *.h
    gcc -c $@

main: main.o math.o
    gcc -o main main.o math.o
```

`mk` reads `mkfile` (like Make reads `Makefile`), recursively processes dependencies, and builds the target tree. It's **simple, elegant, and powerful** — exactly the kind of build system we want to extend.

### 5.3 Shmoo's `mk` Integration

Shmoo can inherit `mk`'s rule format while adding its own features:

```mk
# mkfile (Shmoo-style)
all: VFS:SRC:main

VFS:SRC:main: *.c *.h
    gcc -c $@

main: main.o math.o
    gcc -o main main.o math.o

# Shmoo can add custom rules:
VFS:SRC:headers: *.h
    shmvfs:generate $@  # Custom Shmoo command via 9P
```

The `shmvfs` command would be a Perl script that uses `Net::9P` to talk to the Daemon, making the build rules VFS-aware.

---

## 6. Implementation Roadmap

### 6.1 Phase 1: 9P Client in Perl

Make Shmoo's build orchestrator speak 9P natively:

```perl
# /root/shmoo/lib/Shmoo/VFS/Client/9P.pm

package Shmoo::VFS::Client::9P;
use strict;
use warnings;
use Net::9P;

sub new {
    my ($class, $daemon_addr) = @_;
    my $client = Net::9P->connect($daemon_addr)
        or die "Cannot connect to Daemon: $!";

    return bless {
        client    => $client,
        root      => undef,
        namespace => {},  # Per-process mount table
    }, $class;
}

sub auth {
    my ($self, $user, $secret) = @_;
    my $r = $self->{client}->auth($user, $secret)
        or die "Auth failed: " . $self->{client}->error();

    return $r;
}

sub attach {
    my ($self, $mount_point) = @_;
    my $r = $self->{client}->attach($mount_point)
        or die "Attach failed: " . $self->{client}->error();

    return $r;
}

sub walk {
    my ($self, $path) = @_;
    my $r = $self->{client}->walk($path)
        or die "Walk failed: " . $self->{client}->error();

    return $r;
}

sub stat {
    my ($self, $path) = @_;
    my $r = $self->{client}->stat($path)
        or die "Stat failed: " . $self->{client}->error();

    return $r;
}

sub read {
    my ($self, $path) = @_;
    my $r = $self->{client}->read($path)
        or die "Read failed: " . $self->{client}->error();

    return $r;
}

sub write {
    my ($self, $path, $data) = @_;
    my $r = $self->{client}->write($path, $data)
        or die "Write failed: " . $self->{client}->error();

    return $r;
}

sub close {
    my ($self) = @_;
    $self->{client}->close();
}
```

### 6.2 Phase 2: 9P Server in Perl

Make Shmoo's Daemon a 9P server:

```perl
# /root/shmoo/lib/Shmoo/VFS/Server/9P.pm

package Shmoo::VFS::Server::9P;
use strict;
use warnings;
use IO::Socket::INET;

sub new {
    my ($class, $port) = @_;
    my $server = IO::Socket::INET->new(
        LocalPort => $port,
        Proto     => 'tcp',
        Listen     => 5,
        Reuse     => 1,
    ) or die "Cannot bind to port $port: $!";

    return bless {
        server => $server,
        clients => [],
        namespace => {},  # Global namespace
    }, $class;
}

sub run {
    my ($self) = @_;

    while (my $client = $self->{server}->accept()) {
        my $peer = $client->peerhost();
        print "New client: $peer\n";

        # Handle the client in a new thread or process
        $self->{clients}++[0] = ClientWorker->new($client, $self->{namespace});
        $self->{clients}[0]->run();

        # Clean up dead clients
        @$self->{clients} = grep { $_->alive() } @$self->{clients};
    }
}
```

### 6.3 Phase 3: Plan 9 Native Build

Run the entire Shmoo build system natively on Plan 9:

```bash
# On Plan 9 (9front):

# Install the shared secret
echo 'my-shared-secret' > /tmp/build-secret

# Start the 9P Daemon
perl /root/shmoo/shmoo-daemon.pl --port 5640 --secret /tmp/build-secret &

# Run a build
perl /root/shmoo/shmoo-build.pl /mnt/disc/src/shmoo

# The build runs with:
# - Perl 5 natively on Plan 9
# - 9P protocol to the Daemon (same machine or remote)
# - Plan 9's namespace model for isolation
```

### 6.4 Phase 4: Cross-Platform 9P

Make Shmoo interoperable across all platforms:

| Platform | 9P Client | 9P Server | Perl | Shmoo Status |
|----------|-----------|-----------|------|--------------|
| **Linux** | `lib9p` or `9p.ko` | `lib9p` | Core | ✅ Native |
| **macOS** | `lib9p` (9fans) | `lib9p` | Core | ✅ Via 9fans |
| **Windows** | `lib9p` (9fans) | `lib9p` | ActivePerl/Strawberry | ✅ Via 9fans |
| **Plan 9** | `Net::9P` (Perl) | `Net::9P` (Perl) | Core | ✅ Native |
| **OS/2** | `lib9p` (if available) | `lib9p` | ? | ❌ Depends |

---

## 7. Benefits of Plan 9 Interoperability

### 7.1 Technical Benefits

1. **9P is the best protocol** — Simple, elegant, proven by decades of use
2. **Native Perl support** — No adapter layer between build system and VFS
3. **Composable namespaces** — Per-process isolation that works everywhere
4. **Network transparency** — Remote and local resources are identical
5. **Recursive builds** — `mk` is the ancestor of Shmoo's build model
6. **Cross-platform** — 9P works on Linux, macOS, Windows, Plan 9, OS/2, Amiga

### 7.2 Cultural Benefits

1. **Spreading the word** — Builders using Shmoo will discover Plan 9
2. **New users** — Shmoo becomes a gateway to Plan 9's design philosophy
3. **Community** — Plan 9 community can contribute to Shmoo (they love Perl and 9P)
4. **Preservation** — Plan 9's innovations live on in Shmoo

### 7.3 Unique Differentiator

**No other build system does this:**
- Docker: Linux only, kernel namespaces
- Bazel: Linux/macOS/Windows, but no 9P
- Nix: Linux/macOS only, no 9P
- CMake: Everything, but no 9P
- Make: Everything, but no 9P
- **Shmoo: 9P native, Perl native, Plan 9 native**

This is a **unique selling point** that puts Shmoo in a category of its own.

---

## 8. Comparison: Shmoo vs. Plan 9's `mk`

| Feature | `mk` (Plan 9) | Shmoo (extended) |
|---------|---------------|------------------|
| Rule format | Plain text data files | Plain text data files + Perl hooks |
| Dependencies | File-based | VFS-aware (networked) |
| Environment | Unix commands | VFS-aware commands (9P) |
| Isolation | Per-process namespaces | Per-build mount maps + namespaces |
| Network | Local only | Remote 9P support |
| Language | Shell commands | Perl + 9P |
| Portability | Unix/Plan 9 | Linux, macOS, Windows, OS/2, Plan 9 |
| Build model | Top-down, recursive | Top-down, recursive, network-aware |

---

## 9. The 9P-to-Shmoo Translation Layer

If we want Shmoo to talk to non-Plan 9 9P servers (e.g., Linux's v9fs client), we need a **translation layer**:

```
┌─────────────────────────────────────────────┐
│              Shmoo Build Orchestrator         │
│  (Perl, Shmoo::VFS::Client::9P)              │
└────────┬────────────────────────────────────┘
         │ 9P Protocol (Net::9P)
         ▼
┌─────────────────────────────────────────────┐
│              Shmoo Daemon (9P Server)         │
│  (Perl, Shmoo::VFS::Server::9P)              │
│                                              │
│  ├── Canonical Mount Tree                     │
│  ├── Version Tracking                         │
│  ├── Audit Trail Logging                      │
│  └── Authentication (Shared Secret)           │
└────────┬────────────────────────────────────┘
         │ 9P Protocol
         ▼
┌─────────────────────────────────────────────┐
│          Shmoo Interceptor (9P Client)       │
│  (LD_PRELOAD / DLL injection)                │
│                                              │
│  ├── Path Translation                         │
│  ├── Cache Invalidation (Lazy + Push)        │
│  ├── Standard Root Isolation                  │
│  └── Build Map Enforcement                    │
└────────┬────────────────────────────────────┘
         │ 9P to Real Filesystem
         ▼
┌─────────────────────────────────────────────┐
│            Real Filesystem                    │
│  (ext4, NTFS, FAT, HFS+, etc.)              │
└─────────────────────────────────────────────┘
```

The **translation layer** is in the Shmoo Daemon, which acts as a **9P-to-anything gateway**:
- A `Tread` request for `SRC:/math.h` becomes a read from the actual filesystem location
- A `Twrite` request for `SRC:/main.o` becomes a write to the actual filesystem location
- The Daemon handles all the translation, caching, and auditing

---

## 10. Remote Execution: r-shell and /sys/exec

### 10.1 The r-shell Mechanism

Plan 9 has a tool called **`r-shell`** (or just **`rs`**) that allows you to execute commands on a remote 9P server:

```bash
# Run a command on the remote node:
r-shell remotehost "gcc -o math math.c"

# Equivalent to:
ssh remotehost "gcc -o math math.c"
```

### 10.2 How 9P Handles Remote Execution

The process works like this:

1. **Client initiates 9P connection** to the remote server (just like mounting a filesystem).
2. **Client sends a special 9P operation** (`Topen`) to the special file `/sys/exec` on the remote server.
3. **Client `write()`s the command** (command + arguments) to the `/sys/exec` file descriptor.
4. **Remote server forks a process** in the client's namespace.
5. **Client `read()`s the output** from the same file descriptor — the server pipes stdout/stderr back over 9P.
6. **Client `close()`s** the file descriptor and receives the exit status.

**The key insight:** Remote execution is just a file operation over 9P. The client `open()`s the remote `/sys/exec` endpoint, `write()`s the command, and `read()`s the output. No separate protocol layer — 9P handles everything.

### 10.3 The /sys/exec File

Every 9P node exposes a special file called **`/sys/exec`** that serves as the remote execution endpoint. It is a **file-like interface to process execution**:

```
┌─────────────────────────────────────────────┐
│              Client (Interceptor)            │
│  Net::9P or lib9p                            │
└────────┬────────────────────────────────────┘
         │ 9P Protocol (Topen, Twrite, Tread)
         ▼
┌─────────────────────────────────────────────┐
│  Remote Node 9P Server                       │
│                                              │
│  /sys/exec  ← Special 9P file               │
│    ├── Topen("/sys/exec") → fd=42            │
│    ├── Twrite(fd=42, "gcc -o math math.c")  │
│    ├── Fork process in client namespace      │
│    ├── Exec("gcc", args=["-o", "math", "math.c"])
│    ├── Pipe stdout/stderr back over 9P       │
│    ├── Tread(fd=42) → output                 │
│    └── Tclose(fd=42) → exit status           │
└─────────────────────────────────────────────┘
```

### 10.4 Why This Matters for Shmoo

This is **exactly** the model we want for Shmoo's build system:

| Shmoo Goal | Plan 9 Equivalent |
|------------|-------------------|
| Run build steps on remote nodes | `r-shell` sends commands over 9P |
| Distribute builds across nodes | 9P handles the dispatch automatically |
| Namespace-aware execution | Remote processes inherit client's namespace |
| Remote compiler execution | `r-shell remotehost "gcc ..."  |
| Remote file I/O | All done via 9P `Tread`/`Twrite` |

### 10.5 Dispatch Model

In Plan 9, the **dispatch** (distributing work across nodes) is handled by the **build system** (`mk`) using 9P as the transport layer:

```mk
# mkfile (mk build system)
all: $O/all

# The 'r-shell' tool is called by mk to distribute builds:
O/all: C/src/main.c
    r-shell worker1 "gcc -o $@ $<"  # Send to remote node
    r-shell worker2 "gcc -o $@ $<"  # Send to another node
```

The build system (`mk`) is responsible for *which* node gets the work. 9P is the *transport layer* that makes it possible.

### 10.6 Shmoo's Equivalent

Shmoo can replicate this exact model with its Daemon/Interceptor architecture:

```
┌─────────────────────────────────────────────┐
│          Shmoo Build Orchestrator             │
│  (Perl, mk-compatible)                       │
└────────┬────────────────────────────────────┘
         │ 9P Protocol (Topen, Twrite, Tread)
         ▼
┌─────────────────────────────────────────────┐
│  Build Node 9P Server (Interceptor)          │
│                                              │
│  /sys/exec  ← Special 9P file               │
│    ├── Topen("/sys/exec") → fd=42            │
│    ├── Twrite(fd=42, "gcc -o math math.c")  │
│    ├── Fork process in client namespace      │
│    ├── Exec("gcc", args=["-o", "math", "math.c"])
│    ├── Pipe stdout/stderr back over 9P       │
│    ├── Tread(fd=42) → output                 │
│    └── Tclose(fd=42) → exit status           │
│                                              │
│  / → Shmoo VFS (mounted via 9P)              │
│    /SRC: → Project sources (virtual)         │
│    /OBJ: → Build artifacts (virtual)         │
│    /SYS: → System headers (virtual)          │
└─────────────────────────────────────────────┘
```

The build orchestrator calls `Topen("/sys/exec")` on each node, writes the build command, and reads the output. The VFS is mounted separately via 9P, so the remote process has access to the same namespace as the client.

---

## 11. mk Emulation and Shmoo Interoperability

### 11.1 The Vision: mk × Shmoo

The goal is **bidirectional interoperability**:

1. **mk can use Shmoo nodes** — Plan 9's `mk` build system can dispatch build steps to Shmoo nodes (which speak 9P natively).
2. **Shmoo can emulate mk** — Shmoo can parse and execute `mk` rules (`mkfile`/`Makefile`) directly, making it a drop-in replacement for `mk`.
3. **Shmoo can participate in mk builds** — A Shmoo node can join an existing `mk` build as a remote worker, using 9P for communication.

This would make Shmoo **fully compatible with the Plan 9 ecosystem** while adding its own VFS-aware capabilities.

### 11.2 mk Rule Format

`mk` uses a simple, declarative rule format that is easy to parse and emulate:

```mk
# mkfile (Plan 9 build rules)
all: math

math: math.c math.h
    gcc -o math math.c

%.o: %.c
    gcc -c -o $@ $<
```

Key features:
- **Target:** `target: dependencies`
- **Command:** Indented line with the shell command to execute
- **Pattern rules:** `%.o: %.c` — automatic dependency matching
- **Variables:** `$@` (target), `$<` (first dependency), `$^` (all dependencies)
- **Implicit rules:** Built-in rules for `.c`, `.s`, `.o`, etc.

### 11.3 Shmoo mk-Emulator

Shmoo can include a **mk-compatible rule parser** that understands `mkfile` and executes the rules using Shmoo's VFS capabilities:

```perl
# /root/shmoo/lib/Shmoo/mk/Emulator.pm

package Shmoo::mk::Emulator;
use strict;
use warnings;

sub parse_mkfile {
    my ($class, $file) = @_;
    my $rules = [];
    my $current_target = undef;

    open(my $fh, '<', $file) or die "Cannot open mkfile: $!";
    while (my $line = <$fh>) {
        chomp $line;

        # Blank line or comment — reset current target
        if ($line =~ /^\s*$/ || $line =~ /^\s*\#/) {
            $current_target = undef;
            next;
        }

        # Rule definition: target: dependencies
        if ($line =~ /^([^\t]+):\s*(.*)$/) {
            $current_target = { target => $1, deps => [split(/\s+/, $2)] };
            push @$rules, $current_target;
            next;
        }

        # Command line (indented with tab)
        if ($line =~ /^\t(.*)$/ && $current_target) {
            $current_target->{commands} //= [];
            push @{$current_target->{commands}}, $1;
        }
    }

    return $rules;
}

sub execute {
    my ($class, $rules, $target) = @_;

    for my $rule (@$rules) {
        if ($rule->{target} eq $target) {
            # Execute commands
            for my $cmd (@{$rule->{commands}}) {
                # Expand variables ($@, $<, $^)
                $cmd =~ s/\$@/$rule->{target}/g;
                $cmd =~ s/\$<\b/$rule->{deps}[0]/g;
                $cmd =~ s/\$\^\b/ join(" ", @{$rule->{deps}}) /ge;

                # Execute the command via Shmoo's VFS
                my $result = Shmoo::VFS::run_command($cmd);

                if ($result->{exit_code} != 0) {
                    die "Command failed: $cmd (exit code $result->{exit_code})\n";
                }
            }
            return { success => 1 };
        }
    }

    die "No rule for target: $target\n";
}

sub run_build {
    my ($class, $mkfile, $target) = @_;

    my $rules = $class->parse_mkfile($mkfile);

    for my $rule (@$rules) {
        if ($rule->{target} eq $target) {
            # Check dependencies
            for my $dep (@{$rule->{deps}}) {
                if (!-e $dep) {
                    $class->execute($rules, $dep);  # Build dependency first
                }
            }
            # Execute target
            return $class->execute($rules, $target);
        }
    }
}
```

### 11.4 Shmoo Participating in mk Builds

A Shmoo node can act as a **remote worker** for an existing `mk` build by exposing a 9P server with `/sys/exec` support:

```bash
# On a Shmoo node (any platform):

# Start the Shmoo Daemon with mk-remote mode
shmoo-daemon --mode=mk-worker --bind tcp:0.0.0.0:5640

# Now the node is available to mk builds:
# On the build orchestrator (Plan 9 or other):
mkfile: C/src/main.c
    r-shell shmoo-node "gcc -o $@ $<"  # Dispatch to Shmoo node via 9P

# The Shmoo node:
# 1. Receives the command via 9P (Twrite on /sys/exec)
# 2. Forks a process with Shmoo's VFS namespace
# 3. Executes the command (gcc)
# 4. Pipes output back to the orchestrator via 9P (Tread)
# 5. Returns exit status
```

The Shmoo node can also **enhance mk's capabilities** by providing:
- **VFS-aware compilation** — The remote gcc can access Shmoo's virtual directories (`/SRC:`, `/OBJ:`)
- **Remote caching** — Build artifacts can be cached across nodes via the Shmoo Daemon
- **Namespace isolation** — Each build runs in its own isolated namespace

### 11.5 mkfile → Shmoo Rule Translation

Shmoo can also translate `mkfile` rules into its own build format, making it easy to migrate:

```mk
# Original mkfile (Plan 9)
all: math

math: math.c math.h
    gcc -o math math.c

%.o: %.c
    gcc -c -o $@ $<
```

```mk
# Translated to Shmoo (mk-compatible)
all: VFS:OBJ:math

VFS:OBJ:math: VFS:SRC:math.c VFS:SRC:math.h
    gcc -o VFS:OBJ:math VFS:SRC:math.c VFS:SRC:math.h

%.o: %.c
    gcc -c -o VFS:OBJ:$*.o VFS:SRC:$*.c

# Shmoo adds VFS-aware dependencies automatically
```

### 11.6 Benefits of Bidirectional Interoperability

| Benefit | Description |
|---------|-------------|
| **Drop-in replacement** | Shmoo can replace `mk` on Plan 9 without changing `mkfile` rules |
| **New capabilities** | Shmoo adds VFS awareness to existing `mk` builds |
| **Remote workers** | `mk` builds can dispatch to Shmoo nodes anywhere |
| **Cross-platform** | Shmoo nodes can run on Linux, macOS, Windows, Plan 9 |
| **Community bridge** | Plan 9 builders discover Shmoo through `mk` compatibility |
| **Preservation** | Plan 9's build system lives on in Shmoo |

### 11.7 Implementation Priority

| Priority | Task | Effort |
|----------|------|--------|
| **P0** | mk-compatible rule parser in Perl | Medium (3-5 days) |
| **P1** | Expose `/sys/exec` on Shmoo Daemon for `r-shell` | Medium (3-5 days) |
| **P2** | mkfile → Shmoo rule translator | Small (1-2 days) |
| **P3** | Shmoo node as `mk` remote worker | Medium (2-3 days) |
| **P4** | Full `mk` emulation with implicit rules | Large (1-2 weeks) |

### 11.8 Example: mk Build with Shmoo Nodes

```bash
# Plan 9 build orchestrator (9front):

# Start a Shmoo node on a Linux build server
ssh build1 "shmoo-daemon --mode=mk-worker --bind tcp:0.0.0.0:5640"

# Now the orchestrator can dispatch to the Shmoo node:
mkfile:
    r-shell build1 "gcc -o math math.c"  # Runs on Linux, uses Shmoo VFS
    r-shell build2 "gcc -o test test.c"  # Runs on another node

# The Shmoo node:
# 1. Receives "gcc -o math math.c" via 9P
# 2. Forks process with Shmoo's VFS namespace (SRC:, OBJ:, etc.)
# 3. Executes gcc on the Linux node
# 4. Pipes output back to the orchestrator
# 5. Returns exit status
```

---

## 13. Conclusion

Plan 9 interoperability is not just "cool" — it is a **strategic advantage** for Shmoo. The 9P protocol is the best filesystem protocol in existence, Plan 9's design philosophy is elegant and proven, and Perl's native support on 9front makes the integration seamless.

**The vision:** Shmoo as the bridge between Plan 9 and the modern world — a build system that speaks 9P natively, runs Perl natively, and makes Plan 9's design philosophy accessible to builders everywhere.

**The result:** A unique build system that no one else has, with a community that spans from Plan 9 enthusiasts to modern build engineers.

> "It's a little like spreading Plan 9 around, which is laudable."

**Let's do it.**

---

## 11. Sources

- **9front** — `https://9front.org/` (community continuation of Plan 9)
- **Plan 9 from Bell Labs** — `https://plan9.bell-labs.com/plan9.html`
- **Protocol 9P** — `https://9p.cat-v.org/doc/9p.html` (primary source)
- **Net::9P (Perl module)** — CPAN: `Net::9P` by David Golden
- **mk (Plan 9 build system)** — Built into 9front, documentation at `https://9front.org/`
- **Plan 9 from User Space (9fans)** — `https://9fans.github.io/plan9port/`
- **Linux v9fs** — `https://www.kernel.org/doc/html/latest/filesystems/9p.html`
- **Drawterm** — `https://9fans.github.io/plan9port/` (mount 9P filesystems from any platform)
