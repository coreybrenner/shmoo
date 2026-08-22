# Design: Distributed Build Host Architecture

*2025-01-23 | Status: Active Design*
*Purpose: Distributed build system where each parallel host runs its own daemon managing local job slots, servicing 9p requests, and maintaining a local event log. A global mission coordinator assigns work across hosts. Event logs can be monolithic, per-host, or per-mission depending on operational requirements.*

---

## 1. Architecture Overview

```
┌───────────────────────────────────────────────────────────────────────────┐
│                           Mission Coordinator                             │
│                                                                           │
│  ┌───────────────────────────────────────────────────────────────────┐   │
│  │  Mission 0xA3F1 (Build #4821, "all")                              │   │
│  │  ┌───────────────────────────────────────────────────────────┐    │   │
│  │  │  Global Event Log Manager                                 │    │   │
│  │  │  ├─ Monolithic mode: one joined log                     │    │   │
│  │  │  ├─ Per-host mode: one log per host, indexed by mission │    │   │
│  │  │  └─ Hybrid mode: per-host logs + global index           │    │   │
│  │  └───────────────────────────────────────────────────────────┘    │   │
│  │                                                                     │   │
│  │  ┌───────────────────────────────────────────────────────────┐    │   │
│  │  │  Job Scheduler (Top-level)                                │    │   │
│  │  │  ├─ DAG resolver: parses build graph                      │    │   │
│  │  │  ├─ Topological sort: determines execution order          │    │   │
│  │  │  ├─ Job assignment: maps independent nodes to hosts       │    │   │
│  │  │  └─ Slot management: tracks available slots per host      │    │   │
│  │  └───────────────────────────────────────────────────────────┘    │   │
│  └───────────────────────────────────────────────────────────────────┘   │
│                                                                             │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐  ┌────────────┐           │
│  │ Host Daemon │  │ Host Daemon │  │ Host Daemon │  │ Host Daemon │           │
│  │  A         │  │  B         │  │  C         │  │  D         │           │
│  │ (Linux)    │  │ (Win10)    │  │ (WSL2)     │  │ (macOS)    │           │
│  └─────┬──────┘  └─────┬──────┘  └─────┬──────┘  └─────┬──────┘           │
│        │               │               │               │                    │
│  ┌─────┴──────┐  ┌─────┴──────┐  ┌─────┴──────┐  ┌─────┴──────┐           │
│  │Local Jobs  │  │Local Jobs  │  │Local Jobs  │  │Local Jobs  │           │
│  │Slot 0,1,2,3│  │Slot 0,1    │  │Slot 0,1,2  │  │Slot 0,1,2  │           │
│  └────────────┘  └────────────┘  └────────────┘  └────────────┘           │
└───────────────────────────────────────────────────────────────────────────┘
```

### Components

| Component | Role |
|-----------|------|
| **Mission Coordinator** | Top-level scheduler, DAG resolver, global log manager. Runs on a "lead" host. |
| **Host Daemon** | Per-host process managing local job slots, 9p server, local event log, and communication with the coordinator. |
| **Local Jobs** | Individual build actions (compile, link, etc.) executing on a host. |
| **Global Event Log** | The build trace, stored as monolithic, per-host, or hybrid depending on configuration. |

---

## 2. Mission Concept

A **Mission** is a single build invocation that may span multiple hosts. Each mission has:

- **Mission ID:** Unique identifier (UUID or epoch-based)
- **Build graph:** The DAG of all actions to be executed
- **Host roster:** List of hosts participating in this mission
- **Job assignments:** Which host runs which action
- **Event log scope:** Whether logs are monolithic or per-host
- **Lifecycle:** `planned` → `running` → `completed` (or `failed`)

### Mission Lifecycle

```
planned → [assign jobs to hosts] → running → [complete/fail] → completed/failed
```

The coordinator:
1. Parses the build graph (from `shmoo-build` files, Makefiles, or recipe book)
2. Computes the topological sort
3. Assigns independent jobs to available host slots
4. Monitors job progress via the event log
5. On failure, decides whether to retry, refuel, or abort

