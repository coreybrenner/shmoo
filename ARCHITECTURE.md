# Shmoo — Architecture

**Status:** v0.8  
**Last updated:** 2026-08-19  
**License:** BSD 2-Clause

---

## 0. About

Shmoo is a recursive, cross-platform build system designed to:

- Work in any environment (physical host, Docker, QEMU, WSL, Wine, Cygwin, native OS)
- Provide audit-grade traceability for government/security use cases
- Replicate and unify major build technologies (GNU Make, CMake, NMake, Redo, Xcode build files)
- Operate as an assistive build environment for users with physical limitations

Named after the Al Capp cartoon character.

---

## 1. Architecture — Eight Layers

Every subsystem follows this layered structure. Each layer is responsible for **one** step in a chain. It detects its environment, spawns the next layer with an updated context, and exits. The final layer performs the actual work.

### Layer 1: Launcher / Chain Unwrapper

The root process. Resolves its environment, discovers resources, and spawns the next layer.

- Recursive environment resolution (chain unwinding): each layer does **one** job and exits
- Hard depth limit prevents infinite loops
- Every layer can be the root if launched independently
- Walkback resource search (see §2)
- Patch isolation versioning scheme
- State manifest with SHA-256 chain integrity

**Principles:**
- Context flows via environment variables or manifest files
- Each layer can be launched independently as the root
- The launcher's only job is to find and start the next layer

**Novelty — Cumulative Configuration Chain:**
Unlike systems that snapshot environment state at a single point (Reproducible Builds), or systems that pin inputs but discard mutation history (Nix), Shmoo records **every change made to the configuration across every layer boundary**. Each transition documents:
- Which configuration values were present at the boundary
- What was added, removed, or modified by the transition
- Why the change occurred (e.g., filesystem path translation, interpreter flag injection, module loading)
- A stable reference to the pre-transition state so the chain can be reconstructed

This chain is **probe-able at runtime and post-build**. A log analyzer or audit tool can walk the chain to answer questions like "which layer modified `PERL5LIB`?" or "what flags were injected when we crossed from Linux to QEMU?" Prior systems either record a static snapshot or discard the mutation history after resolution. Shmoo retains both.

### Layer 2: Wrapper / Masquerade

Presents the system as the tool the user expects.

- Symlinks present system as `make`, `nmake`, `pmake`, `cmake`, etc.
- Resolves its execution name to a matching module
- Falls back to a universal wrapper if no specialized module exists
- Config injection via `shmoo-{tool}.env` files (Bourne-safe with optional Perl-lite extensions)

**The Mask:** The system presents itself as the tool the user expects via symlinks. It resolves its execution name to a matching module. If no module exists, it falls back to a universal wrapper that augments the tool's execution.

**Principles:**
- No user configuration needed — the name **is** the command
- Fallback wrapper works for any tool, even without a specialized module
- Augmentation layer intercepts args, env, and I/O

### Layer 3: Stream Bridge

Connects the wrapper to the executor, passing I/O across environment boundaries.

- FD passthrough preferred (zero-buffering)
- Secure temp file fallback for environment boundaries (cross-platform, cross-hypervisor)
- Captures stdin/stdout/stderr, file opens, system calls
- The bridge normalizes I/O streams across layers

**Novelty — State Bridge Across Boundaries:**
The Stream Bridge does more than pass data between processes. It also preserves **configuration state** across environment boundaries where the representation changes:

- **Filesystem path translation**: a path like `/home/user/src` on Linux might map to `C:\home\user\src` under Wine. The bridge records the mapping, not just the translated value.
- **Interpreter flag forwarding**: `-I` flags, `$PERL5LIB`, `$PERL5OPT` — these are captured at each boundary, translated where needed, and forwarded with the original value preserved in the configuration chain.
- **Environment variable normalization**: variable names and semantics differ across environments (e.g., `PATH` vs `Path`, case sensitivity). The bridge records the original value, the target format, and the transformation applied.

Unlike Nix (which hashes the entire build input and stores it as an opaque blob) or Bazel (which exposes a fixed sandbox view), Shmoo's bridge maintains a **dual representation**: the host-native form and the target form, plus the transformation. This allows reconstruction of any intermediate state.

### Layer 4: Monitor (Black Box)

C-level wrapper around `execve` with `ptrace`/`inotify`.

- Logs every file opened, every byte read/written, every syscall
- Audit mode toggle: when disabled, runs full speed with no overhead
- Records all data that enters the system — hashes it, timestamps it, records it
- Append-only log. No deletions.

**Principles:**
- Every byte of data that enters the system must be hashed, timestamped, and recorded
- No deletions
- Append-only JSONL event log
- SHA-256 hashes of all inputs and outputs
- Full I/O capture (stdin, stdout, stderr, opened files)

