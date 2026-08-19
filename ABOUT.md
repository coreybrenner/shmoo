# Shmoo — About

**Status:** v0.8  
**Last updated:** 2026-08-19  
**License:** BSD 2-Clause

---

## What Is Shmoo?

Shmoo is a recursive, cross-platform build system designed to work in any environment — physical host, Docker, QEMU, WSL, Wine, Cygwin, native OS — and to provide audit-grade traceability for government and security use cases.

It replicates and unifies major build technologies (GNU Make, CMake, NMake, Redo, Xcode build files) behind a single, consistent interface. But it's more than a build tool. It's an **assistive build environment** for users with physical limitations — one that can transition execution across environments, run in the background, and return structured, traceable results.

Named after the Al Capp cartoon character. The name fits: Shmoo starts as a tiny thing, grows into whatever the situation demands, and gets things done without fanfare.

---

## Why It Exists

### The Problem With Make

GNU Make is the de facto standard. It's also:
- Opaque — when a build fails, you often don't know *why*
- Untraceable — there's no audit trail of what values came from where
- Rigid — one environment, one set of conventions, one way of doing things
- Silent — no visibility into what a build is actually doing unless you slog through verbose output

Shmoo replaces Make with something that gives you **control, visibility, and audit capabilities** that Make doesn't provide.

### The Target

Apple, as a replacement for GNU Make in their build toolchain. This is the strategic goal — not because Apple needs help (they don't), but because Apple's adoption would validate the architecture at scale and prove that Shmoo works where it matters most.

### The Human Context

The system was designed with users who have physical limitations in mind. A build system that can:
- Transition execution to a different environment (Windows from Linux, remote machines, VMs)
- Run in the background without requiring manual intervention
- Return structured, traceable results that an assistive user can act on
- Operate across boundaries (hypervisors, OSes, user sessions)

"Die making something" — the system that builds itself, and keeps building.

---

## How It Works

Shmoo is a **recursive layered system**. Each layer does one thing, resolves its environment, and spawns the next layer. The layers are:

1. **Launcher** — finds and starts the build
2. **Wrapper / Masquerade** — presents as make/nmake/cmake/etc.
3. **Stream Bridge** — passes I/O across boundaries
4. **Monitor** — logs every operation (the black box)
5. **Parser** — turns build files into CIR (Common Intermediate Representation)
6. **Graph** — DAG of dependencies with semantic hashing
7. **Plugin / Event Bus** — extensibility, control, observability
8. **Glass House** — web dashboard for full visibility

Every value in a build carries its **origin** — traceable back to the file, expansion, or command that produced it. This is the foundation of audit-grade traceability.

---

## Design Philosophy

- **Recursive, not monolithic** — each layer does one job. No giant binary that tries to do everything.
- **Transparent, not silent** — the Glass House gives full visibility. The Monitor captures everything.
- **Traceable, not opaque** — every value has an origin chain. You can answer "why does this have this value?"
- **Adaptive, not rigid** — works in any environment. Transitions between environments seamlessly.
- **Extensible, not closed** — plugins hook events, add control, inject data.

---

*This document is the vision. ARCHITECTURE.md is the brain. PROJECT_LOG.md is the conversation history.*