### Mission Configuration

```yaml
mission:
  id: "0xA3F1"
  name: "Build #4821"
  log_mode: "hybrid"          # monolithic | per-host | hybrid
  log_join_delay_ms: 500      # How often to flush local logs to global
  max_host_slots: 16          # Max parallel jobs per host (global default)
  hosts:
    - host: "alpha.local"
      slots: 8
      type: "linux-x86_64"
      9p_port: 5640
      9p_unix: "/tmp/9p-alpha.sock"
    - host: "beta.local"
      slots: 4
      type: "win10-x86_64"
      9p_port: 5641
      9p_unix: "\\\\.\\pipe\\9p-beta"
```

---

## 3. Event Log Organization

Three storage strategies are supported. The choice depends on operational requirements:

### Option A: Monolithic Log

All hosts' events are joined into a single log, ordered by global EID.

```
Global Event Log (monolithic):
┌──────────┬───────┬──────────┬──────────┬──────────┐
│ EventNum │ Type  │  Length  │  Payload │  Checksum│
│  (8B)    │ (1B)  │  (4B LE) │ Variable │  (4B)    │
├──────────┼───────┼──────────┼──────────┼──────────┤
│ 0        │ META  │ ...      │ Mission  │ ...      │
│          │       │          │ = 0xA3F1 │          │
│ 1000001  │ ENV_SET│ ...     │ CFLAGS   │ ...      │
│          │       │          │ = -O2    │          │
│ 2000001  │ PROC  │ ...      │ gcc -c  │ ...      │
│          │ EXEC  │          │ main.c   │          │
│ 3000001  │ FILE  │ ...      │ app.o   │ ...      │
│          │ WRITE │          │ [data]   │          │
│ ...      │ ...   │ ...      │ ...      │ ...      │
└──────────┴───────┴──────────┴──────────┴──────────┘
```

**Properties:**
- Single log file (or stream) — easy to manage
- Replay is straightforward: read log from EID 0 to target EID
- Global clock assignment by coordinator (EIDs are assigned centrally)
- No ambiguity about ordering — the coordinator is the single source of truth for ordering

**Pros:**
- Simplest replay engine (single log to read)
- Clear total ordering of all events
- Easiest "core file examination" — one log, one timeline
- Speculative roll-forward works naturally

**Cons:**
- Coordinator is a bottleneck (all events pass through it)
- If the coordinator fails, the entire mission is at risk
- Log file can become very large (millions of events × multiple hosts)
- Hosts must send events to coordinator in order (requires coordination)

**Best for:** Missions with fewer than ~20 hosts, or where replay simplicity is paramount.

---

### Option B: Per-Host Logs

Each host maintains its own independent log file.

```
Global Log Directory (per-host):
/build/misiones/0xA3F1/
├── global_meta.json     # Mission metadata, host roster
├── host-alpha.log       # Host A's log (EIDs 1000000+)
├── host-beta.log        # Host B's log (EIDs 2000000+)
├── host-gamma.log       # Host C's log (EIDs 3000000+)
└── global_index.json    # Index: EID → (host, offset)
```

**Properties:**
- Each host writes to its own log file independently (no coordination)
- EID ranges are disjoint per host (host A: 1000000–1999999, host B: 2000000–2999999, etc.)
- A global index maps EID ranges to hosts

