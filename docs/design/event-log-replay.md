# Design: Binary Event Log (BEL) — Deterministic Replay Engine

*2025-01-23 | Status: Active Design*
*Purpose: A tight binary event log with event-numbered transaction records that captures the full build environment state, enabling time-travel reconstruction, speculative roll-forward, and parallel recording with socket-based stream joining. Analogous to examining a core file with a debugger, but for the entire build process.*

---

## 1. The Binary Event Log (BEL)

The BEL is an append-only, event-numbered log that captures every significant state change in the build environment. It is the single source of truth for the build execution — if you have the log, you can reconstruct the entire build from any point.

### Why a Binary Log?
- **Compactness:** Only relevant state changes are recorded (deltas, not full state snapshots at every step)
- **Fast seeking:** Event number → direct offset into the log file (or indexed)
- **Deterministic:** The same log always replays to the same state
- **Append-only:** No corruption risk; if a write fails, the log is simply truncated
- **Streamable:** Can be streamed over sockets, compressed, or archived

### Record Layout (Binary)

```
┌──────────┬───────┬──────────┬───────────┬──────────┬──────────┐
│ EventNum │ Type  │  Length  │  Payload  │ Checksum │  Pad     │
│  (8B)    │ (1B)  │ (4B LE)  │  Variable │ (4B CRC) │ (4B)     │
└──────────┴───────┴──────────┴───────────┴──────────┴──────────┘
```

- **EventNum (8 bytes, little-endian):** Monotonically increasing event counter
- **Type (1 byte):** Record type identifier (see §3)
- **Length (4 bytes LE):** Payload length (0–1,048,575 for inline; larger values indicate chunked storage)
- **Payload (variable):** Type-dependent binary data
- **Checksum (4 bytes):** CRC32C over EventNum + Type + Length + Payload
- **Pad (4 bytes):** Alignment padding (8-byte boundary)

### Inline vs. Chunked Storage

For large payloads (e.g., a 500KB compiler stderr dump), the record stores an **external reference** instead of inline data:

```
Type = RECORD_CHUNK_REF
Payload = [blob_id (8B)][blob_offset (8B)][blob_length (8B)]
```

The actual data is stored in a separate **Event Blob** file (or streamed). This keeps the log file small and indexable.

---

## 2. Parallel Recording Architecture

### Per-Job Local Log Buffers

Each parallel build job runs in its own process and writes to its own **Local Log Buffer** (LLB):

```
┌─────────────────────────────────────────────────────────┐
│                        Monitor                           │
│                                                         │
│  ┌───────────────────────────────────────────────────┐  │
│  │          Global Event Log (on disk)                │  │
│  │  [EID=0] [EID=1] [EID=2] ... [EID=N]              │  │
│  └───────────────────────────────────────────────────┘  │
│          ▲           ▲           ▲                       │
│          │socket    │socket    │socket                  │
│  ┌───────┴──────┐ ┌─┴────────┐ ┌┴────────────┐        │
│  │  Job A       │ │ Job B    │ │ Job C       │  ...   │
│  │  LLB A       │ │ LLB B    │ │ LLB C       │        │
│  │  EID: 0,42,84│ │ EID:0,56 │ │ EID:0,91,182│        │
│  └──────────────┘ └──────────┘ └─────────────┘        │
└─────────────────────────────────────────────────────────┘
```

**Local Log Buffer properties:**
- In-memory ring buffer (configurable size, default 4MB)
- Monotonically increasing local EIDs (start at 0, increment per event)
- **No lock contention** between jobs — each job writes independently
- Automatically flushed to disk and/or streamed when full
- On job exit, final flush ensures no events are lost

**Per-job local EID space:**
- Each job has its own EID counter (e.g., Job A: 0, 42, 84, ...; Job B: 0, 56, ...)
- The Monitor assigns a **base offset** per job (Job A: 0, Job B: 1,000,000, Job C: 2,000,000)
- Global EID = base_offset + local_EID
- This guarantees uniqueness without coordination between jobs

---

## 3. Record Types

