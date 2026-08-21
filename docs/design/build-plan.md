# Shmoo Build Plan — Architecture & Execution

*2025-01-23 | Status: Design Draft*
*Purpose: Comprehensive design document for the Shmoo build orchestrator, including 9p integration, strict environment isolation, and the "Delusion Shell".*

---

## 1. The Build Graph (DAG)

Every build is a **Directed Acyclic Graph (DAG)** of **Actions**. An action is an atomic build step (compile, link, copy, etc.) with explicitly declared inputs, outputs, and dependencies.

### Action Components

Each action has:
- **ID:** Unique identifier (e.g., `cc_compile:src/main.c`)
- **Type:** Build action type (`cc_compile`, `cc_link`, `cp`, `test`, etc.)
- **Inputs:** List of input files with content hashes, tool binaries with hashes, and declared flags
- **Outputs:** List of expected output files with expected content hashes (for verification)
- **Environment:** Specific environment variable snapshot, `PATH`, `LD_PRELOAD`, working directory
- **Dependencies:** List of Action IDs that must complete before this action can run

### 9p Integration

Actions do not access the host filesystem directly. Instead, they interact with **9p Mounts**:
- **Input Mounts (Read-Only):** Source files, headers, libraries (e.g., `/inputs/src`, `/inputs/libs`)
- **Output Mount (Writable):** Build artifacts (e.g., `/outputs/build`)
- **Tool Mount (Read-Only):** The toolchain (e.g., `/tools/gcc`, `/tools/clang`)

These mounts are handled by the 9p Client (if remote) or local 9p Server (if local), mediated by the **Syscall Interceptor** (`LD_PRELOAD`).

---

## 2. Execution Engine

The execution engine is the core scheduler that orchestrates the build:

1. **Graph Resolution:** Parses build files (`shmoo-build`, `Makefile`) into a DAG
2. **Topological Sort:** Determines the correct execution order (dependencies first)
3. **Parallel Execution:** Runs independent actions in parallel, bounded by `--jobs=N`
4. **Sandboxing:** Each action runs in an isolated environment:
   - **Filesystem:** Only declared mounts are visible (via chroot/namespaces or 9p mounts)
   - **Network:** Disabled by default
   - **Environment:** Strictly controlled (see *Environment Isolation*)
5. **Result Collection:** Captures exit code, stdout/stderr, and output files
6. **Caching:** Results are cached by content-addressed hash (action key + input hashes)
7. **Verification:** Output hashes are compared against expected values; mismatches fail the build

---

## 3. Environment Isolation (The "Export" Knob)

### The Problem
Standard tools (Make, bash, cmake) inherit environment variables from the parent shell. This "environment bleed" is a primary cause of non-reproducible builds. Variables like `CFLAGS`, `PATH`, `LD_LIBRARY_PATH`, or `CC` silently influence builds in ways that are hard to audit or reproduce.

### The Solution: Explicit Exports
Shmoo introduces a configuration knob (`strict_env: true`) that changes the default behavior:
- **By Default:** Environment variables defined in a recipe/build rule are **local** to that action. They are **not** automatically exported to child processes (sub-shells, compilers, linkers).
- **Explicit Export:** To make a variable visible to children, the recipe must **explicitly export** it (e.g., `export CFLAGS="-O2"`)
- **Child Processes:** When a child process is spawned, it receives *only* the variables explicitly marked as exported in the parent's recipe.

#### Example (Makefile Compatibility)

**Standard Makefile (environment bleed):**
```makefile
CC = gcc      # Inherited from shell or make
CFLAGS = -O2  # Inherited from shell
all: app
    $(CC) $(CFLAGS) main.c -o app
```
*Problem:* If the developer's shell has `CFLAGS=-g` set, it overrides the Makefile's `CFLAGS`. The build is non-reproducible.

**Shmoo Makefile (strict env):**
```makefile
export CC = gcc      # Explicitly exported
export CFLAGS = -O2  # Explicitly exported
all: app
    $(CC) $(CFLAGS) main.c -o app
```
*Benefit:* If `CC` or `CFLAGS` is not exported, the compiler invocation fails because `gcc` is not found in the child's `PATH`. This enforces strict hermeticity.

### Auditable Multivariate Builds

Because every variable's visibility is explicit, Shmoo can generate a precise **Environment Report** for every build step:
- Which variables were defined in the recipe?
- Which variables were explicitly exported?
- Which variables did the compiler actually receive?
- Did any "leaked" variables (not explicitly exported) affect the build?

This makes the build **fully auditable** and **multivariate**: you can trace every compiler invocation back to exactly which variables and files influenced it. The build environment is no longer a "ghost" — it's a **first-class, auditable artifact**.

### Implementation

The execution engine enforces `strict_env` by:
1. **Clearing the environment** before spawning child processes
2. **Injecting only exported variables** from the recipe's environment definition
3. **Validating** that all required tools and inputs are declared (no implicit system dependencies)
4. **Logging** every variable passed to each child process for auditability

---

## 4. Make Compatibility Layer (Drop-in Replacement)