**Pros:**
- Fully distributed — hosts never block on each other or the coordinator
- Host can crash without affecting other hosts' logging
- Easier to manage log rotation (per-host log files)
- Replay of individual host state is simple (read host's log)

**Cons:**
- Global replay requires reading multiple log files and merging
- Speculative roll-forward must replay from multiple logs
- "Core file examination" at a specific EID requires knowing which host's log to read
- Global ordering requires the index

**Best for:** Large-scale distributed builds (> 20 hosts), fault tolerance is critical, and per-host analysis is valuable.

---

### Option C: Hybrid Log (Recommended Default)

A compromise: per-host log files for fault tolerance and locality, but the coordinator periodically merges them into a global index for replay convenience.

```
/build/misiones/0xA3F1/
├── global_meta.json     # Mission metadata, host roster, config
├── index.json           # EID range → host mapping
│   {
│     "1000000-1999999": "host-alpha",
│     "2000000-2999999": "host-beta",
│     "3000000-3999999": "host-gamma"
│   }
├── host-alpha.log       # Host A's log file
├── host-beta.log        # Host B's log file
├── host-gamma.log       # Host C's log file
├── checkpoints/
│   ├── cp_1000000.json  # Checkpoint at EID 1000000 (state snapshot)
│   ├── cp_2000000.json  # Checkpoint at EID 2000000
│   └── cp_3000000.json  # Checkpoint at EID 3000000
└── global_stream.log    # Optional: periodically flushed global stream
```

**Properties:**
- Hosts write to their own log files (like per-host mode)
- The coordinator periodically merges local logs and writes them to a global stream
- Global index + checkpoints enable efficient replay
- Logs can be flushed incrementally (every N seconds or every N events)

**Pros:**
- Best of both worlds: distributed logging + global replay
- Checkpoints allow replay from the last checkpoint (not from EID 0)
- Global stream can be used for live monitoring (event log viewer)
- Per-host logs are still available for local debugging
- Flexible: coordinator can be slow/intermittent — host logs still accumulate

**Cons:**
- More complex than monolithic mode (index management, checkpoints)
- If the coordinator is down, global stream isn't updated (but local logs still work)
- Need to handle the case where a host's log is flushed while a checkpoint is being saved

**Best for:** General-purpose use. Recommended default.

---

## 4. Host Daemon

Each host runs a daemon process that manages everything local to that host.

### Host Daemon Responsibilities

| Responsibility | Description |
|---------------|-------------|
| **Job Slot Management** | Track available/occupied job slots. Accept jobs from the coordinator. |
| **Event Log Writing** | Record events from local jobs into the local log file. |
| **Event Log Streaming** | Periodically flush events to the coordinator's global log. |
| **9p Server** | Serve 9p requests from local jobs (filesystem operations). |
| **Perl Interposition** | Run `Shmoo::Tool` wrappers for all build tools on this host. |
| **Syscall Interceptor** | Inject `libsyscallhook.so` into local build processes. |
| **Local Cache** | Maintain a content-addressed cache of build outputs. |
| **Health Reporting** | Report slot availability, disk space, CPU/memory to the coordinator. |
| **Mission Lifecycle** | Join/leave missions, report completion/failure. |

### Host Daemon Directory Structure

```
/var/run/shmoo/
├── mission-0xA3F1/
│   ├── daemon.pid              # Daemon PID file
│   ├── local.log               # Local event log (EIDs 1000000+)
│   ├── local.log.index         # Index: EID → file offset
│   ├── slots.json              # Current slot allocation
│   ├── 9p.sock                 # Unix socket for 9p server
│   ├── 9p_tcp_listen           # TCP listen port file
│   ├── cache/                  # Local content-addressed cache
│   │   ├── sha256/
│   │   │   ├── ab/c123...      # Cached artifact
│   │   │   └── de/4567...
│   │   └── index.json          # Cache index
│   └── tools/                  # Host-local toolchain
│       ├── gcc/
│       ├── perl/
│       └── make/
```

### Host Daemon Lifecycle

```
START
  │
  ├─ 1. Initialize: read config, load host-local tools
  │
  ├─ 2. Bind 9p server: open TCP port and/or Unix socket
  │
  ├─ 3. Register with coordinator:
  │     └─ Send: {host_id, available_slots, disk_space, cpu_cores, toolchain_versions}
  │
  ├─ 4. Enter accept loop:
  │     ├─ Accept new jobs from coordinator
  │     ├─ Assign to local slots
  │     ├─ Execute job (with syscall interceptor)
  │     ├─ Record events to local log
  │     ├─ Flush events to coordinator periodically
  │     ├─ Report job completion/failure
  │     └─ Release slot
  │
  ├─ 5. On shutdown:
  │     ├─ Flush all local events to coordinator
  │     ├─ Save local log checkpoint
  │     ├─ Release all slots
  │     └─ Deregister with coordinator
```

### Host Daemon to Coordinator Communication

```
┌──────────────┐              ┌──────────────────┐
│  Host Daemon  │              │  Mission Coord.  │
│              │              │                  │
│  ┌─────────┐ │  REGISTER    │  ┌─────────────┐ │
│  │ 9p Srv  │ │─────────────→│  │ Host Roster │ │
│  └─────────┘ │              │  └─────────────┘ │
│              │              │                  │
│  ┌─────────┐ │  JOB ASSIGN │  ┌─────────────┐ │
│  │ Slot Mgr│ │←────────────│  │ Job Sched.  │ │
│  └────┬────┘ │              │  └──────┬──────┘ │
│       │      │  EXEC START  │         │        │
│       ▼      │─────────────→│         │        │
│  ┌─────────┐ │  EVENT LOG  │         │        │
│  │Local Log│ │─────────────→│  Global │        │
│  └─────────┘ │  FLUSH      │  Log    │        │
│              │              │  Index  │        │
│              │              └─────────┘        │
└──────────────┘                                 │
                                                 │
        Events flow: Host → Coordinator (flush)  │
        Jobs flow:   Coordinator → Host (assign) │
```

---

## 5. Event Log Writing Flow

### Local Log Writing (Per Host)

Each local job writes events to its host's local log:

```
┌─────────────────────────────────────────────┐
│  Host Daemon                                │
│                                             │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  │
│  │ Job A    │  │ Job B    │  │ Job C    │  │
│  │ LLB A    │  │ LLB B    │  │ LLB C    │  │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  │
│       │              │              │       │
│       ▼              ▼              ▼       │
│  ┌──────────────────────────────┐           │
│  │  Local Log Buffer (merged)   │           │
│  │  [EID=1000001] [EID=1000042] │           │
│  │  [EID=2000001] [EID=2000015] │           │
│  │  [EID=3000001]              │           │
│  └──────────────┬───────────────┘           │
│                 ▼                           │
│         ┌─────────────┐                     │
│         │ Flush to:   │                     │
│         │ 1. Local    │                     │
│         │    file     │                     │
│         │ 2. Global   │ (periodic)          │
│         │    stream   │                     │
│         └─────────────┘                     │
└─────────────────────────────────────────────┘
```

**Local Log Format:**
- EID range assigned to the host (e.g., host A: 1000000–1999999)
- Each job's local EID is added to the host's base offset
- Events are written immediately to the local log file (append-only)
- A local log index maps EID → file offset

**Flush to Global (Hybrid mode):**
- Every N seconds or every M events, the local log is flushed to the coordinator
- The coordinator appends the flushed events to the global stream (if enabled)
- The coordinator updates the global index

### Global Log Writing (Monolithic/Hybrid)

```
Coordinator receives:
  From Host A: [EID=1000001, ... 1000042]
  From Host B: [EID=2000001, ... 2000015]
  From Host C: [EID=3000001, ... 3000008]

Coordinator appends to global stream:
  [EID=1000001] ... [EID=3000008]
  (sorted by EID, but EID ranges are disjoint per host so no merge needed)
```

Since each host has a disjoint EID range, the coordinator doesn't need to sort or merge — it just appends the flushed events in the order they arrive. The global index maps EID ranges to hosts for seeking.

---

## 6. Replay Across Hosts

### Monolithic Mode Replay

Simple: read the global log from EID 0 to target EID.

```bash
$ shbuild replay --mission 0xA3F1 --eid 2000015
# Reads: global.log, EID 0–2000015
# State: reconstructed at EID 2000015 (across all hosts)
```

### Per-Host Mode Replay

Read the relevant host's log. For global state, read all hosts' logs up to the target EID.

```bash
$ shbuild replay --mission 0xA3F1 --eid 2000015
# EID 2000015 is in Host B's range (2000000–2999999)
# Reads: host-alpha.log (full), host-beta.log (0–2000015), host-gamma.log (partial)
# State: reconstructed at EID 2000015
```

### Hybrid Mode Replay

Checkpoints enable replay from the last checkpoint, not from EID 0.

```bash
$ shbuild replay --mission 0xA3F1 --eid 2000015
# Last checkpoint before 2000000 is cp_1000000
# Reads: cp_1000000.json (state snapshot at EID 1000000)
#        host-alpha.log (1000000–2000015)
#        host-beta.log (1000000–2000015)
#        host-gamma.log (1000000–2000015)
# State: restored from checkpoint + replay events 1000000–2000015
```

**Checkpoint Format:**
```json
{
  "eid": 1000000,
  "timestamp": 1706000000,
  "state": {
    "filesystem": { "sha256": "abc123...", "size": 10485760 },
    "environment": { "CFLAGS": "-O2", "LDFLAGS": "-L/libs" },
    "processes": [ { "pid": 1001, "cmd": "gcc -c main.c" } ]
  },
  "hosts": {
    "host-alpha": { "eid_range": "1000000-1999999", "log_offset": 524288 },
    "host-beta": { "eid_range": "2000000-2999999", "log_offset": 262144 },
    "host-gamma": { "eid_range": "3000000-3999999", "log_offset": 131072 }
  }
}
```

---

## 7. Speculative Roll-Forward Across Hosts

### Scenario

A compiler fails on Host A at EID 1500000. The fix involves changing a variable that affects Hosts A, B, and C (because Host C links against an artifact built on Host B which depends on the compiler output from Host A).

### Roll-Forward Steps

1. **Identify affected hosts:** The coordinator determines that the variable change at EID 1500000 affects hosts A, B, and C.
2. **Restore from checkpoint:** Restore the state at the last checkpoint (EID 1000000) for all affected hosts.
3. **Mutate the event:** Change the variable at EID 1500000 on Host A.
4. **Replay on Host A:** Replay Host A's local log from EID 1500000 to the end.
5. **Forward to Host B:** Since Host B depends on Host A's output, Host B's input has changed. Replay Host B's log from EID 2500000 (the point where Host B reads Host A's output).
6. **Forward to Host C:** Host C depends on Host B's output. Replay Host C's log from EID 3500000.
7. **Compare results:** The coordinator compares the final artifacts from all hosts.