| Type | Name | Size | Payload Format | Description |
|------|------|------|----------------|-------------|
| 0x01 | `ENV_SET` | ~20–500B | `[name_len:2][name:str][val_len:2][val:str]` | Set environment variable |
| 0x02 | `ENV_UNSET` | ~5–50B | `[name_len:2][name:str]` | Unset environment variable |
| 0x10 | `FILE_WRITE` | ~20–1M+ | `[path_len:2][path:str][offset:8][len:4][data:var]` | Write to file |
| 0x11 | `FILE_CREATE` | ~10–50B | `[path_len:2][path:str]` | Create new file |
| 0x12 | `FILE_DELETE` | ~10–50B | `[path_len:2][path:str]` | Delete file |
| 0x13 | `FILE_CHMOD` | ~10–50B | `[path_len:2][path:str][mode:2]` | Change file permissions |
| 0x20 | `PROC_FORK` | ~10–100B | `[pid:4][flags:4]` | Create child process |
| 0x21 | `PROC_EXEC` | ~10–500B | `[pid:4][cmd_len:2][cmd:str]` | Replace process with command |
| 0x22 | `PROC_EXIT` | ~8B | `[pid:4][exit_code:4]` | Process exits |
| 0x23 | `PROC_SIGNAL` | ~12B | `[pid:4][signal:4]` | Send signal to process |
| 0x24 | `PROC_WAIT` | ~8B | `[pid:4]` | Wait on child process |
| 0x30 | `NET_SEND` | ~10–1M+ | `[dst_len:2][dst:str][len:4][data:var]` | Send network data |
| 0x31 | `NET_RECV` | ~10–1M+ | `[src_len:2][src:str][len:4][data:var]` | Receive network data |
| 0x40 | `LOCK_ACQUIRE` | ~20–100B | `[pid:4][lock_id_len:2][lock_id:str][blocking:1]` | Acquire lock |
| 0x41 | `LOCK_RELEASE` | ~20–100B | `[pid:4][lock_id_len:2][lock_id:str]` | Release lock |
| 0x50 | `BUILD_START` | ~20–200B | `[action_id:4][rule_len:2][rule:str]` | Start build action |
| 0x51 | `BUILD_STEP` | ~10–100B | `[step_num:4][total_steps:4][msg_len:2][msg:str]` | Build step completed |
| 0x52 | `BUILD_END` | ~8B | `[action_id:4]` | Build action complete |
| 0x60 | `TIMER_START` | ~30–200B | `[timer_id:4][name_len:2][name:str]` | Start timer |
| 0x61 | `TIMER_END` | ~16B | `[timer_id:4][duration_us:8]` | Stop timer |
| 0x70 | `CHECKPOINT` | ~16B | `[checkpoint_id:8][timestamp:8]` | Save state checkpoint |
| 0x80 | `MUTATION` | ~20–1M | `[original_eid:8][original_type:1][mutated_data:var]` | Speculative mutation record |
| 0xFF | `META` | ~10–500B | `[key_len:2][key:str][value_len:2][value:str]` | Metadata record |

---

## 4. Socket Stream Protocol (Stream Joining)

### Connection Handshake

Each job opens a Unix domain socket (or TCP loopback) to the Monitor:

```
Job → Monitor:
  [MSG_HELLO][job_id:4][llb_size:8][supported_types:4]

Monitor → Job:
  [MSG_HELLO_ACK][base_offset:8][stream_id:4][max_frame_size:8]
```

### Frame Protocol

Data is streamed in **frames** to avoid fragmented writes:

```
[MSG_DATA][stream_id:4][record_count:2][records...]

[MSG_DATA_END][stream_id:4][final_offset:8]

[MSG_DATA_ERROR][stream_id:4][error_code:4]
```

**Record format in stream:**
```
[frame_record_count:2][record_0][record_1]...[record_N]
```

This batches many records into one socket write, reducing system call overhead.

### Joining Algorithm

The Monitor receives frames from all jobs and merges them into the Global Event Log:

1. **Buffer received frames** per stream
2. **Sort by global EID** (base_offset + local_EID)
3. **Write to Global Event Log** in EID order
4. **Acknowledge** to each job as records are consumed

**Optimization:** Since each job's EID range is disjoint (thanks to base offsets), the Monitor can simply concatenate the streams in the correct order without comparing individual records. This is O(N) with a single pass.

### Why This Matters