**Novelty — Probe-able Event Stream:**
Prior systems record build events as a flat log or a static graph. Shmoo's Monitor produces a **structured, query-able event stream** where:
- Events carry explicit causal links (parent → child)
- Configuration mutations are recorded as distinct events with before/after state snapshots
- The event stream can be queried during a running build (via the daemon's HTTP API) or consumed post-build by an external analyzer
- No reconstruction is required — the full provenance chain is recorded at the point of observation, not synthesized later

This allows live debugging of build failures, post-build audit of environment drift, and plugin-based interception of specific event types.

### Layer 5: Parser / AST

Converts build files into a common representation.

- Zsh/globbing parser with Zsh-compatible glob engine
- GNU Make, CMake, Redo, NMake parsers
- Outputs **CIR** — Common Intermediate Representation
- Each parser produces the same CIR structure regardless of input format

### Layer 6: Graph Engine

Directed acyclic graph of build dependencies.

- Semantic hashing (ignores whitespace/comments in source files)
- Fast C-level stat checks
- Deletion detection: missing files traced back from linker errors
- Full decision log: **why** something is or isn't rebuilt
- Incremental builds: subsequent builds compare graph against reality

### Layer 7: Plugin / Event Bus

Hooks into specific events, streams logs, exerts control, injects data.

**Events:**
- `On_Build_Start`
- `On_Config_Load`
- `On_Command_Pre`
- `On_Command_Post`
- `On_Module_Load`
- `On_Build_End`

**Plugin signals:**
- `SHMOO_STATUS_OK`
- `ABORT`
- `DELAY`
- `PATCH`

**Principles:**
- Events are versioned and typed
- Plugins are isolated — code does not leak between them
- The base executor unpacks and routes event streams
- Plugins can stream logs, exert control, or inject data

### Layer 8: Glass House

Embedded web dashboard.

- Real-time layer status, log stream, anomaly highlighting
- `http://localhost:<port>`
- AI-adversarial mode: identifies missed optimization opportunities
- Full build decision history
- Interactive graph visualization

---

## 2. Walkback Resource Search

Resources are discovered by walking back from the execution symlink to find local overrides, then following the chain to find system defaults. Deduplication prevents redundant paths.

**Example chain:** `$HOME/bin/make` → `/usr/local/bin/shmoo` → `/opt/shmoo/bin/shmoo`

**Search order:**

1. `$HOME/lib/shmoo/v1.2.3/` — exact patch match from user layer
2. `$HOME/lib/shmoo/v1.2/` — broad minor match from user layer
3. `/usr/local/lib/shmoo/v1.2.3/` — exact patch match from link layer
4. `/usr/local/lib/shmoo/v1.2/` — broad minor match from link layer
5. `/opt/shmoo/lib/shmoo/v1.2.3/` — exact patch match from target layer
6. `/opt/shmoo/lib/shmoo/v1.2/` — broad minor match from target layer

**Principles:**
- User-local resources always take priority
- Each symlink in the chain adds a corresponding resource directory
- Paths are deduplicated as they are stacked
- Version-aware: only load modules matching the calling script's version (or broader minor version)

---

## 3. Versioning Rules

- **Exact:** `v1.2.3` loads from `v1.2.3/` only
- **Broad:** `v1.2.3` ALWAYS loads from `v1.2/` (if exists)
- **Rejection:** `v1.2.3` IGNORES `v1.2.0/` (and vice-versa)
- **Dynamic update:** changing `v1.2` symlink updates all v1.2.x scripts simultaneously

**Patch Isolation:** v1.2.3 does not load from v1.2.0.  
**Broad Matching:** v1.2.3 CAN load from v1.2/.

---

## 4. State Bridge / Memory Abstraction

Shared state between layers is abstracted as a "Memory Bridge." Different OS layers may access it via different mechanisms — `mmap`, `virtio-fs`, sockets, shared temp files. The bridge normalizes this.

**Principles:**
- Single source of truth for build state
- Version-specific modules (Patch Isolation: v1.2.3 does not load from v1.2.0)
- Broad version matching (v1.2.3 CAN load from v1.2/)
- Symlink-updatable (change `v1.2` → `v1.2.3` to update all v1.2.x scripts)

---

## 5. Origin — Data Provenance

Every value in the build carries its origin — traceable back to source. This is not optional. This is the foundation of audit-grade traceability.

A value's provenance is a **chain** of origins, not a single record. Walk back through the chain to see every file, every expansion, every substitution that produced the final value.

**Origin types:**
- `Origin::File` — read directly from a file (source path, line, col, file hash)
- `Origin::Expand` — produced by variable expansion or transformation (operation, input ref, trace)
- `Origin::Env` — from environment variables (name, pid, uid, inherited)
- `Origin::System` — from syscalls (call name, arguments, result, timestamp)
- `Origin::Command` — from command substitution (command, args, stdin/stdout origins, cwd, env origin)
- `Origin::Buffer` — from internal computation (parent ref, operation, size, shared)

**Integration:**
- Every token produced by a parser carries an origin
- A parsed directive carries the aggregate origin of all its tokens, plus any computed values
- When a variable is stored, its value and origin are stored together; modification chains are tracked

**Audit queries:**
- "Why does this variable have this value?" — walk back through the chain to the source file
- "Where did this file path come from?" — reconstruct the concatenation chain
- "Was this value ever modified?" — single entry = set once; multiple entries = overrides with origins

**Implementation:** C backend required. Every operation in a build fires events; Perl overhead on billions of operations chokes the event loop. Origin data is packed C structs. Perl accesses via `unpack()` — fast, no overhead.

---

## 6. Lexer-Driven Parser

The parser is a **driver directed by a lexer**. The lexer owns the grammar — it knows what tokens exist and where they match in the input string. The parser's intent is handled by a callback function.

**Architecture:**

```
Input String
    │
    ▼
┌─────────────────────┐
│   Grammar (Lexer)   │  ←  Large regex / token definitions
│                     │
│  Matches a token    │
│  at position N      │
└────────┬────────────┘
         │
         │ calls callback with:
         │  1. Context (injected by caller)
         │  2. Match information (token text, position, grammar rule)
         ▼
┌─────────────────────┐
│  Callback Function  │  ←  Determined by parser purpose
│                     │
│  May:               │
│    • Build a token  │  ← Accept the match as-is
│    • Demur          │  ← Reject, return chars
│                     │    as raw text for
│                     │    enclosing token
└────────┬────────────┘
         │
         │ result fed back to parser
         ▼
     Parser State Machine
```

**Key principles:**
1. Lexer owns the grammar. It knows what tokens exist and where they match.
2. Lexer calls a callback for each match. The callback receives context + match data.
3. Callback decides whether to accept the token or demur.
4. Same grammar, different parsers — one lexer/grammar can drive syntax checking, word splitting, or pipeline execution via injected callbacks.

---

## 7. Remote Execution — Linux → QEMU → Windows

When a build step requires an environment that differs from the host (e.g., Windows compilation from Linux host), Shmoo transitions execution via a demand-started QEMU VM with guest-agent command execution.

**Two transition modes:**

1. **Daemon mode**: The subordinate execution spawns a dedicated daemon whose sole job is monitoring and communication relay. The actual build runs in the target environment; the daemon passes events back to the root daemon.
2. **exec() mode**: The subordinate execution directly calls `exec()` on the target interpreter. This preserves open file handles, environment state, and the full configuration chain without an intermediate daemon. Feasible only when `exec()` is possible (same OS, different interpreter or different WINEPREFIX, etc.).

**Three-phase workflow:**
```
detect(vm) → start_if_needed(vm) → wait_for_agent(vm) → guest_exec(vm, cmd) → poll_exit(vm, pid) → parse_result
```

**Phase 1 — Detect:** Check if the target VM is running via `virDomainGetState()` (libvirt C API) or `query-status` (direct QMP).

**Phase 2 — Start:** If stopped, call `virDomainCreate()` (non-blocking). If paused, call `virDomainResume()`.

**Phase 3 — Execute:** Poll `guest-info` until the guest agent responds. Then send `guest-exec` command JSON via `virDomainQemuAgentCommand()`. Poll `guest-exec-status` with the returned PID until completion.

**Tool choice:** libvirt C API is preferred (structured error handling, domain management, multi-hypervisor support). Direct QMP is fallback when libvirtd is unavailable.

**Output:** stdout/stderr are base64-encoded by the guest agent. Decoded by the stream bridge before passing to the next layer.

---

## 8. Prediction and Rebuild

The system supports two modes of operation: **recording** (initial build of an opaque build system like Make) and **prediction** (rebuilding using the learned model).

**Recording mode:**
- The system runs a native build tool (e.g., Make) and monitors it via the Black Box Monitor.
- Because Makefiles are executed as they are parsed, the full dependency graph is not known until execution completes.
- The Monitor captures the entire graph: every rule fired, every file opened, every flag passed.
- This graph becomes the basis for the Shmoo build description language.

**Prediction mode:**
- The learned model is evaluated against the current source tree.
- Unlike Make, where the graph is discovered through execution, the learned model is a static structure that can be evaluated without running the original tool.
- This produces deterministic predictions about what will be rebuilt, based solely on file hashes and dependency relationships.

**Novelty — Learning from Dynamic Systems:**
Prior build systems all require the build graph to be known in advance. Make discovers it at runtime but cannot replay it deterministically. Shmoo learns the graph from a one-time execution of the native build, then uses the learned graph for all subsequent builds. This bridges the gap between Make's dynamic nature (it discovers dependencies through execution) and the predictability required for hermeticity and audit-grade traceability.

---

## 9. Plugin Architecture

The system is explicitly designed for plugin extensibility. Plugins hook into named events, can read, write, or alter the data flowing through those events, and can signal `OK`, `ABORT`, `DELAY`, or `PATCH` to the base executor.

**Event types:**
- `On_Build_Start` — build is initializing
- `On_Config_Load` — a configuration file has been read
- `On_Command_Pre` — a build command is about to execute
- `On_Command_Post` — a build command has completed
- `On_Module_Load` — a build module has been loaded
- `On_Build_End` — the build has completed

**Novelty — Bidirectional Plugin Data Flow:**
Most build systems treat plugins as either observers (they can read state) or injectors (they can modify state, but only through a fixed API). Shmoo plugins can do both on the same event:

1. **Read**: inspect the configuration chain, the provenance of any value, the state of any layer
2. **Write**: modify the configuration chain, add provenance annotations, insert new build rules
3. **Alter**: change the data flowing through the event stream (e.g., rewrite a file path, modify a compiler flag, inject a new dependency)
4. **Signal**: `OK` (proceed), `ABORT` (fail the build), `DELAY` (wait for external condition), `PATCH` (the build graph has changed)

This allows plugins to implement a wide range of behaviors — from simple variable tracking ("report when `CFLAGS` changes") to complex audit pipelines ("record every file opened by every command, produce a SLSA attestation") — without modifying the base system.

---

## 10. Remote Execution — Linux → Wine → Windows Perl

**Next phase:** Linux → Wine → Windows Perl interpreter.

Transfers execution from Linux to a Windows Perl interpreter running under Wine. The stream bridge normalizes the boundary between Linux processes and Wine's Win32 emulation layer.

**Known limitations (from QEMU remote execution):**
- One-shot exec only in current implementation
- Long-running daemons need separate monitoring logic
- Guest must have qemu-ga installed — not automatic with QEMU
- Runs as SYSTEM level, not user context
- Frozen guest — no recovery from guest crash except reboot

---

## 11. Technical Decisions

### License: BSD 2-Clause

Chosen over MIT because Apple (the target adopter as GNU Make replacement) recognizes and trusts BSD. Less modern "permissive-license fatigue" in enterprise Unix environments.

### Implementation Strategy

1. **Pure Perl first** — validates architecture before C commitment
2. **C extensions loaded when available** (POSIX executor), fall back to Perl (Windows/Wine)
3. **Performance-critical paths:** `Inline::C` for globbing, `mmap()`/`mremap()` for shared memory cache

### mTLS Daemon

Shmoo includes an mTLS daemon for secure inter-layer communication in distributed builds. This daemon handles authentication between layers that span network boundaries (e.g., host → VM, VM → remote build worker).

**Novelty — Distributed Build Provenance:**
In a distributed build (e.g., the same build running on multiple hosts), the daemon maintains a **single provenance chain** that spans all hosts. Each host's build events are tagged with a host identifier and a sequence number, and the daemon merges them into a single ordered stream. An auditor can reconstruct the build state at any point in time, even if multiple hosts were executing build steps concurrently.

### Global Jobservers

A global jobserver mechanism limits concurrent build tasks across all layers, preventing resource exhaustion. This mirrors GNU Make's `--jobs` model but extends it across environment boundaries (host → VM → Wine layer).

---

## 12. Anti-Patterns to Avoid

- **Monolithic executors** — one huge binary that tries to do everything. Decompose into layers.
- **Implicit state** — state that lives only in memory or process globals. Persist to manifest files.
- **Hardcoded paths** — always resolve resources relative to a known anchor.
- **Version mixing** — a v1.2.3 module loading from a v1.2.0 directory.
- **Silent failures** — if a layer cannot spawn the next, the build must abort with full context.
- **Event storm** — plugins that fire too many events cause overhead. Batch where possible.
- **One-shot only** — the current exec model does not support daemon monitoring. This is a known gap.

---

## 13. Checklist

When designing a complex system, verify:

- [ ] Is there a clear separation between the launcher (chain resolver) and the worker (build executor)?
- [ ] Can any layer be launched independently as the root?
- [ ] Is the resource search path deterministic and deduplicated?
- [ ] Does the versioning scheme prevent cross-contamination between patch levels?
- [ ] Can the system be audited at any layer without losing context?
- [ ] Is there a fallback for missing specialized modules?
- [ ] Are I/O streams preserved across environment boundaries?
- [ ] Can plugins hook and control the build without modifying core logic?

---

*This document is the brain. PROJECT_LOG.md is the conversation history.*