### Divergence Detection

The coordinator tracks divergence at each host:

```
Host A: Divergence at EID 1500000 (compiler changed output)
  ↓
Host B: Divergence at EID 2500000 (linker input changed due to Host A's output)
  ↓
Host C: No divergence at EID 3500000 (linker succeeded with new inputs)

Result: Build succeeds! The fix worked.
```

---

## 8. Job Slot Management

### Top-Level Scheduler

The coordinator's job scheduler works as follows:

```
1. Parse build graph → DAG of actions
2. Topological sort → execution order
3. Assign first batch of independent actions to available host slots
4. For each action that completes:
   a. Check if new actions are now ready (all dependencies met)
   b. If yes, assign ready actions to available slots
   c. Repeat until no more actions are ready or no slots available
5. If no actions are ready and no slots are available → mission complete
6. If any action fails → mission failed (or retry per policy)
```

### Slot Allocation

Each host advertises its available slots. The coordinator tracks slot usage:

```
Host A: 8 slots total, 5 busy, 3 free
Host B: 4 slots total, 4 busy, 0 free
Host C: 4 slots total, 2 busy, 2 free
```

Jobs are assigned to hosts based on:
- **Availability:** Must have a free slot
- **Capability:** Host must have the required toolchain (gcc version, OS, architecture)
- **Proximity:** Prefer hosts that have the required input files already cached
- **Load balancing:** Prefer hosts with fewer busy slots