- **No lock contention:** Jobs never block on each other
- **High throughput:** Socket batching minimizes overhead
- **Fault tolerance:** If a job crashes mid-stream, the Monitor has everything up to the last frame
- **Streaming in real-time:** The Monitor can begin rendering the Interactive Graph before the build completes

---

## 5. The Replay Engine

The Replay Engine reads the Global Event Log and reconstructs the full build environment state.

### State Object

```perl
package Shmoo::Replay::State;

has 'filesystem' => (is => 'rw', default => sub { Shmoo::Replay::FS->new });
has 'environment' => (is => 'rw', default => sub { Shmoo::Replay::Env->new });
has 'process_table' => (is => 'rw', default => sub { Shmoo::Replay::ProcTable->new });
has 'network_state' => (is => 'rw', default => sub { Shmoo::Replay::NetState->new });
has 'lock_table' => (is => 'rw', default => sub { Shmoo::Replay::LockTable->new });
has 'timers' => (is => 'rw', default => sub { Shmoo::Replay::TimerTable->new });
```

### Execution Phases

```
1. LOAD        → Read the Global Event Log (mmap for speed)
2. SEEK        → Jump to a target EID (or start from 0)
3. REPLAY      → Execute each record sequentially, mutating State
4. PAUSE/STEP  → At any EID, the state is fully reconstructed
5. MUTATE      → Optionally modify a record before replaying
6. RERUN       → Continue replaying from the mutation point
7. COMPARE     → Compare final state against the original run
```

### Reconstruction Example

```
EID=0:  ENV_SET SHELL=/bin/bash
EID=1:  ENV_SET PATH=/tools/bin:/usr/bin
EID=2:  BUILD_START action=compile:main.c rule=cc_compile
EID=3:  FILE_WRITE /outputs/main.o [offset=0, len=4096]
EID=4:  PROC_FORK pid=1001 flags=0
EID=5:  PROC_EXEC pid=1001 cmd=gcc -O2 -o /outputs/main.o /inputs/main.c
EID=6:  NET_SEND 9p.server:5640 [request=stat /inputs/main.c]
EID=7:  NET_RECV 9p.server:5640 [response=stat_ok size=1024]
EID=8:  PROC_EXIT pid=1001 exit_code=0
EID=9:  BUILD_STEP step=1 total=3 msg="main.o compiled"
...
EID=1000: BUILD_END action=compile:main.c  ← failure point
```

At EID=1000, the State object contains:
- The full in-memory filesystem with all files created/written
- All environment variables as they existed at the failure point
- The process table (all processes that ran, with exit codes)
- All network exchanges (9p requests/responses)
- Lock acquisitions and releases

---

## 6. Time-Travel (Reconstruction at Any EID)

The engineer can "rewind" to any EID and examine the build state:

```bash
# Show the build state at EID 500
$ shbuild replay --eid 500

# The engine:
# 1. Reads the Global Event Log
# 2. Executes records 0–500 in order
# 3. At EID=500, the State object is fully reconstructed
# 4. The engineer can inspect:
#    - $ shbuild replay --eid 500 --env         # Show environment
#    - $ shbuild replay --eid 500 --files      # Show filesystem
#    - $ shbuild replay --eid 500 --processes  # Show running processes
#    - $ shbuild replay --eid 500 --network    # Show network exchanges
```

This is exactly like examining a core file — but for the **entire build environment**, not just a single crashing process.

---

## 7. Speculative Roll-Forward (What-If Testing)

The engineer can **mutate** the log at any EID and re-run forward to see what happens:

```
Original Run:
  EID=200: ENV_SET CFLAGS=-O2
  EID=300: ENV_SET CFLAGS=-O3    ← User wants to test this change
  EID=500: BUILD_END             ← Original failure point

Speculative Run:
  EID=200: ENV_SET CFLAGS=-O2    ← Same state up to here
  EID=300: ENV_SET CFLAGS=-O3    ← Mutated by user
  EID=301: MUTATION eid=300 type=ENV_SET original=CFLAGS=-O2
  EID=302...N: Replayed with CFLAGS=-O3
  EID=M: BUILD_END                ← Did it succeed?
```

**Divergence Detection:**
The engine compares the speculative run against the original:
- **No divergence:** The changed variable had no effect
- **Expected divergence:** The build still fails at the same EID
- **Unexpected divergence:** The build succeeds (fix!) or fails at a different EID (new bug)

