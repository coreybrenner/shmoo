# Build Hermeticity — Technical Study Guide

**Study guide for:** Audit-grade traceability, reproducibility, build security  
**Related Shmoo feature:** Black Box Monitor, Layer 4, provenance tracking  
**Date:** 2026-08-19

---

## Table of Contents

1. [What Is Build Hermeticity?](#1-what-is-build-hermeticity)
2. [Why Hermeticity Matters](#2-why-hermeticity-matters)
3. [Mechanisms for Achieving Hermeticity](#3-mechanisms-for-achieving-hermeticity)
4. [Hermeticity in GNU Make](#4-hermeticity-in-gnu-make)
5. [Hermeticity in Bazel](#5-hermeticity-in-bazel)
6. [Hermeticity in Nix](#6-hermeticity-in-nix)
7. [Hermeticity in Cargo](#7-hermeticity-in-cargo)
8. [Hermeticity in Meson + Ninja](#8-hermeticity-in-meson--ninja)
9. [Hermeticity in CMake](#9-hermeticity-in-cmake)
10. [Comparison Matrix](#10-comparison-matrix)
11. [Implications for Shmoo](#11-implications-for-shmoo)

---

## 1. What Is Build Hermeticity?

**Hermeticity** in build systems means the build produces the same output for the same inputs, regardless of the host environment, timing, network availability, or other external factors. A hermetic build is **fully self-contained**: every dependency, every compiler flag, every environment variable is explicitly specified and controlled.

### The Three Degrees of Hermeticity

**Level 1 — Deterministic Output**  
Same inputs → same output. The build can be repeated and verified. This requires stripping timestamps, sorting file lists, and using deterministic compiler flags.

**Level 2 — Sandboxed Execution**  
The build does not rely on system-installed tools or libraries. Every tool is explicitly declared and isolated from the host. The build runs in an isolated environment (sandbox, container, chroot).

**Level 3 — Fully Self-Contained (The Gold Standard)**  
The build system downloads, verifies, and uses every dependency it needs. No network access is required at build time (after initial setup). The build environment is a reproducible artifact that can be recreated anywhere.

Nix operates at Level 3. Bazel can reach Level 3. GNU Make typically operates at Level 0 (not hermetic at all).

### The Core Principle

Every byte that enters the build must have a known source. Every tool that runs must have a known identity. Every environmental factor that could affect the output must be either fixed or recorded. If you cannot reconstruct the exact build environment from the recorded data, the build is not hermetic.

---

## 2. Why Hermeticity Matters

### Security (Supply Chain Integrity)

- If the build depends on a network fetch, the fetch can be intercepted (MITM, DNS poisoning, compromised CDN).
- If the build depends on system-installed libraries, those libraries can be updated without the build knowing.
- Without hermeticity, an attacker can modify a system library (e.g., `libc`) and every build that links against it silently changes — the attacker's code runs, but the build looks identical.

### Audit-Grade Traceability

- Government, defense, and regulated industries require proof of build integrity.
- Hermetic builds produce **provenance attestation**: a cryptographic record of every input, every tool, every flag used.
- This attestation can be verified by an auditor without re-running the build.

### Reproducibility

- Debugging a bug requires knowing exactly what was built and how.
- If the build is not hermetic, two builds at different times may produce different outputs for the same source code, making debugging impossible.
- Hermeticity allows **bit-for-bit reproducibility**: `sha256sum binary1 == sha256sum binary2`.

### Portability

- A hermetic build does not depend on the host environment.
- It can be built on any machine, in any CI system, with the same result.
- This is critical for large teams and distributed build environments.

---

## 3. Mechanisms for Achieving Hermeticity

### 3.1 Content-Addressable Storage (CAS)

Instead of storing files by path (e.g., `/usr/lib/libfoo.a`), store them by content hash (e.g., `sha256:e3b0c44298...`). If the content hasn't changed, the file can be reused from cache without re-fetching or re-building.

**Examples:**
- Nix: every store path is a hash of its inputs
- Bazel: remote cache is content-addressed
- Cargo: registry entries are content-addressed

### 3.2 Sandboxing

Isolate the build environment so it cannot access files, tools, or services outside its declared scope.

**Techniques:**
- **`chroot` / `chrootless`** — restrict filesystem view
- **Mount namespaces** — Linux `CLONE_NEWNS`, Bazel's sandboxing
- **cgroups** — limit CPU, memory, network access
- **seccomp** — restrict syscalls (e.g., no `connect`, no `read` outside declared paths)
- **Containers** — Docker, podman, glibc-nanos

### 3.3 Dependency Pinning

Every dependency is pinned to a specific version, commit, or hash. No "latest" or "current" — only an exact identifier.

**Techniques:**
- **Checksums** — MD5, SHA-256 of downloaded artifacts
- **Git commits** — exact SHA-1 or SHA-256 hash
- **Lockfiles** — `cargo.lock`, `go.sum`, `buck.lock`
- **Nix derivations** — declarative hash-based dependency graph

### 3.4 Environment Capture

Record every environment variable, every compiler flag, every file path that could affect the build.

**Techniques:**
- **Strace / ptrace** — trace all syscalls, record every file opened, every network connection
- **LD_DEBUG** — capture dynamic linker behavior
- **Environment serialization** — dump `env` to a file before build, compare between builds
- **Compiler flags** — record `-D`, `-I`, `-L`, `-W` flags used

### 3.5 Deterministic Builds

Ensure the build output is identical across environments by eliminating non-determinism.

**Sources of non-determinism and their fixes:**
| Source | Fix |
|--------|-----|
| Timestamps | Use a fixed epoch (`SOURCE_DATE_EPOCH=0`), strip build timestamps |
| File order | Sort file lists alphabetically (or use a deterministic ordering) |
| Compiler paths | Use `-ffile-prefix-map` to canonicalize paths |
| Randomness | Disable compiler randomization flags, use fixed seeds |
| Host names / PIDs | Replace with deterministic placeholders |
| Debug info | Strip debug info or use deterministic DWARF generation |

### 3.6 Provenance Attestation

Produce a machine-readable record of the build that can be independently verified.

**Standards:**
- **SLSA** (Supply-chain Levels for Software Artifacts) — a framework for build integrity
- **SPDX** — software bill of materials (SBOM) format
- **In-toto** — attestation framework for build provenance
- **Bazel Build Event Protocol (BEP)** — structured JSON events from every build action

---

## 4. Hermeticity in GNU Make

### Current State: Minimal Hermeticity

GNU Make is fundamentally a **task runner**, not a hermetic build system. It executes recipes as shell commands, has no knowledge of inputs or outputs beyond file modification times, and provides no isolation.

### How Make Handles Environment

- **`MAKEFLAGS`** — can be passed to recursively invoke Make with the same flags, but this is a convenience, not a hermeticity guarantee.
- **`.EXPORT_ALL_VARIABLES`** — exports all Make variables to environment, but this can leak unintended state.
- **`--environment-overrides`** — command-line variables override makefile variables, but this is the opposite of hermeticity (external influence on the build).
- **`vpath`** — search paths for source files, but no isolation from system files.

### What Make Lacks

1. **No input/output tracking** — Make only checks file modification times. If a header file changes but the .o file is newer, Make won't rebuild (stale state). If a tool changes between builds, Make doesn't know.
2. **No sandboxing** — recipes run in the host environment. Every build depends on the system's PATH, installed libraries, compiler versions, etc.
3. **No dependency pinning** — Makefiles rarely pin compiler versions or toolchain hashes.
4. **No provenance attestation** — nothing records what was built, with what flags, from what sources.
5. **No deterministic build support** — no built-in mechanism for stripping timestamps or ensuring output reproducibility.

### Workarounds (Not True Hermeticity)

- **`docker build`** — wraps Make in a container, achieving hermeticity at the container level, but not at the Make level.
- **`make -p`** — dumps the Make database (variables, rules, dependencies), but this is a snapshot, not a verification tool.
- **`make --dry-run`** — shows what would be executed, but does not record or verify execution.

### Conclusion for Shmoo

Make's lack of hermeticity is precisely what Shmoo's Black Box Monitor and provenance tracking solve. Shmoo intercepts the Make invocation, records every file opened, every flag passed, every output produced, and builds a verifiable record that Make itself cannot produce.

---

## 5. Hermeticity in Bazel

### Overview

Bazel is one of the most mature hermetic build systems. It was designed from the ground up for hermeticity at Google scale (building the entire Android OS, Chrome, and Google.com from a single source tree).

### How Bazel Achieves Hermeticity

**1. Action Graph — Explicit Dependencies**  
Every build action (compile, link, copy) has explicit input and output declarations. Bazel does not rely on file modification times or shell globbing. The graph is fully defined before execution.

**2. Sandboxing (Hermetic Sandboxes)**  
Each action runs in an isolated sandbox created by the Bazel daemon (`bazel-d`). The sandbox is a fresh `chroot` + mount namespace where:
- Only declared input files are visible
- Only declared output paths can be written to
- Network access is disabled
- `/tmp` is isolated
- `readlink`, `stat`, `openat` syscalls are intercepted

Bazel's sandboxing uses Linux namespaces and cgroups:
```bash
# Bazel spawns a sandbox like this internally:
unshare --mount --pid --fork --mount-proc
chroot /tmp/bazel-sandbox-xyz/
exec /usr/bin/gcc -c input.c -o output.o
```

**3. Content-Addressable Remote Cache**  
Outputs are stored in a content-addressable cache. If the inputs (and their hashes) have not changed, the cached output is reused without re-execution. The cache is remote-capable (shared across machines).

**4. Toolchain Pinning**  
Every tool (compiler, linker, archiver) is a declared Bazel `toolchain`. The toolchain is a fixed binary or a `container_image` rule. There is no "use the system gcc" — the toolchain is explicitly versioned and pinned.

**5. Build Event Protocol (BEP)**  
Every build action is recorded as a structured JSON event:
```json
{
  "id": { "action_id": "abc123" },
  "startTime": "2026-08-19T12:00:00Z",
  "endTime": "2026-08-19T12:00:05Z",
  "commandLine": "gcc -O2 -c input.c -o output.o",
  "inputs": [
    { "path": "input.c", "digest": "sha256:..." }
  ],
  "outputs": [
    { "path": "output.o", "digest": "sha256:..." }
  ]
}
```

**6. `--sandbox_debug` and `--verbose_failures`**  
Debug flags that show sandbox contents and verbose error messages, useful for diagnosing hermeticity issues.

### Bazel's Hermeticity Limitations

1. **Linux-first** — sandboxing relies heavily on Linux namespaces. macOS support exists but is less thorough (uses a simpler chroot-like approach). Windows support uses Docker or a different sandboxing layer.
2. **Starlax restricts power** — build rules are written in Starlax (a subset of Python), which limits what rules can do. This is by design (to prevent non-deterministic behavior) but can be frustrating for complex toolchains.
3. **Cold start overhead** — building a fresh sandbox for every action has a startup cost (typically 100-500ms per action).

### Bazel Relevance to Shmoo

Bazel's provenance is closer to what Shmoo aims for, but Bazel focuses on the build itself. Shmoo's unique angle is the **cross-environment transition** — tracking environment changes across boundaries (Linux → QEMU → Wine → native) in addition to build provenance.

---

## 6. Hermeticity in Nix

### Overview

Nix is the most aggressively hermetic build system. It was designed by Eelco Dolstra specifically to solve the "it works on my machine" problem. Every build is fully isolated, every dependency is versioned by hash, and every package is stored in `/nix/store/<hash>-<name>`.

### How Nix Achieves Hermeticity

**1. Pure Functions — Declarative Builds**  
Every Nix package is described by a **Nix expression** (a function in the Nix language) that returns a derivation: a description of how to build the package. The derivation is a pure function: same inputs → same output. No shell scripts with implicit dependencies.

```nix
# Example: simple C package in Nix
{ stdenv, fetchurl }:

stdenv.mkDerivation {
  name = "hello-2.12";
  src = fetchurl {
    url = "https://ftp.gnu.org/gnu/hello/hello-2.12.tar.gz";
    sha256 = "0ssi1...";  # SHA-256 hash of the source tarball
  };
  # No need to declare GCC or make — stdenv provides them
}
```

The derivation is a **declarative specification**. Nix evaluates it to produce a store path like `/nix/store/abc123-hello-2.12`.

**2. Hash-Based Store Paths**  
Every file in `/nix/store` is stored by a hash of its inputs (source code, dependencies, build script, compiler flags). If any input changes, the hash changes, and the file gets a new path. This means:
- No overwriting: old versions are preserved (allowing rollbacks)
- No implicit dependencies: if a dependency changes, the hash changes
- No shared mutable state: every package is isolated

**3. Isolated Builds (Chroot + Restricted Tools)**  
Nix builds run in an isolated chroot where:
- `/nix/store` is the only filesystem root (plus `/proc` and `/dev`)
- `wget` is patched to reject non-hashable URLs
- `make` is patched to reject non-deterministic behavior
- Network access is disabled by default
- The build script cannot access the host system at all

```bash
# Nix builds are conceptually like this:
chroot /nix/store/xyz-builder /nix/store/abc-buildscript.sh
# Inside the chroot: only /nix/store is visible.
# No /usr/bin/gcc. No /etc/ld.so.cache. No network.
```

**4. Dependency Pinning by Hash**  
Every dependency in a Nix expression is pinned by SHA-256 hash. If the upstream changes, the hash changes, and the build fails until the expression is updated.

**5. Deterministic Builds**  
Nix enforces deterministic builds by:
- Patching build tools (e.g., `make` is patched to fail on non-deterministic behavior)
- Setting `SOURCE_DATE_EPOCH` for timestamps
- Using `--reproducible` flags for GCC, Binutils
- Requiring package authors to fix non-determinism in their build scripts

### Nix's Strengths

- **True hermeticity** — a Nix build can run on any machine with the same result, even if the machine has no internet access (after initial evaluation).
- **Rollback** — since `/nix/store` paths are immutable, you can always roll back to a previous build.
- **Atomic installs** — `nix-env -i` installs atomically; if it fails, nothing is changed.
- **Shared store** — multiple users can share the same Nix store, saving disk space.

### Nix's Weaknesses (Relevant to Shmoo)

1. **Not cross-environment** — Nix is designed for a single environment (Linux with Nix). It does not handle transitions across environments (Linux → Windows, host → VM). This is where Shmoo differs.
2. **Complex language** — the Nix expression language is declarative but has a steep learning curve. Shmoo uses Perl, which is more familiar and more imperative.
3. **Build-time only** — Nix builds a package and stops. Shmoo's goal is a **continuous build environment** with plugin hooks, real-time monitoring, and cross-environment transitions.
4. **No daemon for monitoring** — Nix has no equivalent to Shmoo's daemon/BEP layer. Each build is independent.

### Nix Relevance to Shmoo

Nix is the gold standard for build hermeticity in a single environment. Shmoo aims to replicate Nix's hermeticity guarantees while adding cross-environment transition tracking. The provenance chain in Shmoo could incorporate Nix-style content-addressable storage for build artifacts, while adding environment transition metadata on top.

---

## 7. Hermeticity in Cargo (Rust)

### Overview

Cargo is Rust's build tool and package manager. It achieves hermeticity through dependency pinning, hash-based registry entries, and deterministic build outputs.

### How Cargo Achieves Hermeticity

**1. Cargo.lock — Dependency Pinning**  
`Cargo.lock` records the exact version and source of every dependency. When building, Cargo reads `Cargo.lock` and uses exactly those versions. No "latest" or "current".

**2. Git Commit Pinning**  
Dependencies from git repos are pinned by exact commit hash:
```toml
[dependencies]
mylib = { git = "https://github.com/user/mylib", rev = "abc123def456" }
```

**3. Crates.io Checksums**  
Every crate uploaded to Crates.io must have a SHA-256 checksum. Cargo verifies the checksum before building. If the crate changes on the server, Cargo's cache will reject it and re-download.

**4. Deterministic Builds (Rustc)**  
Rustc supports deterministic builds via:
- `-C link-dead-code=no` (disable dead code that might include debug info with timestamps)
- Compiler flags like `-Z build-std` to ensure consistent standard library builds
- No timestamps in binary output by default

**5. Cross-Compilation**  
Cargo supports cross-compilation with toolchain-specific configurations (`[target.<triple>]`). Each target has its own dependency resolution and build script.

### Cargo's Hermeticity Limitations

1. **Registry dependency** — Cargo downloads from Crates.io by default. If Crates.io is down or compromised, the build fails or uses compromised artifacts.
2. **Build scripts** — `build.rs` scripts run in the host environment and can access the network, modify files, and run arbitrary code. This is a hermeticity vulnerability.
3. **No sandboxing** — Cargo does not sandbox build scripts. A malicious `build.rs` can do anything on the host.
4. **No provenance attestation** — Cargo does not record build provenance or produce attestation documents.

### Cargo Relevance to Shmoo

Cargo's dependency pinning model (lockfile + hashes) is a good pattern for Shmoo's provenance tracking. The cross-compilation model is also relevant — Shmoo transitions across environments, and Cargo's `[target.<triple>]` approach provides a template for environment-specific dependency resolution.

---

## 8. Hermeticity in Meson + Ninja

### Overview

Meson is a build system that generates Ninja build files. Ninja is the actual build executor. Together, they provide moderate hermeticity, mainly through dependency pinning and deterministic build output.

### How Meson + Ninja Achieve Hermeticity

**1. Meson WrapDB — Dependency Pinning**  
Meson WrapDB is a registry of subprojects (dependencies) with declared hashes and update policies. Each subproject is pinned by version or hash.

**2. `meson setup --reconfigure`**  
Re-running `meson setup` regenerates the Ninja files from scratch, ensuring the build graph is consistent.

**3. Ninja's `-d explain`**  
Ninja can explain why a rule was rebuilt (stale dependency). This helps detect non-hermetic behavior (e.g., a file that shouldn't have changed did change).

**4. Deterministic Ninja Files**  
Ninja build files are generated deterministically from Meson. If the Meson script is the same, the Ninja file is the same.

### Limitations

- **No sandboxing** — Ninja executes commands in the host environment.
- **No provenance attestation** — nothing records what was built or with what flags.
- **Weak dependency isolation** — dependencies are declared in Meson, but there's no content-addressable storage or hash verification by default.

---

## 9. Hermeticity in CMake

### Overview

CMake is a cross-platform build system generator. It generates build files for Make, Ninja, or IDEs. Its hermeticity depends entirely on how it's used.

### How CMake Handles Hermeticity

**1. `find_package()` — Dependency Resolution**  
CMake's `find_package()` searches the system for libraries. This is **not hermetic** by default — it depends on the host environment. However, CMake supports:
- **Config-mode find modules** — specifying an exact path (`-DCMAKE_PREFIX_PATH=/path/to/lib`)
- **Package configuration files** — `.cmake` files that declare exact versions

**2. `try_compile()` — Build Testing**  
CMake can compile and run test code during configuration. This can detect system capabilities but also introduces non-determinism (test results may vary by host).

**3. `CMAKE_DISABLE_FIND_PACKAGE`**  
Disable automatic find_package calls, forcing explicit dependency specification.

### CMake's Hermeticity Limitations

1. **Host-dependent by default** — `find_package()` searches the system PATH and standard library directories. Two different hosts will produce different CMake configurations.
2. **No sandboxing** — CMake generators execute commands in the host environment.
3. **No provenance attestation** — nothing records the build configuration or execution.
4. **Build file generation is not deterministic** — CMake's output depends on the host environment (detected compiler version, available libraries, etc.).

### CMake Relevance to Shmoo

CMake demonstrates the problems Shmoo solves: a build system that depends on host detection, with no hermeticity guarantees. Shmoo's Monitor and provenance tracking would allow CMake builds to be recorded and audited, even if CMake itself doesn't provide hermeticity.

---

## 10. Comparison Matrix

| Feature | Make | Bazel | Nix | Cargo | Meson+Ninja | CMake |
|---------|------|-------|-----|-------|-------------|-------|
| **Dependency pinning** | None | Yes (toolchains) | Yes (SHA-256) | Yes (lockfile) | Yes (WrapDB) | Manual |
| **Sandboxing** | None | Yes (namespaces) | Yes (chroot) | No | No | No |
| **Content-addressed storage** | No | Yes (remote cache) | Yes (`/nix/store`) | No | No | No |
| **Provenance attestation** | No | Yes (BEP) | Partial (store paths) | No | No | No |
| **Deterministic output** | No | Yes | Yes | Yes (with flags) | Yes | No |
| **Cross-environment transitions** | No | Limited | No | Partial (cross-compile) | Limited | No |
| **Network at build time** | Allowed | Allowed (but cacheable) | Not allowed (after eval) | Allowed (Crates.io) | Allowed | Allowed |
| **Audit-grade traceability** | No | Yes | Partial | No | No | No |

---

## 11. Implications for Shmoo

### What Shmoo Can Learn

1. **Nix's approach** — content-addressable store paths for build outputs. Every artifact gets a hash based on its inputs.
2. **Bazel's approach** — BEP for build event recording. Shmoo's daemon could produce BEP-compatible JSON events.
3. **Cargo's approach** — lockfile-style provenance recording for dependencies.
4. **All systems' failures** — no system combines hermeticity with cross-environment transition tracking. This is Shmoo's unique contribution.

### What Shmoo Will Do Differently

1. **Cross-environment transition tracking** — record every environment shift (Linux → QEMU → Wine → native) with full context preservation.
2. **Plugin-based extensibility** — hooks that can read, write, or alter the build information flow.
3. **Real-time monitoring** — the daemon tracks events in real-time, not just at build completion.
4. **Audit-grade traceability** — every value carries its origin. Every build step is recorded with its inputs, outputs, flags, and provenance.
5. **Prediction and rebuild** — learn from the initial opaque build (e.g., Make's parsed-as-executed behavior) and produce a deterministic model for subsequent rebuilds.

### The Shmoo Hermeticity Model

```
Host Environment (Linux)
  │ Layer 1: Launcher — discovers environment, spawns next layer
  ▼
  │ Layer 2: Wrapper — masquerade as make/nmake/cmake
  ▼
  │ Layer 3: Stream Bridge — FD passthrough, secure temp files
  ▼
  │ Layer 4: Monitor — Black Box, logs every operation
  │          ├─ Records every syscall (ptrace/inotify)
  │          ├─ Records every file opened
  │          ├─ Records every flag and env var
  │          └─ Produces BEP-compatible JSON events
  ▼
  │ Environment Transition (e.g., Linux → QEMU → Windows)
  │          ├─ Records: environment type, transition reason, tools used
  │          ├─ Preserves: interpreter command line, -I flags, env vars
  │          └─ Attaches: origin chain to all recorded data
  ▼
  │ Layer 5: Parser — CIR output
  ▼
  │ Layer 6: Graph — DAG with semantic hashing
  ▼
  │ Layer 7: Plugin / Event Bus — extensibility
  ▼
  │ Layer 8: Glass House — HTTP server, interactive dashboard
  ▼
  Output: provenance-attested build artifact
```

The key insight: hermeticity is not just about sandboxing and pinning dependencies. It's about **recording the entire chain of custody** from source code to final binary, across every environment boundary, with every transition documented and verifiable.

This is the gap no existing build system fills, and it's what makes Shmoo's design unique.
