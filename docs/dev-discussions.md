# Shmoo Dev Discussion Log

*Ongoing development discussions, design notes, and architectural decisions.*
*Last updated: 2025-01-23*

---

## 2025-01-23: Recipe Book Architecture & Build Hermeticity

### Core Vision
- **Goal:** Make Shmoo the first tool someone uses when running builds on or for a different platform.
- **Transition recipes:** CPAN modules that map environment transitions (source → target) to Perl code.
- **Auto-upgrade:** Builder detects missing recipes, fetches them from CPAN, and installs them on demand.
- **Open source foundation + sealed/certified commercial offerings.**

### Key Design Decisions
1. **Recipes are CPAN modules** — versioned, installable, composable, community-contributable.
2. **Recipe Book is the central registry** — maps transitions to recipe modules.
3. **Recipes have standard phases** — preflight, prepare, migrate, validate, cleanup.
4. **Dependency resolution is recursive** — complex transitions compose simpler recipes.
5. **Builder is stateful** — maintains state across phases, survives between runs.

### External Dependencies & Overrides
- Recipes depend on external tools (compilers, Perl, etc.).
- System defaults are often broken or leaky (e.g., ActiveState Perl's Windows registry access issues → led to finding Strawberry Perl).
- **Force Override pattern:** Recipes can bypass broken system defaults by installing known-good versions.
- **Certified Externals:** Shmoo provides pre-verified, purity-checked external toolchains (the commercial moat).

### Build Hermeticity & Determinism
- **Release mode:** Compiler drivers enforce consistent date/time strings, randomized seeds, locale, and timezone.
- **Binary inspection suite:** Dissects generated binaries, notes facts, compares with baselines.
- **Current landscape:** No build system bundles deep binary auditing with compiler driver enforcement.
  - Bazel/Buck2/Pants: sandbox hermeticity, no post-build dissection.
  - Nix: deterministic builds, no binary dissection CLI.
  - diffoscope: great for comparing binaries, but not a build-system hook.
  - SLSA/in-toto/sigstore: provenance attestation, not internal binary analysis.
- **Shmoo's gap:** Unified pipeline that (1) enforces deterministic compiler flags, (2) inspects binaries for purity, (3) compares against baselines, and (4) signs/certifies results.

### Business Model
- **Open source:** Anyone can use the recipe book, compiler wrappers, inspector, and comparator.
- **Sealed & certified:** Shmoo sells certified tarballs, SLA-backed toolchains, signed baseline fingerprints, priority recipe dev, and dedicated purity audits.
- **Certified Externals:** The commercial differentiator. We maintain the known-good, purity-verified external toolchains that override broken system defaults.

### Open Questions
- How granular should recipe granularity be? (Fine-grained recipes = more composition, but more overhead. Coarse-grained = less composition, simpler.)
- How to handle "black box" tools (e.g., commercial compilers) that can't be wrapped?
- What's the minimum viable purity report? (Which facts are essential for comparison?)
- How to handle "transition recipes" that require manual steps (e.g., physical hardware provisioning)?
- How to integrate with existing CI/CD systems (GitHub Actions, GitLab CI, Jenkins)?

---

## 2025-01-23: Plan9 vs. Busybox Architecture Decision

### The Decision
**Abandon plan9userland (plan9port) in favor of busybox as the userland layer.**

### Rationale
- **plan9port is non-standard:** Custom toolchain (`mk`, `9c`, `9l`), circular dependencies (`lib9` → `libbio` → `libplumb` → `lib9`), no shared library model.
- **busybox is standard:** Compiles with gcc/musl, `make menuconfig` for feature selection, `--enable-shared` for shared library support, LD_PRELOAD compatible.
- **busybox embedded:** `libbusybox.so` with named entry points (`bb_ls`, `bb_cat`, `bb_stat`, etc.) callable from C.
- **syscall interception:** LD_PRELOAD wrapper around `open()`, `read()`, `write()`, etc. intercepts busybox syscalls, routes to 9p client → 9p server → filesystem.

### 9P Bridge Architecture
```
busybox applets → LD_PRELOAD syscallhook → 9p client → 9p server → filesystem
```

### Build Order
1. `lib9pclient.a` (pure C, zero deps)
2. `lib9pserver.a` (depends on lib9pclient)
3. `libsyscallhook.so` (depends on lib9pclient, libdl)
4. `libbusybox.so` (compiled standalone, links against libsyscallhook via LD_PRELOAD)

---

## 2025-01-23: VM Orchestration Prototype

### `bin/vm-runner.sh`
- Detects VMs on libvirt, raw QEMU, Parallels, or cloud providers.
- Starts VMs if needed.
- Auto-detects IP addresses.
- Executes commands via SSH/WinRM/guest-agent.
- Prototype branch: `vm-orchestration` (pushed to origin).
- Long-term: Port to Perl with Inline::C, generalize into multi-host orchestration system.

---

## 2025-01-23: Interactive Delusion Shell & Deep Debugging (Maunderings)

### The "Time Travel" VFS Layer
- The VFS layer must support mounting compressed archives (tarballs, squashfs) as read-only roots.
- **Remounting at Failure:** When a build fails, the system captures the state of the environment (VFS mounts, logical volumes, environment variables, command line arguments).
- The engineer enters the `delshell` (Delusion Shell). Their command history is wiped and replaced *only* with the command that failed.
- **Rollback/Forward:** The engineer can "rewind" the VFS to the exact state of the external filesystems and logical volumes at the moment the failed command started.
- **Overlay Tracking:** An overlay filesystem layer keeps the "delusion" active, tracking all modifications (files edited, variables changed, environment mutations).

### Command Monitoring & Security
- **Aggressive Monitoring:** Commands issued in the delshell are logged and actively monitored for out-of-bounds access attempts (e.g., trying to escape the sandbox, read host secrets, or touch protected files).
- **Exit Analysis:** When the build engineer exits the shell, the altered command text is analyzed.
- **Security Filter & Rerun:** The session is run through a security filter. If approved, the scope is rerun with the modifications.
- **Resume:** The result is recorded, and the build restarts exactly where it left off (from the successful checkpoint).

### Deep Tool Integration (Perl/Inline::C)
- **Stepping Up:** Integrating the build toolchain as Perl module implementations allows a developer to step *up* directly from the failed execution environment into the scope of the Make rule that spawned it.
- **Variable Probing:** The engineer can probe variables at the time of failure, along the execution path.
- **Scope Rerun:** Change a variable or fix a rule, rerun that specific scope, and repeat until it succeeds. The debug scope is closed only when the step passes.
- **Automated Diffs:** A final diff is ready at the engineer's fingertips, relative to the start of the debug session or the top of the project tree.

### Interactive Variable Speculation
- **Live Environment Alteration:** A utility grafted into the delshell (actually a front for a Perl function) allows the engineer to alter the working environment live.
- **Real-Time Analysis:** The engineer can analyze a working variable state at multiple layers (compiler, linker, system), interactively speculating about the results of changing a variable's value by calling upon the machinery of the tool itself.
- **"What-If" Debugging:** "What if I change this flag? I'll alter the variable, rerun the compiler's variable scope, and see if it compiles cleanly."

### Remote Transmittal & Session Audit
- **Remote Access:** `shmoo-mitigated` access to the constructed delusion on a remote host.
- **Transmittal:** Vital information about the session (command history, file diffs, environment snapshots) is transmitted back across a different channel (e.g., a local terminal or a different network path).
- **Session Artifacts:** At the end of the session, a comprehensive summary is kept available: list of commands, files accessed, environment changes, diffs of edited files, and a state dump.
- **Scope Refunding:** When the engineer is happy with the fix, they can formally restart the build by archiving or deleting artifacts generated by anything in the current scope and "refunding" (cleaning and restarting) the whole scope.
- **Artifact Analysis:** A plugin system for before/after artifact analysis (e.g., comparing binary sizes, symbol tables, or entropy).

---

## Future Work

- [ ] Implement `Shmoo::Transitions::RecipeBook` (registry, upgrader, loader)
- [ ] Implement `Shmoo::Transitions::Recipe::Base` (base class with default phases)
- [ ] Implement `Shmoo::Upgrader` (auto-install missing recipes)
- [ ] Write sample recipe: `CygwinToWineStrawberry`
- [ ] Write tests for the recipe book
- [ ] Publish initial module to CPAN
- [ ] Implement compiler driver wrappers (`shbuild-cc`)
- [ ] Implement binary inspection suite (`shbuild-inspect`)
- [ ] Implement purity comparator (`shbuild-diff`)
- [ ] Implement provenance signing (`shbuild-sign`)
- [ ] Develop Certified External toolchains (Strawberry Perl, GCC, Clang, etc.)