**"Roll forward to failure" mode:**
The engine can automatically roll forward from the mutation point until it detects the same failure, or the build completes. This tells the engineer: *"Your change didn't fix it — the failure still occurs at EID=500."*

---

## 8. Interactive Graph Integration

The **Interactive Graph UI** visualizes the Global Event Log:

### Graph Structure
- **Nodes** = Event records (TRs)
- **Edges** = Causal dependencies (determined by record types, e.g., PROC_EXEC depends on the environment at EID before it)
- **Color coding:**
  - Green: Normal execution
  - Yellow: Warning (unusual timing, large data transfer, high entropy)
  - Red: Failure/error state
  - Blue: User-modified/speculative event
  - Gray: Pruned from view (not relevant to current focus)

### Visual Features
- **Zoom/pan:** Navigate through millions of events
- **Filter by type:** Show only ENV_SET, FILE_WRITE, PROC_EXEC, etc.
- **Drill down:** Click a node to see its full payload (hex dump, decoded values)
- **Highlight chain:** Click a variable change → highlights all downstream nodes that depend on it
- **Divergence overlay:** Toggle between original and speculative run; differences highlighted
- **Time slider:** Slide through EIDs to watch the build evolve

---

## 9. Chunked Blob Storage (For Large Payloads)

For large payloads (e.g., compiler output > 64KB), the record stores an external reference:

```
Type = RECORD_EXTERNAL_REF (0xFE)
Payload = [blob_id:8][blob_offset:8][blob_length:8]
```

**Blob file layout:**
```
┌──────────────────┐
│  Blob Index      │ ← [blob_id → file_path]
├──────────────────┤
│  Blob Data       │ ← Raw binary data for all blobs
└──────────────────┘
```

Blobs are stored in a separate file (or directory tree) and referenced by the log. This keeps the log file small and seekable.

---

## 10. Performance Optimizations

### Memory Mapping
- `mmap()` the Global Event Log for zero-copy reads
- The replay engine seeks by offset (O(1) with mmap)

### Indexing
- In-memory index: `EID → file_offset` (hash map or B-tree)
- Type index: `Type → list of EIDs` (for filtering by record type)

### Compression
- Optional: LZ4 or Zstandard compression of the log file
- Compressed logs are faster on disk but require decompression for replay
- Blob files can be compressed independently

### Asynchronous Write-Ahead
- Jobs write to Local Log Buffers asynchronously
- Monitor merges streams asynchronously
- The build process never blocks on log writes (buffer overflows are handled by truncation with a warning)

### Checkpointing
- At regular intervals (e.g., every 1000 events or every minute), save a **full state snapshot** to a checkpoint file
- If the log is corrupted or truncated, the engine can restore from the last checkpoint and replay only the remaining events
- Checkpoints are identified by EID (the EID at which the checkpoint was taken)

---

## 11. Security Model

### Mutation Approval
When the engineer modifies the log (speculative roll-forward), the changes pass through a **security filter**:
1. **Command audit:** Check for dangerous commands (e.g., `rm -rf /`)
2. **Path validation:** Ensure file writes stay within the build tree
3. **Network policy:** Check for unexpected network connections
4. **Approval gate:** The engineer explicitly confirms the mutation before replay

### Logging Mutations
All mutations are themselves logged as `MUTATION` records, creating an audit trail:
```
EID=500: ENV_SET CFLAGS=-O3          ← User's change
EID=501: MUTATION eid=500 type=ENV_SET original=CFLAGS=-O2  ← Audit trail
```

---

## 12. Implementation Plan

### Phase 1: Core Log Structure
- Define binary record format (Section 2)
- Implement record types (Section 3)
- Implement CRC32C checksumming
- Implement basic append-only log writer

### Phase 2: Parallel Recording
- Implement per-job Local Log Buffers (in-memory ring buffers)
- Implement socket stream protocol (Section 4)
- Implement base offset assignment per job
- Implement Monitor-side frame reception and merging

### Phase 3: State Object & Replay Engine
- Implement `Shmoo::Replay::State` (filesystem, environment, process table, etc.)
- Implement record execution (each record type mutates State)
- Implement seek/rewind (jump to any EID, replay from there)
- Implement full state reconstruction at any EID

