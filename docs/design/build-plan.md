# Build Plan: Shmoo Core Orchestrator

*2025-01-23 | Status: Active Design*
*Purpose: The execution engine that ties 9p, syscall interception, and build recipes into a hermetic, auditable pipeline.*

---

## 1. The Build Graph (DAG)

Every build is a **Directed Acyclic Graph (DAG)** of **Actions**. An action is an atomic operation (compile, link, copy, script, test) with explicit inputs, outputs, and dependencies.

### Action Metadata
- **ID:** Unique identifier (e.g., `compile:src/main.c`)
- **Type:** `cc_compile`, `cc_link`, `sh_script`, `test`, etc.
- **Inputs:** List of files (with hashes) and tools (with hashes) required to execute the action.
- **Outputs:** List of expected output files and their expected content hashes (for verification).
- **Environment:** A specific snapshot of environment variables, `PATH`, and `LD_PRELOAD`.
- **Dependencies:** List of Action IDs that must complete before this one starts.

#### Example Graph
```
compile:base.o (inputs: base.c, gcc, libc.h)
   ↓
compile:lib.o (inputs: lib.c, gcc, lib.h, base.o)
   ↓
link:libfoo.so (inputs: base.o, lib.o, ld)
   ↓
link:app.bin (inputs: app.o, libfoo.so, ld)
   ↓
test:run (inputs: app.bin, test_runner)
```

### 9p Integration (Filesystem Access)
Actions do not access the host filesystem directly. Instead, they interact with **9p Mounts**:
- **Input Mounts (Read-Only):** Source files, headers, libraries (e.g., `/inputs/src`, `/inputs/libs`).
- **Output Mount (Writable):** Build artifacts (e.g., `/outputs/build`).
- **Tool Mount (Read-Only):** The toolchain (e.g., `/tools/gcc`).

These mounts are handled by the 9p Client (if remote) or local 9p Server (if local), mediated by the **Syscall Interceptor** (`LD_PRELOAD`).

---

## 2. Execution Engine

The execution engine is the core scheduler:

1.  **Graph Resolution:** Parses build files (`shmoo-build`, `Makefile`) into a DAG.
2.  **Topological Sort:** Determines the correct execution order (dependencies first).
3.  **Parallel Execution:** Runs independent actions in parallel, bounded by `--jobs`.
4.  **Sandboxing:** Each action runs in an isolated environment:
    - **Filesystem:** Only declared mounts are visible (via chroot/namespaces or 9p mounts).
    - **Network:** Disabled by default.
    - **Environment:** Strictly controlled (see *Environment Isolation*).
5.  **Result Collection:** Captures exit code, stdout/stderr, and output files.
6.  **Caching:** Results are cached by content-addressed hash (action key + input hashes). If a cached result exists, the action is skipped.

---

## 3. Environment Isolation (The "Export" Knob)

### The Problem
Standard tools (Make, bash) inherit environment variables from the parent shell. This "environment bleed" is a primary cause of non-reproducible builds.

### The Solution: Explicit Exports
Shmoo introduces a configuration knob (`strict_env: true`) that changes the default behavior:
- **By Default:** Environment variables defined in a recipe/build rule are **local** to that action. They are **not** automatically exported to child processes (sub-shells, tools, compilers).
- **Explicit Export:** To make a variable visible to children, the recipe must **explicitly export** it (e.g., `export VAR=value`).
- **Child Processes:** When a child process is spawned, it receives *only* the variables explicitly marked as exported in the parent's recipe.

#### Example (Makefile Compatibility)
In a standard Makefile:
```makefile
CC = gcc
all:
    $(CC) main.c -o main  # CC is inherited from the shell or make
```

In Shmoo (with `strict_env: true`):
```makefile
export CC = gcc  # Explicit export required
all:
    $(CC) main.c -o main
```

If `CC` is not exported, the compiler invocation fails because `gcc` is not found in the child's `PATH` (unless `gcc` is a declared tool dependency). This enforces strict hermeticity.

### Auditable Multivariate Builds
Because every variable's visibility is explicit, Shmoo can generate a precise **Environment Report** for every build step:
- Which variables were defined?
- Which variables were exported?
- Which variables did the compiler actually receive?
- Did any "leaked" variables (not explicitly exported) affect the build?

This makes the build **fully auditable** and **multivariate**: you can trace every decision back to a specific variable or file in the recipe.

---

## 4. Make Compatibility Layer (Drop-in Replacement)

Shmoo can drop in place of Make and other build systems by:
1.  **Parsing Makefiles:** Translating Makefile rules into Shmoo actions.
2.  **Environment Translation:** When translating a Makefile, Shmoo respects `export` statements. Variables without `export` are treated as local.
3.  **Shell Wrapper:** Shmoo's Makefile execution runs through a shell wrapper that enforces the `strict_env` policy.

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

---

## 5. The Delusion Shell (Interactive Development)

Shmoo can spawn an interactive shell that runs *inside* the build environment (the "Delusion Shell"):
- **Purpose:** Interactive debugging and development within a reproducible environment.
- **Execution:** `shbuild dev --action compile:app.c`
- **Environment:** The shell is pre-configured with the exact same `PATH`, `LD_PRELOAD`, mounts, and environment variables as the build step.
- **Capabilities:**
  - `cd` around the build tree (`/inputs`, `/outputs`).
  - Run tools (`gcc`, `make`, `gdb`).
  - Edit files (`vi`, `nano`) — edits are written back to the 9p-mounted input tree.
  - Debug the build step interactively.

This makes Shmoo a **development environment** as well as a build system, blurring the line between "build" and "dev" while maintaining strict hermeticity.

---

## 6. Configuration & Knobs

| Knob | Description | Default |
|------|-------------|---------|
| `strict_env` | If true, environment variables are not exported by default. | `true` |
| `parallel_jobs` | Maximum number of parallel build steps. | `NPROC` |
| `cache_enabled` | If true, cache build results by content hash. | `true` |
| `debug_shell` | If true, spawn an interactive shell on build failure. | `false` |
| `9p_remote` | URL of the remote 9p server (e.g., `tcp:9p.server:5640`). | (empty) |
| `sanitize` | If true, strip host-specific paths from compiler output. | `true` |

---

## 7. Implementation Plan

1.  **Parser:** Write a parser for Shmoo build files (`shmoo-build`) and Makefiles.
2.  **DAG Builder:** Implement the graph builder (nodes, edges, topological sort).
3.  **Environment Manager:** Implement the `strict_env` logic (local vs. exported variables).
4.  **Execution Engine:** Implement the scheduler, sandboxing, and result collection.
5.  **9p Integration:** Connect to the 9p client/server for filesystem access.
6.  **Syscall Hook:** Integrate the `LD_PRELOAD` wrapper for filesystem interception.
7.  **Delusion Shell:** Implement the interactive shell interface.
8.  **Caching:** Implement the content-addressed cache.
9.  **Testing:** Write tests for environment isolation, Make compatibility, and 9p integration.

---

## 8. Auditable Reporting

Shmoo generates a **Build Report** for every execution:
- **Graph:** The full DAG of actions.
- **Environment:** The exact variables passed to each step.
- **Inputs/Outputs:** File hashes and timestamps.
- **Tools:** Toolchain versions and hashes.
- **Purity:** A list of any "impurities" detected (e.g., undeclared variables, unexpected tool access).

This report is signed and versioned, providing a **cryptographically verifiable** audit trail of every build.