---

## 9. Implementation Plan

### Phase 1: Mission Coordinator Core
- [ ] Mission structure and lifecycle management
- [ ] Build graph parser (shmoo-build, Makefile)
- [ ] Topological sort algorithm
- [ ] Job assignment algorithm (slot-aware, capability-aware)
- [ ] Basic coordination protocol (HTTP or custom TCP)

### Phase 2: Host Daemon Core
- [ ] Host daemon structure and lifecycle
- [ ] Job slot management
- [ ] Local event log writing (per-host log file)
- [ ] Local event log index
- [ ] Registration with coordinator

### Phase 3: Event Log Integration
- [ ] Event log flushing (local → coordinator)
- [ ] Global log index (EID range → host mapping)
- [ ] Checkpoint creation and loading
- [ ] Hybrid mode: per-host logs + global stream

### Phase 4: Replay Engine (Distributed)
- [ ] Monolithic replay (single log)
- [ ] Per-host replay (multi-log)
- [ ] Hybrid replay (checkpoint + multi-log)
- [ ] Speculative roll-forward across hosts

### Phase 5: 9p Server Integration
- [ ] 9p server on each host daemon
- [ ] 9p client on each host for remote access
- [ ] Cross-host 9p mounts (Host A can mount Host B's filesystem)

### Phase 6: Testing & Hardening
- [ ] Multi-host integration tests
- [ ] Fault tolerance (host crash, network partition)
- [ ] Performance benchmarks (throughput, latency)
- [ ] Stress test (100+ hosts, 10000+ jobs)

---

## 10. Configuration Reference

### Mission Config (YAML)

```yaml
mission:
  id: "0xA3F1"
  name: "Build #4821"
  log_mode: "hybrid"               # monolithic | per-host | hybrid
  
  log_flush:
    interval_seconds: 5            # Flush local logs every N seconds
    event_count: 1000              # Or every N events (whichever first)
  
  checkpoints:
    enabled: true
    interval_eids: 100000          # Create checkpoint every N events
    interval_seconds: 60           # Or every N seconds
  
  scheduler:
    max_global_jobs: 64            # Max parallel jobs across all hosts
    retry_count: 2                 # Retry failed jobs N times
    retry_delay_seconds: 5         # Delay before retry
  
  hosts:
    - name: "alpha"
      host: "alpha.local"
      slots: 8
      type: "linux-x86_64"
      9p_port: 5640
      9p_unix: "/tmp/9p-alpha.sock"
      tools:
        gcc: "/usr/bin/gcc-13"
        perl: "/usr/bin/perl-5.38"
        make: "/usr/bin/make"
      mounts:
        - path: "/src"
          local_path: "/data/shmoo/src"
        - path: "/libs"
          local_path: "/data/shmoo/libs"
    
    - name: "beta"
      host: "beta.local"
      slots: 4
      type: "win10-x86_64"
      9p_port: 5641
      9p_unix: "\\\\.\\pipe\\9p-beta"
      tools:
        gcc: "C:\\tools\\mingw\\gcc.exe"
        perl: "C:\\tools\\strawberry\\perl\\bin\\perl.exe"
      mounts:
        - path: "/src"
          local_path: "D:\\shmoo\\src"
        - path: "/libs"
          local_path: "D:\\shmoo\\libs"
```

---

## 11. Summary

The distributed build architecture introduces:

- **Mission Coordinator:** Top-level scheduler that assigns jobs across hosts
- **Host Daemon:** Per-process daemon managing local slots, event logs, 9p server, and toolchain
- **Event Log Options:** Monolithic (simple replay), per-host (fully distributed), hybrid (best of both)
- **Checkpointing:** Enables fast replay from last stable state
- **Speculative Roll-Forward:** Across host boundaries with divergence detection
- **Job Slot Management:** Coordinated allocation across hosts based on capability, availability, and load

This turns the build system from a single-machine tool into a **cluster build engine** capable of coordinating builds across heterogeneous hosts while maintaining the full debugging and replay capabilities of the Shmoo build system.