### Phase 4: Speculative Roll-Forward
- Implement log mutation API (insert, modify, delete records)
- Implement MUTATION record type
- Implement divergence detection (compare original vs. speculative)
- Implement automatic roll-forward to failure detection

### Phase 5: Chunked Blob Storage
- Implement RECORD_EXTERNAL_REF type
- Implement blob file management
- Integrate with log reader/writer

### Phase 6: Checkpointing
- Implement checkpoint saving (full state snapshot)
- Implement checkpoint loading (restore state, replay remaining events)
- Implement checkpoint validation (checksum, EID range)

### Phase 7: Security & Auditing
- Implement mutation approval workflow
- Implement MUTATION record audit trail
- Implement security filter for dangerous operations

### Phase 8: Interactive Graph UI
- Implement event log visualization (Section 8)
- Implement node drill-down and payload inspection
- Implement divergence overlay view
- Implement time slider and zoom/pan

---

## 13. Example: Build Failure Debugging Workflow

### Scenario
Build fails at EID=1,000,000 with a linker error: `"undefined reference to 'foo'"`

### Step 1: Examine the State at Failure
```bash
$ shbuild replay --eid 1000000 --env | grep CFLAGS
CFLAGS=-O2 -g
LDFLAGS=-L/mnt/vol/libs -lfoo

$ shbuild replay --eid 1000000 --files | grep libfoo.a
/mnt/vol/libs/libfoo.a: exists, size=102400, modified=EID=500000
```

### Step 2: Trace the Origin of the Failure
```bash
# Find when libfoo.a was last written
$ shbuild replay --eid 500000 --proc  # Who created libfoo.a?
EID=499990: PROC_EXEC pid=2001 cmd=ar rcs /mnt/vol/libs/libfoo.a base.o util.o

# Find who called the failing linker
$ shbuild replay --eid 999999 --proc  # Process running at EID=1M
EID=999995: PROC_EXEC pid=3001 cmd=ld -o app app.o libfoo.a
EID=1000000: PROC_EXIT pid=3001 exit_code=1
```

### Step 3: Speculate — Try a Different CFLAGS
```bash
# Mutate: change CFLAGS from -O2 to -O3 at EID=200000
$ shbuild mutate --eid 200000 --type ENV_SET --key CFLAGS --value "-O3 -g"
MUTATION recorded at EID=1000001

# Roll forward to failure point
$ shbuild replay --eid 1000000 --speculative

# Result:
Divergence detected at EID=800000 (compiler step)
Build still fails at EID=1000000 (same linker error)
Conclusion: CFLAGS change did not affect the missing symbol
```

### Step 4: Speculate — Try a Different Library
```bash
# Mutate: add -lbar to LDFLAGS at EID=200000
$ shbuild mutate --eid 200000 --type ENV_SET --key LDFLAGS --value "-L/mnt/vol/libs -lfoo -lbar"
MUTATION recorded at EID=1000001

# Roll forward
$ shbuild replay --eid 1000000 --speculative

# Result:
Build succeeds at EID=950000!
Divergence: The missing 'foo' symbol was provided by libbar, not libfoo
Conclusion: The build recipe needs -lbar in addition to -lfoo
```

### Step 5: Fix the Recipe
```bash
$ shbuild commit --eid 200000 --fix "Add -lbar to LDFLAGS"
Recipe updated. Next build will use the fixed parameters.
```

---

## 14. Summary

The **Binary Event Log (BEL)** is the foundation of Shmoo's time-travel, speculation, and debugging capabilities:

- **Tight binary format** with event-numbered transaction records
- **Parallel recording** — each job writes to its own buffer, streams via socket
- **Socket stream joining** — Monitor merges streams in EID order (O(N), no locking)
- **Deterministic reconstruction** — the full build environment at any EID
- **Speculative roll-forward** — mutate the log, replay, detect divergence
- **Core file analogy** — examine the entire build state like a debugger examines a core file
- **Interactive graph UI** — visualize the log as a graph, drill down, overlay divergence

This turns the build system into a **fully deterministic, replayable execution trace** — comparable to a debugger's ability to examine a core file, but for the entire build process.