Shmoo can drop in place of Make and many other build systems by:
1. **Parsing Makefiles:** Translating Makefile rules into Shmoo actions
2. **Environment Translation:** When translating a Makefile, Shmoo respects `export` statements. Variables without `export` are treated as local (and thus won't leak to children).
3. **Shell Wrapper:** Shmoo's Makefile execution runs through a shell wrapper that enforces the `strict_env` policy.

#### The "Make Wrapper" Script

```bash
#!/bin/sh
# shbuild-make-wrapper.sh
# Runs a Makefile within Shmoo's strict environment policy.

set -eu

# 1. Parse the Makefile to extract 'export' statements.
exports=$(grep '^export' Makefile | sed 's/^export //')

# 2. Clear the environment (only keep essential system vars).
env -i PATH="/tools:/usr/bin" HOME="$HOME" TERM="$TERM"

# 3. Inject only the explicitly exported variables.
for var in $exports; do
    eval "$var"
    export "$var"
done

# 4. Execute the Makefile target.
make "$@"
```

This wrapper ensures that even if a developer runs `make`, Shmoo enforces the strict environment policy, preventing "environment bleed" from the developer's shell.

The wrapper can also:
- Parse Makefile `.PHONY`, `.SUFFIXES`, and implicit rules
- Translate Makefile variables into Shmoo environment definitions
- Generate Shmoo action graphs from Makefile dependencies
- Provide a seamless transition path from Make to Shmoo

---

## 5. The Delusion Shell (Interactive Development)

Shmoo can spawn an **interactive shell** that runs inside the build environment (the "Delusion Shell"):
- **Purpose:** Interactive debugging, development, and exploration within a reproducible environment
- **Execution:** `shbuild dev --action compile:app.c` or `shbuild dev`
- **Environment:** The shell is pre-configured with the exact same `PATH`, `LD_PRELOAD`, mounts, and environment variables as the build step
- **Capabilities:**
  - `cd` around the build tree (`/inputs/src`, `/outputs`, `/tools`)
  - Run tools (`gcc`, `make`, `gdb`, `vi`)
  - Edit files (`vi`, `nano`) — edits are written back to the 9p-mounted input tree
  - Debug the build step interactively
  - Run scripts that operate on the build environment

#### Why This Is Powerful

1. **Seamless Remote Development:** Developers can work in a fully reproducible, pre-configured environment without manually setting up toolchains or dependencies.
2. **Interactive Debugging:** Instead of just seeing a build failure in CI, a dev can `cd` into the exact failing build step's environment, inspect source code, run the compiler interactively, and fix the issue live.
3. **Scripting & Tooling:** The developer has a full userland available — `find`, `grep`, `awk`, `sed`, `vi`, `gcc` — all running in the delusion environment. They can write and test scripts that operate on the remote filesystem transparently.
4. **Sandboxed & Secure:** The 9p client and syscall interceptor ensure that the shell can only access what the build system explicitly mounts. No host filesystem bleed.
5. **Version-Controlled Environment:** The delusion environment itself is defined by the 9p mounts and the build recipe. If the recipe changes, the environment changes. It's fully versioned and reproducible.

#### Example Session

```bash
$ shbuild dev --action compile:app.c

# Inside the Delusion Shell:
$ pwd
/inputs/src

$ ls -la
main.c  main.h  Makefile

$ vi main.c
(editing...)

$ /tools/gcc/bin/gcc -o /outputs/app main.c
(compilation happens transparently over 9p, with strict env isolation)

$ /outputs/app
Hello, Delusion World!

$ exit
```

---

## 6. Configuration & Knobs

| Knob | Description | Default |
|------|-------------|---------|
| `strict_env` | If true, environment variables are not exported by default. Children only receive explicitly exported variables. | `true` |
| `parallel_jobs` | Maximum number of parallel build steps. | `NPROC` |
| `cache_enabled` | If true, cache build results by content hash. | `true` |
| `debug_shell` | If true, spawn an interactive shell on build failure. | `false` |
| `9p_remote` | URL of the remote 9p server (e.g., `tcp:9p.server:5640`). | (empty) |
| `sanitize` | If true, strip host-specific paths from compiler output. | `true` |
| `strict_mode` | If true, fail the build on any undeclared dependency or implicit tool. | `true` |

---

## 7. Implementation Plan

1. **Parser:** Write a parser for Shmoo build files (`shmoo-build`) and Makefiles
2. **DAG Builder:** Implement the graph builder (nodes, edges, topological sort)
3. **Environment Manager:** Implement the `strict_env` logic (local vs. exported variables)
4. **Execution Engine:** Implement the scheduler, sandboxing, and result collection
5. **9p Integration:** Connect to the 9p client/server for filesystem access
6. **Syscall Hook:** Integrate the `LD_PRELOAD` wrapper for filesystem interception
7. **Delusion Shell:** Implement the interactive shell interface
8. **Caching:** Implement the content-addressed cache
9. **Testing:** Write tests for environment isolation, Make compatibility, and 9p integration

---

## 8. Auditable Reporting

Shmoo generates a **Build Report** for every execution:
- **Graph:** The full DAG of actions
- **Environment:** The exact variables passed to each step (only explicitly exported ones)
- **Inputs/Outputs:** File hashes and content verification results
- **Tools:** Toolchain versions and hashes
- **Purity:** A list of any "impurities" detected (e.g., undeclared variables, unexpected tool access)
- **Signature:** Cryptographic signature (cosign/sigstore) for provenance

This report is versioned and can be attached to release artifacts, providing a **cryptographically verifiable audit trail** of every build.

---

## Summary

The Shmoo Build Plan ties together:
- **9p Client/Server** for transparent remote filesystem access
- **Syscall Interception** (`LD_PRELOAD`) for sandboxed file operations
- **Strict Environment Isolation** (explicit exports only) for auditable, reproducible builds
- **Make Compatibility** for drop-in replacement of existing build systems
- **The Delusion Shell** for interactive development in the build environment
- **Auditable Reporting** for cryptographically verifiable build trails

The result is a build system that is not just a build system — it's a **fully reproducible, auditable, interactive development environment** that can run anywhere (local, remote, VM, cloud) while maintaining strict hermeticity.
