# Design Questions — Open Issues for Shmoo

**Last updated:** 2026-08-20  
**Purpose:** Track unresolved design questions, open implementation approaches, and architectural decisions under review.  
These are NOT final decisions. They are areas where the approach must be determined, tested, and validated before code implementation begins.

---

## 1. LD_PRELOAD / DLL Injection for VFS Interception

### The Question

Can we interpose on `open()`, `access()`, `stat()`, `readdir()`, `mkdir()`, `unlink()`, `rename()`, `opendir()`, `closedir()`, `chdir()`, `getcwd()`, and related filesystem functions by building a shared library with `Inline::C`, loading it via `LD_PRELOAD`, and re-executing the Perl interpreter?

### Proposed Approach

```bash
# 1. Build the VFS interposition library with Inline::C
perl Makefile.PL    # Creates libshmoo_vfs.so

# 2. Preload it and re-execute Perl
export LD_PRELOAD=/path/to/libshmoo_vfs.so
export SHMOO_STASH='{"volumes":...}'  # Configuration stash
exec perl -I. /path/to/app.pl "$@"
```

### Benefits

- **Transparent to Perl:** Perl's built-in I/O functions (`open()`, `stat()`, `readdir()`, etc.) all go through libc, which calls our interposed functions. Perl sees no difference.
- **Transitive to child processes:** Any program spawned by Perl (compilers, linkers, shell commands, etc.) inherits `LD_PRELOAD` and the VFS layer. A compilation process that calls `open()` will automatically go through the VFS.
- **Transitive to other programs:** If we start a non-interactive shell with `LD_PRELOAD`, all tools it invokes (`gcc`, `make`, `as`, `ld`, etc.) inherit the VFS.
- **C-level implementation:** The interposition layer is pure C — no Perl overhead, no XS module boundary, no risk of Perl code bypassing the VFS.

### Risks and Challenges

1. **Perl internal file handling:** Perl may bypass libc I/O for internal operations (e.g., reading compiled bytecodes, cache files, or internal data structures via `sysopen()` with raw fd ops). These bypass the interposition layer.
   
2. **XS modules:** Many Perl modules (DBD::*, Crypt::*, etc.) use XS code that calls libc functions directly. They inherit the interposition, but some may use raw fd operations or direct syscalls (`syscall(2)`) that skip libc entirely.
   
3. **Signal handlers and async signals:** If `LD_PRELOAD` changes the behavior of functions called from signal handlers, the process may deadlock or corrupt state. Must be tested with `SIGCHLD`, `SIGTERM`, and other signal-heavy scenarios.
   
4. **Re-entrancy:** Interposed functions are called from any thread. Must be re-entrant and thread-safe. If the VFS uses locks or global state, contention will be a performance bottleneck.
   
5. **Recursive interposition:** If the VFS calls `stat()` internally (to check if a file exists), and `stat()` is also interposed, we must avoid infinite recursion. Solutions:
   - Save the original function pointer on first load and use it for internal calls
   - Use a flag to detect recursion (e.g., `in_vfs()` flag)
   - Use `dlsym(RTLD_NEXT, "stat")` to get the real libc function

6. **Perl module cache bypass:** Perl may bypass the VFS for loading `.pm` files if they are found in the compile-time `@INC` paths. Must verify that `do()`, `require()`, `use()` all go through interposed `open()`.
   
7. **Perl's `fcntl()` and `ioctl()` calls:** Some Perl I/O operations (like non-blocking I/O or file locking) use `fcntl()` or `ioctl()` at the fd level. If the VFS needs to track these (e.g., for advisory locks on mounted volumes), we must interpose them too.
   
8. **`execve()` and child processes:** When the Perl interpreter calls `execve()` to replace itself with another program, the VFS context must be carried through. Options:
   - Embed the configuration stash as a JSON blob in an environment variable
   - Write the stash to a temporary file and pass the path via an environment variable
   - Use a dedicated "shmoo exec wrapper" that injects the stash before `execve()`
   
9. **Performance overhead:** Every file operation goes through the VFS layer first. This adds lookup, translation, and possibly remote calls (for remote volumes). Must be benchmarked.
   
10. **Cross-platform portability:** `LD_PRELOAD` is Linux-specific. On macOS, use `DYLD_INSERT_LIBRARIES`. On Windows, use `LoadLibrary` or DLL injection. On BSD, use `exec()` with `execve()` wrapper or similar. The interposition approach is OS-dependent.

### Open Questions

- Is `LD_PRELOAD` the right approach, or should we provide a Perl wrapper library (`Shmoo::VFS`) that explicitly replaces Perl's I/O?
- How do we handle Perl's internal file operations that bypass libc?
- How do we propagate the VFS context to child processes that use `execve()`?
- What is the performance impact on a typical build process (which does thousands of small file reads and writes)?
- Should the interposition layer be opt-in (via `LD_PRELOAD`) or mandatory (built into a custom Perl binary)?

---

## 2. Mounting Archives via the VFS

### The Question

Should the VFS layer support mounting compressed archives (zip, tar, tar.gz, tar.bz2, tar.xz, cpio, etc.) as virtual filesystems?

### Proposed Behavior

```
Volume: DEPS
  Type: archive (zip/tar.gz)
  Source: /opt/deps/deps.tar.gz
  Mount options: { read_only: true, extract_to_scratch: false }

Volume: SRC
  Type: archive (tar.xz)
  Source: /opt/src/shmoo-source.tar.xz
  Mount options: { read_only: false, extract_to_scratch: true }
```

### Benefits

- **Read-only mount:** A zip or tar archive can be mounted read-only, making it appear as a normal directory structure. The application can read files from it without extracting to disk.
- **Writable scratch overlay:** When mounted with `extract_to_scratch: true`, the archive is extracted to a temporary directory, and writes go to a scratch overlay. Originals remain untouched.
- **Self-contained build dependencies:** A build process can depend on a pre-packaged archive (e.g., a library distribution) that "appears" as a directory.
- **Reproducible builds:** Archiving the entire source tree or dependency tree makes it easy to reconstruct the exact build environment.

### Open Questions

- Which archive formats to support? (zip, tar, tar.gz, tar.bz2, tar.xz, cpio, 7z, rar, etc.)
- Should the archive be decompressed in-memory or on-disk? (Memory is limited; on-disk requires a temp directory.)
- How do we handle archives that contain absolute paths? (Should they be stripped or relocated to the mount point?)
- Should we support writing back to the archive? (Modifying a tar archive is not well-supported; zip supports it but is less common.)
- How does the VFS layer handle symlinks inside archives? (Many archives preserve or strip symlinks differently.)
- Should archive mounting be a built-in feature, or should it be provided as a plugin/module?

---

## 3. The Shmoo Shell

### The Question

Shmoo needs a non-interactive Bourne-compatible shell that understands the VFS context natively. How should this shell be designed, and how does it integrate with the VFS and the Perl interpreter?

### Proposed Architecture

```
User → Shmoo Shell (Bourne-compatible, non-interactive)
        │
        ├── Variable expansion ($HOME, $PWD, $SHMOO_STASH) → VFS
        ├── Glob/wildcard expansion → VFS (case-aware)
        ├── cd / Working directory → VFS (case-aware)
        ├── File redirection (>, >>, <) → VFS (case-aware)
        ├── Pipes and subprocesses → VFS context carried forward
        │
        └── Spawned commands (gcc, make, etc.) → LD_PRELOAD inherited
```

### Key Behaviors

1. **Variable expansion:**
   - `$HOME` resolves through the VFS (resolves volume names, applies case rules)
   - `$PWD` is the VFS-aware current directory, not the POSIX one
   - Custom variables like `$SHMOO_VOLUMES` carry volume definitions

2. **Globbing:**
   - On POSIX: `*.txt` matches `file.txt` but not `file.TXT`
   - On Windows: `*.txt` matches `file.txt`, `file.TXT`, `file.Txt`
   - The shell must know the case sensitivity of the target filesystem

3. **Working directory:**
   - `cd /home/src` on Linux maps to the VFS path `/home/src`
   - If the shell is running in a Docker container, `cd /app` stays in the container
   - If the shell is running on the host and the file is in a mounted archive, `cd` goes through the VFS mount point

4. **Subprocess spawning:**
   - When the shell spawns a subprocess (e.g., `gcc file.c`), the subprocess inherits:
     - `LD_PRELOAD` (if set) → VFS layer
     - `SHMOO_STASH` → configuration stash
     - Volume definitions → the subprocess can use volume names
     - Case sensitivity rules → the subprocess respects the target FS rules

5. **Non-interactive mode:**
   - The shell reads commands from stdin or a script file
   - No readline, no history, no prompt
   - Errors go to stderr with clear, structured messages
   - Exit codes are propagated normally

### Open Questions

- Should the shell be a standalone C program, or a Perl script with a Bourne-compatible shell interface?
- How do we handle shell builtins (`echo`, `cd`, `test`, `[`, `export`, `unset`) in a VFS-aware way?
- Should the shell support job control (`&`, `wait`, `fg`, `bg`) in a non-interactive build context?
- How do we handle shell expansion (`$(command)`, `` `command` ``) when the command itself uses the VFS?
- Should the shell support variables that expand to volume names? (e.g., `cd $SRC:src`)
- How do we handle shell redirection (`>`, `>>`, `<`) when the target file might be on a different filesystem?

### Design Constraints

- Must be non-interactive (no readline, no terminal features)
- Must be Bourne-compatible (scripts should work in POSIX `/bin/sh`)
- Must understand VFS context natively (not just a wrapper around `/bin/sh`)
- Must propagate VFS context to all child processes
- Must be fast (it's on the critical path for every build operation)

---

## 4. Case Sensitivity Propagation Across the Stack

### The Question

How do we ensure that case sensitivity rules are preserved and correctly applied at every layer: the shell, the VFS, Perl, and child processes?

### Current Design

- The VFS layer tracks case sensitivity per filesystem
- The path parser tracks case sensitivity based on path type
- The configuration stash carries the FS type and case rules
- Child processes inherit the VFS context via `LD_PRELOAD`

### Open Questions

- If a file is created on a case-insensitive FS (Windows), should the VFS layer record the original case?
- If a file is accessed on a case-sensitive FS (Linux) but was created on a case-insensitive FS, should the VFS layer use the original case or the case of the POSIX path?
- How do we handle Unicode normalization (NFD/NFC) in case-insensitive comparisons?
- Should the VFS layer normalize filenames to a canonical form before writing them? (e.g., convert all filenames to lowercase for case-insensitive FSs)

### Key Design Decisions (from path-translation study guide)

1. **Every path component carries its case-sensitivity flag**
2. **VFS operations are always case-aware** based on the target filesystem's rules
3. **Cross-environment case translation is logged** in the configuration chain
4. **Case-preserving filesystems get special handling** — the original case is stored and preserved across translations
5. **Conflict detection is automatic** — the VFS layer detects when case-insensitive filesystems might have collisions
6. **Hash computation is context-aware** — hashes are case-sensitive or case-insensitive based on the FS
7. **Every translation between environments carries the FS type**

---

## 5. VFS Layer Implementation Language

### The Question

The VFS layer must be written in C. How do we integrate it with Perl?

### Options

| Option | Pros | Cons |
|--------|------|------|
| **A. LD_PRELOAD interposition** | Transparent to Perl, affects all child processes, no Perl changes needed | Risk of bypassed calls, re-entrancy issues, recursive interposition, platform-dependent (`LD_PRELOAD` only on Linux, `DYLD_INSERT_LIBRARIES` on macOS) |
| **B. Inline::C shared library + Perl wrapper** | Clean Perl integration, no `LD_PRELOAD` needed, easier to test | Child processes don't inherit the VFS layer unless they also load the library |
| **C. Custom Perl binary** | Full control, no bypass possible, can intercept every file op | Requires rebuilding Perl, hard to distribute, breaks compatibility |
| **D. Hybrid: LD_PRELOAD + Inline::C** | Combine best of both: `LD_PRELOAD` for system-wide interception, `Inline::C` for Perl-specific handling | Complex to coordinate, potential conflicts between the two approaches |

### Decision Under Review

- **Option A** (LD_PRELOAD) is proposed but carries significant risks (Perl bypasses, recursive interposition, child process propagation).
- **Option B** (Inline::C shared library) is safer but doesn't provide system-wide interception for child processes.
- **Option D** (Hybrid) may be the most complete solution but is also the most complex.

### Open Questions

- Can we reliably intercept ALL Perl file operations without bypasses?
- Should we provide BOTH `LD_PRELOAD` and a Perl wrapper library, letting the user choose?
- How do we handle XS modules that call libc functions directly?
- Should the VFS layer be a separate process that communicates via IPC (pipes, sockets, shared memory)?

---

## 6. Configuration Stash Serialization

### The Question

The configuration stash (`SHMOO_STASH`) carries volume definitions, mount maps, volume options, case rules, and more. How should it be serialized for transmission across environment boundaries?

### Current Design

- Stored as a JSON document in an environment variable
- Consumed and filtered at each environment boundary
- Redacted options removed before forwarding to subordinate environments

### Open Questions

- JSON is large. Should we use a binary format (Protocol Buffers, MessagePack) for performance?
- Environment variables have size limits (typically 128KB-2MB). Can the stash grow beyond that?
- Should the stash be written to a file instead of an environment variable? (Would avoid size limits but adds a file lookup step.)
- How do we handle sensitive data (passwords, API keys) in the stash? (Encrypt? Don't forward? Redact early?)

---

## 7. Build Graph Representation

### The Question

How should the build dependency graph be represented and tracked by the Black Box Monitor?

### Current Design (from ARCHITECTURE.md)

- The monitor tracks every file read/written by the build
- It builds a dependency graph (inputs → outputs)
- The graph is stored in a structured, highly-compressed file stream on disk
- An HTTP server provides interactive exploration of the build state

### Open Questions

- How do we handle files that are read but not tracked (e.g., system libraries, headers)?
- Should the graph be directed or undirected? (Dependencies are directed, but for some purposes undirected is easier.)
- How do we represent circular dependencies? (They shouldn't exist, but what if they do?)
- Should the graph be stored in-memory or on-disk? (In-memory for live queries, on-disk for persistence.)
- How do we handle incremental builds? (Should the graph support delta updates?)

---

## 8. Remote Execution Over SSH

### The Question

When executing commands on a remote host via SSH, how do we handle the VFS context?

### Current Design

- SSH is used for cross-host Linux-to-Linux transitions
- The shell on the remote host inherits `LD_PRELOAD` and the configuration stash
- The VFS layer handles volume resolution, case rules, and path translation

### Open Questions

- How do we install `LD_PRELOAD` on the remote host? (Must the shared library be present on the remote?)
- Should we use a SSH tunnel to carry the configuration stash, or embed it in an environment variable?
- How do we handle SSH key authentication in an automated build context?
- Should the remote execution use `ssh user@host "command"` or `ssh user@host < script.sh`?

---

## 9. QEMU Guest Agent Integration

### The Question

How do we reliably communicate with a QEMU guest using the guest agent (`qemu-ga`)?

### Current Design (from qemu-windows-vms study guide)

- Use `virDomainGetGuestInfo()` to query the guest state
- Use `virDomainSendGuestCommand()` to run commands
- Use `virDomainGetMemoryStats()` to check guest memory usage

### Open Questions

- What if the guest doesn't have `qemu-ga` installed? (Fallback to QMP or SSH?)
- How do we handle guest reboot (the guest agent must be re-connected)?
- Should the guest agent be pre-installed in the Windows VM image, or installed dynamically?
- How do we handle multiple QEMU guests (should the monitor support concurrent connections?)?

---

## 10. Path List Semantics

### The Question

How should Shmoo's path lists (like `$PATH`) interact with the VFS?

### Current Design

- Path lists are colon- or semicolon-separated lists of directory paths
- The path library handles splitting, classifying, and resolving each component
- Empty entries mean "current directory" in POSIX, ignored in DOS

### Open Questions

- Should path lists be VFS-aware? (e.g., `PATH=$SRC:bin:$BUILD:obj` where `$SRC` and `$BUILD` are volumes?)
- Should path lists carry volume definitions? (e.g., if `SRC` is a volume, does `SRC:bin` expand to a volume path?)
- How do we handle case sensitivity in path list expansion? (e.g., on a case-insensitive FS, does `PATH=/bin` also match `/BIN`?)

---

## 11. Build Prediction and Rebuild

### The Question

How do we implement the prediction/rebuild loop for builds?

### Current Design

- **First build:** Record the build by monitoring what the native build system (e.g., Make) actually does
- **Subsequent builds:** Use the learned model to predict which files need to be rebuilt
- The prediction should be more deterministic than the native build system

### Open Questions

- How do we store the learned model? (JSON? Binary? On-disk graph?)
- How do we handle changes to the native build system? (Should we re-record when the Makefile changes?)
- Should the prediction model be incremental (just track what changed) or full (rebuild the entire graph)?
- How do we handle builds that produce non-deterministic output? (e.g., timestamps embedded in binaries?)

---

## 12. Plugin Architecture

### The Question

How should the plugin system work?

### Current Design

- Plugins register for event hooks
- Events carry names and can be hooked by log analyzers
- Information flow through the system can be read, written, or altered by plugins
- The same infrastructure serves trivial tasks (wrap `ls`) and complex tasks (build an embedded Linux distribution)

### Open Questions

- Should plugins be written in Perl, C, or both?
- How do plugins communicate with the VFS layer? (Do they get direct access to volume definitions?)
- Should plugins be loadable at runtime or compile-time?
- How do we handle plugin security? (Can a malicious plugin corrupt the configuration stash?)
- Should plugins be scoped to a specific build step or global?

---

## 13. Obtaining Short-Name Paths in DOS/Windows Environments

### The Question

When executing programs in a DOS or Windows environment, we may need to obtain the 8.3 short-name path for files. This is critical for:
- Passing paths to older executables that cannot handle long filenames
- Interfacing with tools or scripts that expect 8.3 paths
- Cross-environment path translation where the target FS uses 8.3 names

### Methods for Obtaining Short Names

#### 1. Windows API: `GetShortPathName`

The primary programmatic approach is the Windows API function `GetShortPathName()`:

```c
DWORD GetShortPathName(
  LPCTSTR lpszLongPath,   // long filename
  LPTSTR  lpszShortPath,  // buffer for short name
  DWORD   cchBufferLength // size of buffer
);
```

Located in `kernel32.dll`. Called from:
- C/C++ programs directly linked against `kernel32.dll`
- Perl via `Inline::C` or `Win32::API` module
- QEMU guest agent commands (`qemu-ga` supports executing arbitrary commands)

The function returns the 8.3 short path for a given long path. Example:
```
"C:\Program Files\Shmoo\build.exe" → "C:\PROGRA~1\SHMOO~1\build.exe"
```

#### 2. Command Line: `dir /x`

In a Windows command prompt or DOS shell:
```cmd
dir /x C:\path\to\file.txt
```

This displays both the long filename and its 8.3 short name alias. Useful for manual inspection but also callable programmatically:
```cmd
cmd /c "dir /x C:\path\to\file.txt"
```

Parse the output to extract the short name.

#### 3. PowerShell: `Get-ShortPathName`

In modern Windows (PowerShell v3+):
```powershell
(New-Object System.IO.FileInfo("C:\Program Files\Shmoo\build.exe")).ShortName
# Returns: SHMOO~1.EXE

(New-Object System.IO.DirectoryInfo("C:\Program Files\Shmoo")).Parent.FullName
# Can also use: Get-Item -Force .\ShortName\Path\Here | Select-Object ShortName
```

#### 4. QEMU Guest Agent: `guest-exec`

When executing commands inside a QEMU guest via the guest agent:
```
qemu-ga command: guest-exec "cmd.exe /c \"echo %CD%\""
```

To get the short path of the current directory inside the guest:
```
guest-exec "cmd.exe /c \"for %i in (.) do @echo %%~fi\""
```

Or call `GetShortPathName` directly through a custom script executed via `guest-exec`.

#### 5. Wine: `winecmd` or `wineconsole`

When running under Wine:
```bash
wine cmd /c "dir /x C:\path\to\file"
```

Or use the `winepath` utility (if available):
```bash
winepath -s /mnt/c/Program\ Files/Shmoo
# Returns short path: Z:\PROGRA~1\SHMOO~1
```

### Integration with Shmoo's VFS Layer

When the VFS layer translates paths from POSIX to DOS:

```
1. Receive POSIX path: /home/corey/src/shmoo
2. Translate to DOS mount: Z:\shmoo
3. For each file/directory in the DOS mount:
   a. Query for 8.3 short name (via GetShortPathName API call)
   b. Store mapping: long_path → short_path
   c. Return short_path to caller when needed
4. Cache the mapping to avoid repeated API calls
```

The mapping should be stored in the configuration stash:
```
SHMOO_STASH: {
    short_path_cache: {
        "Z:\\shmoo": {
            "long": "Z:\\shmoo",
            "short": "Z:\\SHMOO"
        },
        "Z:\\Program Files\\Shmoo\\build.exe": {
            "long": "Z:\\Program Files\\Shmoo\\build.exe",
            "short": "Z:\\PROGRA~1\\SHMOO~1\\BUILD.EXE"
        }
    }
}
```

### Open Questions

- Should the VFS layer automatically convert all DOS paths to short names, or only when explicitly requested?
- How do we handle the case where 8.3 generation is disabled on the target system (NTFS can disable 8.3 generation)?
- What is the performance impact of querying `GetShortPathName` for every file operation?
- Should the short-path cache be stored persistently or per-session?
- How do we handle collisions when multiple long paths resolve to the same short path?

### Sources

- **Microsoft Docs** — `GetShortPathName` function documentation
  - Located in `kernel32.dll`, part of the Windows API
- **SuperUser** — "How can I find the short path of a Windows directory/file?"
  - [https://superuser.com/questions/348079/how-can-i-find-the-short-path-of-a-windows-directory-file](https://superuser.com/questions/348079/how-can-i-find-the-short-path-of-a-windows-directory-file)
  - Covers `dir /x`, PowerShell, and programmatic approaches
- **SS64.com** — Windows CMD filename syntax reference
  - [https://ss64.com/nt/syntax-filenames.html](https://ss64.com/nt/syntax-filenames.html)
  - Legal characters, 8.3 rules, long filename support
- **Wikipedia** — 8.3 filename article
  - [https://en.wikipedia.org/wiki/8.3_filename](https://en.wikipedia.org/wiki/8.3_filename)
  - Working with short filenames in a command prompt, VFAT case preservation rules

---

## 14. OS/2 Path Handling and Rexx Logical Disk Feature

### The Question

How does OS/2's handling of paths differ from Windows, and can OS/2's Rexx logical disk feature (AttachDir/Setlocal) inform Shmoo's VFS logical volume design?

### OS/2 vs Windows — Key Differences

**Path Separators and Command-Line Syntax:**
- **Windows (DOS heritage):** Paths use `\` (backslash), switches use `/` (forward slash)
- **OS/2:** Paths use `\` (or `/`), switches use `/` (like Unix)
- **Implication:** OS/2 command-line tools don't have the `/` vs `\` ambiguity that Windows has when paths are passed as arguments

**HPFS (High Performance File System):**
- Introduced in OS/2 1.2 (1989) by IBM
- Long filenames (like Windows 95 LFN, but earlier)
- Case-insensitive, case-preserving (like NTFS)
- Alternate data streams called "Extended Attributes"
- 64-byte filename limit (8.3 mode: 8+3+2)
- Different directory structure than FAT/NTFS

**Logical Drive Attachment (OS/2 vs Windows):**
- **Windows:** Drive letters always tied to physical/virtual partitions (`C:`, `D:`) or network shares (`net use`)
- **OS/2:** Drive letters can be attached to arbitrary directories via `AttachDir` — no physical partition required
- **Example:** `SETLOCAL D:=C:\some\arbitrary\directory` makes `D:` point to any directory

### OS/2 Rexx Setlocal and AttachDir

OS/2's Rexx scripting language provides a `Setlocal` command and `AttachDir` functionality that creates scoped logical drives:

```rexx
// In Rexx script
CALL Setlocal                    // Create local environment
"AttachDir D: = C:\some\dir"    // D: now points to C:\some\dir
// ... operations using D: ...
CALL Setlocal                    // Destroy local environment, D: mapping lost
```

**Key features:**
1. **No physical disk required** — drive letters can map to arbitrary directories
2. **Scoped environment** — `Setlocal` creates an isolated drive mapping context
3. **Case-insensitive, case-preserving** — HPFS/OS/2 semantics
4. **Removable** — `Setlocal` destroys the local environment and all mappings

### How This Informs Shmoo's VFS Design

OS/2's `AttachDir` is conceptually closer to Shmoo's logical volumes than Windows' drive letters:

| Feature | Windows | OS/2 | Shmoo (proposed) |
|---------|---------|------|------------------|
| Drive/Vol name | `C:` | `D:` | `SRC:` |
| Physical backing | Required (partition/device) | Not required | Not required |
| Arbitrary directory | No (only via mount) | Yes (`AttachDir`) | Yes (`Volume:DIR`) |
| Scoped/mutable | No (system-wide) | Yes (`Setlocal`) | Yes (per-session) |
| Grafting | No | No | Yes (multiple dirs under one volume) |
| Protection modes | No | No | Yes (scratch, direct, read-only) |

**Relevance to Shmoo's VFS:**
1. **OS/2's `AttachDir` is the closest historical precedent** for attaching directories as logical volumes without physical backing
2. **OS/2's `Setlocal` scoping** inspires how Shmoo's VFS context should be isolated between environments
3. **HPFS case-insensitive, case-preserving semantics** align with the VFS layer's case handling requirements
4. **OS/2 command-line `/` for switches** avoids the path/switch ambiguity that Windows has

### Open Questions

- Can OS/2's `AttachDir` concept be adapted for cross-environment VFS mounting?
- How does `Setlocal` scoping compare to Shmoo's configuration stash approach?
- Should Shmoo's volumes support scoped/unscoped variants (like `Setlocal` vs system-wide)?
- How do we handle case sensitivity when grafting directories from different filesystems?

### Sources

- **IBM OS/2 Documentation** — Setlocal and AttachDir references
  - `https://www-01.ibm.com/support/knowledgecenter/ssz89b_6.0/com.ibm.r42.os2.doc/rx_rexx.html`
  - Covers Setlocal, AttachDir, and Rexx environment scoping
- **OS/2 1.2 Release Notes** (1989)
  - Introduction of HPFS filesystem
  - Installable filesystems architecture
- **HPFS Specification** (IBM)
  - Extended Attributes (alternate data streams)
  - 64-byte filename limits
  - Case-insensitive, case-preserving semantics

---

## 15. Filesystem Namespace Isolation — Cross-Platform Comparison

### The Question

How do Linux, macOS, and Windows handle per-process or per-process-group filesystem isolation, and what does this mean for Shmoo's VFS architecture?

### Linux: Mount Namespaces

Linux has **mount namespaces** via `clone(CLONE_NEWNS)` or `unshare(CLONE_NEWNS)`:

```c
pid_t child = clone(child_func, stack_top, CLONE_NEWNS | SIGCHLD, NULL);
// Child and its descendants see a completely independent mount table
// Parent's mounts are invisible to the child and vice versa
// Can bind-mount, union-mount, COW layers, etc.
```

**Capabilities:**
- Per-process-group mount table — every mount operation is scoped to the namespace
- Bind mounts, union mounts (overlayfs, aufs), COW layers — all namespace-aware
- Built into the kernel — no per-call overhead
- Exportable via `/proc/PID/mountinfo` for audit/replay

**Requirements:**
- Root or `CAP_SYS_ADMIN` capability
- Linux-only (kernel feature)

### macOS: No Mount Namespaces

macOS has **no equivalent** to Linux mount namespaces. The filesystem namespace is global.

**Available capabilities:**
- **Mount operations:** `mount(2)` is global — any process with sufficient privileges can mount/unmount
- **SIP (System Integrity Protection):** Heavily restricts mount operations, even for root
- **NSFileProvider framework:** User-space file provider for iCloud Drive. On-demand file mounting, but per-app and user-level — not a per-process-group namespace
- **FUSE (MacFUSE, FUSEX):** Third-party FUSE implementations. System-wide — any process can see FUSE-mounted filesystems
- **Sandbox profiles (seatbelt):** Restrict file access, but don't create a new filesystem view. Blocked files return ENOENT/EPERM — access control, not namespace isolation
- **macOS Containers (macOS 13+):** Per-app sandboxed data directories with scoped file access. **Data isolation** only — **no filesystem view isolation**. No independent mount table, so you can't bind-mount or union-mount directories in a way that only the container sees

**Bottom line:** You can restrict access to files (sandbox profiles, containers) or provide virtual filesystems (FUSE, FileProvider), but you cannot create an isolated filesystem view for a process group the way Linux does with mount namespaces.

### Windows: No Mount Namespaces

Windows is similarly lacking. The file system namespace is fundamentally global and monolithic.

**Available capabilities:**
- **Junction points and symbolic links:** Path redirection at the NTFS level, but these are global and permanent. `mklink /D` and `MountVol` create them system-wide — every process sees them
- **Alternate Data Streams:** Metadata attached to files, not related to filesystem views
- **Impersonation tokens:** Change a process's security context, which affects *access* to files, not the *view*
- **Windows Filter Manager:** Kernel-mode or user-mode file system filter drivers (antivirus, backup, encryption). Global — a filter applies to all processes. You can check the current process, but you can't make it active only for one process group

**Filter drivers that come close (but fall short):**
- **File system minifilter drivers:** Hook I/O operations at the file system driver level. You can implement per-process logic by checking the current process, but the filter itself is registered globally.
- **WinFsp / Dokany:** FUSE-like implementations for Windows (user-mode file systems). Like macOS FUSE, these are system-wide. You can't give one process group a different view.
- **VirtualDisk API:** Attach/detach virtual disks, but these are system-wide block devices.

**What Windows lacks:**
- No per-process or per-process-group mount table
- No equivalent to `clone(CLONE_NEWNS)` or `unshare(CLONE_NEWNS)`
- No way to make a bind mount, union mount, or COW layer visible to only one process group

**Windows Containers caveat:** Windows 10 (1709+) and Windows Server 2016+ have namespaces for containers (network, PID, mount), but this is a container engine feature, not a system call you can use for arbitrary process groups.

### Comparison Table

| Capability | Linux | macOS | Windows |
|-----------|-------|-------|---------|
| Mount namespaces | `clone(CLONE_NEWNS)`, `unshare(CLONE_NEWNS)` | **No** — global namespace | **No** — global namespace |
| Per-process filesystem view | Yes | No | No |
| Per-process-group filesystem view | Yes (all threads in a namespace) | No | No |
| Bind mounts | Yes | No (only global) | No (junctions are global) |
| COW/Overlay layers | Yes (overlayfs, btrfs, LVM) | No | No |
| Union mounts | Yes (overlayfs, aufs, btrfs) | No | No |
| User-mode filesystems | FUSE (system-wide) | FUSE (system-wide) | WinFsp/Dokany (system-wide) |
| Access restriction per-process | Yes (namespaces + seccomp) | Sandbox profiles (seatbelt) | ACLs + impersonation |
| Container support | Docker, LXC, systemd-nspawn | Sandbox containers (data only) | Windows Containers (namespace support) |

### Implications for Shmoo

This is a significant architectural consideration:

1. **Linux:** Mount namespaces are the ideal mechanism for per-process-group filesystem isolation. Each process group gets its own filesystem view with bind mounts, COW layers, and union mounts.

2. **macOS/Windows:** Cannot provide true per-process filesystem isolation. The VFS layer must be implemented differently:
   - **LD_PRELOAD / DLL injection:** Intercept every file operation (as we've been discussing). Gives per-process filesystem control, but at the cost of intercepting every call rather than using namespace isolation.
   - **FUSE/WinFsp:** Provide a system-wide virtual filesystem, but all processes see the same view. You can't give one process group a different view.

3. **The LD_PRELOAD approach we discussed is actually the best option for non-Linux platforms** because it gives you per-process filesystem interception — the closest you can get to namespace isolation on those platforms.

### Open Questions

- Can OS/2's `AttachDir` concept be adapted for cross-environment VFS mounting?
- How does `Setlocal` scoping compare to Shmoo's configuration stash approach?
- Should Shmoo's volumes support scoped/unscoped variants (like `Setlocal` vs system-wide)?
- How do we handle case sensitivity when grafting directories from different filesystems?
- Can we abstract mount namespace semantics so the same build scripts work on all platforms?
- Should we build a fallback VFS layer for macOS/Windows that uses DLL injection/LD_PRELOAD when namespaces aren't available?

---

## 16. Audit Trails — Filesystem Transaction Logging

### The Question

How can we represent changes to the filesystem state so that an audit trace can see a picture of the filesystem as it existed at the time of the build? Can we build a running log of filesystem transactions into a shared blob, so the picture updates as each filesystem call is made?

### Cross-Platform Audit Approaches

**Linux: Mount Namespaces + LD_PRELOAD**
- The namespace itself is the audit trail — every mount operation is recorded in the namespace's mount table
- `mount(2)` calls are logged by the preload library or by `auditd`/`systemd`
- Export the mount table via `/proc/PID/mountinfo` to reconstruct the exact filesystem state at any point
- Every file operation goes through the namespace's mount table, giving implicit audit of graft/mount mappings

**Windows: File System Filter Drivers + ETW**
- **Minifilter drivers:** Intercept I/O operations at the file system driver level. Can log every file operation to a buffer, but the driver is registered globally
- **ETW (Event Tracing for Windows):** System-wide tracing framework. Enable `Microsoft-Windows-Kernel-File` provider to get a complete audit trail of every file operation, but it's system-wide, not per-process-group
- **DLL injection:** Intercept filesystem calls via DLL injection (Windows equivalent of `LD_PRELOAD`). Log each operation to shared memory for per-process audit trails

**OS/2: API Hooking + Shared Memory**
- Install an API hook on filesystem functions (`DosOpen`, `DosClose`, `DosRead`, `DosWrite`, `DosDelete`, `DosRename`)
- Log each intercepted call to a shared memory blob
- An audit consumer reads the blob and builds a picture of filesystem state changes
- OS/2 also has `ApiLog` and `ApiMon` utilities for tracing system calls (primarily for debugging)

### The Shared Blob Approach (Cross-Platform)

Regardless of platform, a **shared blob** approach works for cross-platform audit trails:

**Blob Structure:**
```c
typedef struct {
    uint64_t sequence;      // Global sequence number
    uint64_t timestamp;     // Nanosecond timestamp
    pid_t pid;              // Process ID
    uint32_t operation;     // OP_OPEN, OP_WRITE, OP_DELETE, etc.
    char path[MAX_PATH];    // File path
    uint64_t path_len;      // Length of path
    uint32_t result;        // Success/failure code
    uint32_t mount_id;      // Mount/volume ID for this operation
    uint64_t offset;        // File offset (for reads/writes)
    uint64_t bytes;         // Bytes read/written
    uint8_t case_sensitive; // Whether this path is case-sensitive
    uint8_t is_short_name;  // Whether this is an 8.3 short name
} vfs_log_entry_t;
```

**How It Works:**
1. **Initialize the blob** when the preload library loads — allocate shared memory via platform-specific means (`mmap` on Linux, `CreateFileMapping` on Windows, `DosAllocSHARED` on OS/2)
2. **Log every filesystem operation** — append-only log entry with sequence number, timestamp, process ID, operation, path, result, mount ID, offset, bytes
3. **Consume the blob** in an audit process — replay operations to reconstruct filesystem state at any point

**Blob Options:**
| Option | Pros | Cons |
|--------|------|------|
| Circular buffer | Fixed size, no overflow | Can lose old entries |
| Log with rotation | Keeps all entries, rotates on full | More complex management |
| Log to disk (mmap) | Survives crash, persists across restarts | Requires disk I/O, slower |
| Log to both memory and disk | Best of both worlds | Most complex |

**Audit Trail Representation:**
The audit trail should be a complete log of every filesystem operation, structured so it can be replayed to reconstruct the filesystem state at any point. Each entry should include:
- **Process ID** — which process made the call
- **Timestamp** — when the call was made
- **Operation** — open, read, write, close, delete, rename, mkdir, rmdir, symlink, link, etc.
- **Path** — the full file path (long and short if applicable)
- **Result** — success/failure code (errno, Windows ERROR_*)
- **Mount/volume ID** — which volume/mount point was involved
- **Case handling** — whether the path is case-sensitive, whether it's a short name
- **Offset/bytes** — for reads/writes: the file offset and number of bytes

### Platform Comparison

| Capability | Linux | Windows | OS/2 |
|-----------|-------|---------|------|
| Mount namespaces | `clone(CLONE_NEWNS)` — built-in | **No** — global namespace | **No** — global namespace |
| Per-process filesystem view | Yes | No (via DLL injection) | No (via API hooking) |
| Per-process-group view | Yes | No (via DLL injection) | No (via API hooking) |
| File system filter drivers | Yes (global, but can filter by namespace) | Yes (global, but can filter by process) | No |
| ETW/system tracing | Yes (`auditd`, `auditctl`) | Yes (ETW, `tracelog`) | Yes (`ApiLog`, `ApiMon`) |
| Shared memory logging | `mmap` / `shmget` | `CreateFileMapping` | `DosAllocSHARED` |
| DLL injection / API hooking | `LD_PRELOAD` | `LoadLibrary` / `Detours` | API hooking |
| Audit trail per-process | Yes (via namespace + preload) | Yes (via DLL + shared memory) | Yes (via hook + shared memory) |
| Audit trail per-process-group | Yes (via namespace) | No (only per-process) | No (only per-process) |

### Recommendations

**For Linux:**
- Use mount namespaces (`clone(CLONE_NEWNS)`) for per-process-group filesystem isolation
- Use `LD_PRELOAD` for VFS interception and audit trail logging
- Export the mount table via `/proc/PID/mountinfo` for audit
- Use `auditd` or `systemd-audit` for system-wide tracing

**For Windows:**
- Use DLL injection for VFS interception and audit trail logging
- Use `CreateFileMapping` for the shared blob
- Use ETW for system-wide tracing (if you need it)
- Consider file system filter drivers if you need system-wide tracing

**For OS/2:**
- Use API hooking for VFS interception and audit trail logging
- Use `DosAllocSHARED` for the shared blob
- Use `ApiLog` for system-wide tracing (if you need it)

**Cross-platform:**
- The shared blob approach works on all platforms
- The blob should be append-only to avoid lock contention
- Each process appends its own operations
- An audit consumer can replay the operations to reconstruct the filesystem state
- The log format should be platform-independent (JSON, CSV, or a custom binary format)

**Final recommendation:**
The **shared blob approach** is the best cross-platform solution for audit trails. It gives you per-process filesystem logging, works on all platforms, and can be extended to include the VFS layer's graft/mount mappings and case handling information. On Linux, you can also use mount namespaces for additional isolation, but the shared blob works everywhere.

### Open Questions

- What is the performance overhead of logging every filesystem call to the shared blob?
- Should we use ring buffers, append-only logs, or something else for the shared blob?
- How do we handle crash recovery — does the audit trail need to survive a process crash?
- Should we use a binary format for the blob (faster, smaller) or a text format (easier to debug)?
- Can we integrate the audit trail with the build prediction system to learn which files are read/written during builds?
- Should we provide a tool to replay the audit trail and reconstruct the exact filesystem state at any point?
- Can the audit trail be used to validate reproducibility across different builds?

---

## 17. URL Path Hints and Extensions

### The Question

URL path encoding can contain hints attached to each leg of the path, so a server can select or aggregate data to represent a subset of the content in its result set. Could this be an achievable feature for Shmoo's path handling mechanisms?

### Concept

In HTTP, URL paths can encode structured hints that servers use to optimize responses. For example:
- `GET /api/users/{id}/posts/{date:2024-01}` — hints at the aggregation level (group by month)
- `GET /data/region:us-west/dataset:weather/format:csv` — selects region, dataset, and format
- `GET /files/sparse::metadata` — requests only metadata, not full content

This is similar to **path parameters**, **query parameters**, or **media type negotiation**, but encoded directly in the path structure.

### Relevance to Shmoo's VFS

Shmoo's path handling could use a similar approach for:
- **Selective file access:** Request only metadata, headers, or specific sections of a file
- **Virtual filesystem aggregation:** A single path could resolve to data from multiple sources (e.g., `vfs:local::data:vfs:remote::data` means "aggregate data from local and remote sources")
- **Compression/encoding hints:** `file.dat?enc=compress` or `file.dat::lz4` to request compressed data on the fly
- **Mount hints:** `mount:tmpfs` to specify the target filesystem type for a mount operation

### Implementation Approach

**Path extension syntax:**
```
/path/to/resource::hint1:value1,hint2:value2
```

**Parsing logic:**
```c
// Example: parse "/data/files::compress,lz4::mode:fast"
path = "/data/files"
hints = {"compress": "lz4", "mode": "fast"}
```

**VFS integration:**
1. The path parser detects `::` as a hint separator
2. Hints are extracted and parsed into a key-value map
3. The VFS layer consults the hints to select/aggregate data
4. The original path is passed to the underlying filesystem

**Example use cases:**
- `vfs:/data::compress,zstd` — compress data on-the-fly before returning it
- `vfs:/data::aggregate,monthly` — aggregate data by month (like HTTP's date parameter)
- `vfs:/data::sparse,metadata` — return only metadata, not full content
- `vfs:/data::mount,tmpfs` — mount the path as a tmpfs overlay

### Pros
- Clean, extensible API — hints are part of the path, not separate parameters
- VFS layer can handle hints transparently — applications don't need to know about hints
- Hints are encoded in the path, so they're visible in logs, traces, and audit trails
- Similar to existing URL path encoding patterns, so developers will find it familiar

### Cons
- Non-standard — not part of RFC 3986 or POSIX
- Requires careful parsing — hints must be distinguished from path segments
- May conflict with file/directory names that contain `::`
- Hints must be stripped before passing to the underlying filesystem

### Open Questions
- Should Shmoo use a standard URL path encoding (like RFC 6920 or RFC 6570) or a custom syntax?
- How do we handle hints in non-HTTP contexts (local filesystem, VFS mounts)?
- Should hints be case-sensitive or case-insensitive?
- How do we handle hints in cross-environment path translation (do hints survive translation)?
- Can hints be used for cross-environment data selection (e.g., "compress this data before sending to the Windows VM")?
- Should we use a standardized hint syntax (like `key:value` pairs) or a more expressive format (like JSON)?

### Sources
- **RFC 6570** — URI Template (path parameter encoding)
- **RFC 6920** — Media Type Negotiation (content selection hints)
- **HTTP/2 Path Compression** — Path segments as hints for server optimization
- **URL Path Encoding** — Existing patterns for encoding structured data in paths

---

## 18. Filesystem Namespace Isolation — Cross-Platform Comparison

### The Question

How do Linux, macOS, and Windows handle per-process or per-process-group filesystem isolation, and what does this mean for Shmoo's VFS architecture?

### Linux: Mount Namespaces

Linux has **mount namespaces** via `clone(CLONE_NEWNS)` or `unshare(CLONE_NEWNS)`:

```c
pid_t child = clone(child_func, stack_top, CLONE_NEWNS | SIGCHLD, NULL);
// Child and its descendants see a completely independent mount table
// Parent's mounts are invisible to the child and vice versa
// Can bind-mount, union-mount, COW layers, etc.
```

**Capabilities:**
- Per-process-group mount table — every mount operation is scoped to the namespace
- Bind mounts, union mounts (overlayfs, aufs), COW layers — all namespace-aware
- Built into the kernel — no per-call overhead
- Exportable via `/proc/PID/mountinfo` for audit/replay

**Requirements:**
- Root or `CAP_SYS_ADMIN` capability
- Linux-only (kernel feature)

### macOS: No Mount Namespaces

macOS has **no equivalent** to Linux mount namespaces. The filesystem namespace is global.

**Available capabilities:**
- **Mount operations:** `mount(2)` is global — any process with sufficient privileges can mount/unmount
- **SIP (System Integrity Protection):** Heavily restricts mount operations, even for root
- **NSFileProvider framework:** User-space file provider for iCloud Drive. On-demand file mounting, but per-app and user-level — not a per-process-group namespace
- **FUSE (MacFUSE, FUSEX):** Third-party FUSE implementations. System-wide — any process can see FUSE-mounted filesystems
- **Sandbox profiles (seatbelt):** Restrict file access, but don't create a new filesystem view. Blocked files return ENOENT/EPERM — access control, not namespace isolation
- **macOS Containers (macOS 13+):** Per-app sandboxed data directories with scoped file access. **Data isolation** only — **no filesystem view isolation**. No independent mount table, so you can't bind-mount or union-mount directories in a way that only the container sees

**Bottom line:** You can restrict access to files (sandbox profiles, containers) or provide virtual filesystems (FUSE, FileProvider), but you cannot create an isolated filesystem view for a process group the way Linux does with mount namespaces.

### Windows: No Mount Namespaces

Windows is similarly lacking. The file system namespace is fundamentally global and monolithic.

**Available capabilities:**
- **Junction points and symbolic links:** Path redirection at the NTFS level, but these are global and permanent. `mklink /D` and `MountVol` create them system-wide — every process sees them
- **Alternate Data Streams:** Metadata attached to files, not related to filesystem views
- **Impersonation tokens:** Change a process's security context, which affects *access* to files, not the *view*
- **Windows Filter Manager:** Kernel-mode or user-mode file system filter drivers (antivirus, backup, encryption). Global — a filter applies to all processes. You can check the current process, but you can't make it active only for one process group

**Filter drivers that come close (but fall short):**
- **File system minifilter drivers:** Hook I/O operations at the file system driver level. You can implement per-process logic by checking the current process, but the filter itself is registered globally.
- **WinFsp / Dokany:** FUSE-like implementations for Windows (user-mode file systems). Like macOS FUSE, these are system-wide. You can't give one process group a different view.
- **VirtualDisk API:** Attach/detach virtual disks, but these are system-wide block devices.

**What Windows lacks:**
- No per-process or per-process-group mount table
- No equivalent to `clone(CLONE_NEWNS)` or `unshare(CLONE_NEWNS)`
- No way to make a bind mount, union mount, or COW layer visible to only one process group

**Windows Containers caveat:** Windows 10 (1709+) and Windows Server 2016+ have namespaces for containers (network, PID, mount), but this is a container engine feature, not a system call you can use for arbitrary process groups.

### Comparison Table

| Capability | Linux | macOS | Windows |
|-----------|-------|-------|---------|
| Mount namespaces | `clone(CLONE_NEWNS)`, `unshare(CLONE_NEWNS)` | **No** — global namespace | **No** — global namespace |
| Per-process filesystem view | Yes | No | No |
| Per-process-group filesystem view | Yes (all threads in a namespace) | No | No |
| Bind mounts | Yes | No (only global) | No (junctions are global) |
| COW/Overlay layers | Yes (overlayfs, btrfs, LVM) | No | No |
| Union mounts | Yes (overlayfs, aufs, btrfs) | No | No |
| User-mode filesystems | FUSE (system-wide) | FUSE (system-wide) | WinFsp/Dokany (system-wide) |
| Access restriction per-process | Yes (namespaces + seccomp) | Sandbox profiles (seatbelt) | ACLs + impersonation |
| Container support | Docker, LXC, systemd-nspawn | Sandbox containers (data only) | Windows Containers (namespace support) |

### Implications for Shmoo

This is a significant architectural consideration:

1. **Linux:** Mount namespaces are the ideal mechanism for per-process-group filesystem isolation. Each process group gets its own filesystem view with bind mounts, COW layers, and union mounts.

2. **macOS/Windows:** Cannot provide true per-process filesystem isolation. The VFS layer must be implemented differently:
   - **LD_PRELOAD / DLL injection:** Intercept every file operation (as we've been discussing). Gives per-process filesystem control, but at the cost of intercepting every call rather than using namespace isolation.
   - **FUSE/WinFsp:** Provide a system-wide virtual filesystem, but all processes see the same view. You can't give one process group a different view.

3. **The LD_PRELOAD approach we discussed is actually the best option for non-Linux platforms** because it gives you per-process filesystem interception — the closest you can get to namespace isolation on those platforms.

### Open Questions

- Can OS/2's `AttachDir` concept be adapted for cross-environment VFS mounting?
- How does `Setlocal` scoping compare to Shmoo's configuration stash approach?
- Should Shmoo's volumes support scoped/unscoped variants (like `Setlocal` vs system-wide)?
- How do we handle case sensitivity when grafting directories from different filesystems?
- Can we abstract mount namespace semantics so the same build scripts work on all platforms?
- Should we build a fallback VFS layer for macOS/Windows that uses DLL injection/LD_PRELOAD when namespaces aren't available?

---

## 19. Audit Trails — Filesystem Transaction Logging

### The Question

How can we represent changes to the filesystem state so that an audit trace can see a picture of the filesystem as it existed at the time of the build? Can we build a running log of filesystem transactions into a shared blob, so the picture updates as each filesystem call is made?

### Cross-Platform Audit Approaches

**Linux: Mount Namespaces + LD_PRELOAD**
- The namespace itself is the audit trail — every mount operation is recorded in the namespace's mount table
- `mount(2)` calls are logged by the preload library or by `auditd`/`systemd`
- Export the mount table via `/proc/PID/mountinfo` to reconstruct the exact filesystem state at any point
- Every file operation goes through the namespace's mount table, giving implicit audit of graft/mount mappings

**Windows: File System Filter Drivers + ETW**
- **Minifilter drivers:** Intercept I/O operations at the file system driver level. Can log every file operation to a buffer, but the driver is registered globally
- **ETW (Event Tracing for Windows):** System-wide tracing framework. Enable `Microsoft-Windows-Kernel-File` provider to get a complete audit trail of every file operation, but it's system-wide, not per-process-group
- **DLL injection:** Intercept filesystem calls via DLL injection (Windows equivalent of `LD_PRELOAD`). Log each operation to shared memory for per-process audit trails

**OS/2: API Hooking + Shared Memory**
- Install an API hook on filesystem functions (`DosOpen`, `DosClose`, `DosRead`, `DosWrite`, `DosDelete`, `DosRename`)
- Log each intercepted call to a shared memory blob
- An audit consumer reads the blob and builds a picture of filesystem state changes
- OS/2 also has `ApiLog` and `ApiMon` utilities for tracing system calls (primarily for debugging)

### The Shared Blob Approach (Cross-Platform)

Regardless of platform, a **shared blob** approach works for cross-platform audit trails:

**Blob Structure:**
```c
typedef struct {
    uint64_t sequence;      // Global sequence number
    uint64_t timestamp;     // Nanosecond timestamp
    pid_t pid;              // Process ID
    uint32_t operation;     // OP_OPEN, OP_WRITE, OP_DELETE, etc.
    char path[MAX_PATH];    // File path
    uint64_t path_len;      // Length of path
    uint32_t result;        // Success/failure code
    uint32_t mount_id;      // Mount/volume ID for this operation
    uint64_t offset;        // File offset (for reads/writes)
    uint64_t bytes;         // Bytes read/written
    uint8_t case_sensitive; // Whether this path is case-sensitive
    uint8_t is_short_name;  // Whether this is an 8.3 short name
} vfs_log_entry_t;
```

**How It Works:**
1. **Initialize the blob** when the preload library loads — allocate shared memory via platform-specific means (`mmap` on Linux, `CreateFileMapping` on Windows, `DosAllocSHARED` on OS/2)
2. **Log every filesystem operation** — append-only log entry with sequence number, timestamp, process ID, operation, path, result, mount ID, offset, bytes
3. **Consume the blob** in an audit process — replay operations to reconstruct filesystem state at any point

**Blob Options:**
| Option | Pros | Cons |
|--------|------|------|
| Circular buffer | Fixed size, no overflow | Can lose old entries |
| Log with rotation | Keeps all entries, rotates on full | More complex management |
| Log to disk (mmap) | Survives crash, persists across restarts | Requires disk I/O, slower |
| Log to both memory and disk | Best of both worlds | Most complex |

**Audit Trail Representation:**
The audit trail should be a complete log of every filesystem operation, structured so it can be replayed to reconstruct the filesystem state at any point. Each entry should include:
- **Process ID** — which process made the call
- **Timestamp** — when the call was made
- **Operation** — open, read, write, close, delete, rename, mkdir, rmdir, symlink, link, etc.
- **Path** — the full file path (long and short if applicable)
- **Result** — success/failure code (errno, Windows ERROR_*)
- **Mount/volume ID** — which volume/mount point was involved
- **Case handling** — whether the path is case-sensitive, whether it's a short name
- **Offset/bytes** — for reads/writes: the file offset and number of bytes

### Platform Comparison

| Capability | Linux | Windows | OS/2 |
|-----------|-------|---------|------|
| Mount namespaces | `clone(CLONE_NEWNS)` — built-in | **No** — global namespace | **No** — global namespace |
| Per-process filesystem view | Yes | No (via DLL injection) | No (via API hooking) |
| Per-process-group view | Yes | No (via DLL injection) | No (via API hooking) |
| File system filter drivers | Yes (global, but can filter by namespace) | Yes (global, but can filter by process) | No |
| ETW/system tracing | Yes (`auditd`, `auditctl`) | Yes (ETW, `tracelog`) | Yes (`ApiLog`, `ApiMon`) |
| Shared memory logging | `mmap` / `shmget` | `CreateFileMapping` | `DosAllocSHARED` |
| DLL injection / API hooking | `LD_PRELOAD` | `LoadLibrary` / `Detours` | API hooking |
| Audit trail per-process | Yes (via namespace + preload) | Yes (via DLL + shared memory) | Yes (via hook + shared memory) |
| Audit trail per-process-group | Yes (via namespace) | No (only per-process) | No (only per-process) |

### Recommendations

**For Linux:**
- Use mount namespaces (`clone(CLONE_NEWNS)`) for per-process-group filesystem isolation
- Use `LD_PRELOAD` for VFS interception and audit trail logging
- Export the mount table via `/proc/PID/mountinfo` for audit
- Use `auditd` or `systemd-audit` for system-wide tracing

**For Windows:**
- Use DLL injection for VFS interception and audit trail logging
- Use `CreateFileMapping` for the shared blob
- Use ETW for system-wide tracing (if you need it)
- Consider file system filter drivers if you need system-wide tracing

**For OS/2:**
- Use API hooking for VFS interception and audit trail logging
- Use `DosAllocSHARED` for the shared blob
- Use `ApiLog` for system-wide tracing (if you need it)

**Cross-platform:**
- The shared blob approach works on all platforms
- The blob should be append-only to avoid lock contention
- Each process appends its own operations
- An audit consumer can replay the operations to reconstruct the filesystem state
- The log format should be platform-independent (JSON, CSV, or a custom binary format)

**Final recommendation:**
The **shared blob approach** is the best cross-platform solution for audit trails. It gives you per-process filesystem logging, works on all platforms, and can be extended to include the VFS layer's graft/mount mappings and case handling information. On Linux, you can also use mount namespaces for additional isolation, but the shared blob works everywhere.

### Open Questions

- What is the performance overhead of logging every filesystem call to the shared blob?
- Should we use ring buffers, append-only logs, or something else for the shared blob?
- How do we handle crash recovery — does the audit trail need to survive a process crash?
- Should we use a binary format for the blob (faster, smaller) or a text format (easier to debug)?
- Can we integrate the audit trail with the build prediction system to learn which files are read/written during builds?
- Should we provide a tool to replay the audit trail and reconstruct the exact filesystem state at any point?
- Can the audit trail be used to validate reproducibility across different builds?

---

## 20. URL Path Hints and Extensions

### The Question

URL path encoding can contain hints attached to each leg of the path, so a server can select or aggregate data to represent a subset of the content in its result set. Could this be an achievable feature for Shmoo's path handling mechanisms?

### Concept

In HTTP, URL paths can encode structured hints that servers use to optimize responses. For example:
- `GET /api/users/{id}/posts/{date:2024-01}` — hints at the aggregation level (group by month)
- `GET /data/region:us-west/dataset:weather/format:csv` — selects region, dataset, and format
- `GET /files/sparse::metadata` — requests only metadata, not full content

This is similar to **path parameters**, **query parameters**, or **media type negotiation**, but encoded directly in the path structure.

### Relevance to Shmoo's VFS

Shmoo's path handling could use a similar approach for:
- **Selective file access:** Request only metadata, headers, or specific sections of a file
- **Virtual filesystem aggregation:** A single path could resolve to data from multiple sources (e.g., `vfs:local::data:vfs:remote::data` means "aggregate data from local and remote sources")
- **Compression/encoding hints:** `file.dat?enc=compress` or `file.dat::lz4` to request compressed data on the fly
- **Mount hints:** `mount:tmpfs` to specify the target filesystem type for a mount operation

### Implementation Approach

**Path extension syntax:**
```
/path/to/resource::hint1:value1,hint2:value2
```

**Parsing logic:**
```c
// Example: parse "/data/files::compress,lz4::mode:fast"
path = "/data/files"
hints = {"compress": "lz4", "mode": "fast"}
```

**VFS integration:**
1. The path parser detects `::` as a hint separator
2. Hints are extracted and parsed into a key-value map
3. The VFS layer consults the hints to select/aggregate data
4. The original path is passed to the underlying filesystem

**Example use cases:**
- `vfs:/data::compress,zstd` — compress data on-the-fly before returning it
- `vfs:/data::aggregate,monthly` — aggregate data by month (like HTTP's date parameter)
- `vfs:/data::sparse,metadata` — return only metadata, not full content
- `vfs:/data::mount,tmpfs` — mount the path as a tmpfs overlay

### Pros
- Clean, extensible API — hints are part of the path, not separate parameters
- VFS layer can handle hints transparently — applications don't need to know about hints
- Hints are encoded in the path, so they're visible in logs, traces, and audit trails
- Similar to existing URL path encoding patterns, so developers will find it familiar

### Cons
- Non-standard — not part of RFC 3986 or POSIX
- Requires careful parsing — hints must be distinguished from path segments
- May conflict with file/directory names that contain `::`
- Hints must be stripped before passing to the underlying filesystem

### Open Questions
- Should Shmoo use a standard URL path encoding (like RFC 6920 or RFC 6570) or a custom syntax?
- How do we handle hints in non-HTTP contexts (local filesystem, VFS mounts)?
- Should hints be case-sensitive or case-insensitive?
- How do we handle hints in cross-environment path translation (do hints survive translation)?
- Can hints be used for cross-environment data selection (e.g., "compress this data before sending to the Windows VM")?
- Should we use a standardized hint syntax (like `key:value` pairs) or a more expressive format (like JSON)?

### Sources
- **RFC 6570** — URI Template (path parameter encoding)
- **RFC 6920** — Media Type Negotiation (content selection hints)
- **HTTP/2 Path Compression** — Path segments as hints for server optimization
- **URL Path Encoding** — Existing patterns for encoding structured data in paths

---

## 21. The Shmoo Daemon/Interceptor Architecture

### The Question

The VFS layer is implemented via a **Daemon/Interceptor** model. How do we make the shared blob parallelizable, represent the "illusion" (current state of mounts/grafts), and audit state changes across a network?

### The Architecture

The "VFS Illusion" is managed by a centralized **Daemon** (the "Brain") and intercepted by client-side **Interceptor** libraries (the "Eyes/Hands").

#### A. The Shmoo Daemon (The Brain)
A centralized service that owns the **Canonical Mount Tree**.
- **Responsibilities:**
  - Maintain the global logical name table.
  - Validate mount requests (is `SRC:` already mounted? does the path exist?).
  - Record audit logs of every state change (mount, graft, scratch creation).
  - Distribute the "Illusion" to clients via IPC (Unix Sockets on Linux/macOS/OS/2, Named Pipes or TCP sockets on Windows).

#### B. The Interceptor (The Eyes/Hands)
A client-side library injected via `LD_PRELOAD` (Linux/macOS/OS/2) or DLL injection (Windows).
- **Responsibilities:**
  - **Intercept System Calls:** Hook `open()`, `stat()`, `rename()`, `mkdir()`.
  - **Path Translation:** When an application asks for `SRC:/path/file`, the Interceptor asks the Daemon: *"Where does `SRC:` point right now, and is there a local scratch overlay?"*
  - **Apply Overlay:** If the volume is "Scratch" (COW), the Interceptor creates a copy-on-write wrapper, redirecting the write to a local scratch directory while the Daemon knows it's part of a virtual layer.
  - **Reporting:** The Interceptor notifies the Daemon of changes (e.g., "I just created `SRC:/path/file`").

### VMS Logical Name Integration

The Daemon/Interceptor model maps perfectly onto the VMS Logical Name architecture:

| VMS Concept | Shmoo Equivalent | Description |
|-------------|------------------|-------------|
| **System Table** | **Daemon Global State** | The "World's Truth." Defines the global mounts, volume names (e.g., `SRC:`, `OBJ:`), and their physical backing. Networkable. |
| **Process Table** | **Client Overlay** | Maintained by the **Interceptor**. Contains local overrides. If a process defines `SRC:` to point to a different local scratch area, it "conceals" the global `SRC:` definition. |
| **Concealment** | **Shadowing** | The Process table shadows the System table for that specific process. `DEFINE/NOCONCEAL` restores the global view. |
| **Search List** | **Union Mount** | A logical name pointing to a list of directories (`DIR1;DIR2`). The Interceptor (or Daemon) resolves this list into a union view. |
| **Privileges** | **Daemon Security** | The Daemon enforces "Read-Only" logical definitions for the user, while the build system (the process) can only modify its own local namespace. |

### Auditing and the "Illusion"

The **Illusion** is the state of the filesystem *as seen by the user*, which might be different from the physical reality due to grafts, overlays, and mount maps.

1. **Centralized Logging:** The Daemon keeps a **Mutable Log** of every state change. Every time a volume is mounted, unmounted, or a scratch layer is created, the Daemon writes an entry: `TIMESTAMP ACTION USER PATH`.
2. **Networkability:** Because the Daemon is central, it captures *everything*, regardless of where the physical writes happen. A remote client can access a "Remote VFS Illusion" by connecting to the Daemon over the network.
3. **Audit Trail:** The Daemon logs every IPC call (mount, unmount, scratch creation). This provides a complete, verifiable history of the build environment's evolution.

### The IPC Protocol (Example)
Simple, text-based or Binary (MessagePack).

*Request (Resolve):*
```json
{ "action": "resolve_path", "path": "SRC:src/build.o", "pid": 1024 }
```

*Response:*
```json
{ 
  "resolved": "/tmp/shmoo/scratch/src/build.o", 
  "mode": "scratch_copy_on_write", 
  "version": 42 
}
```

*Request (Log Access):*
```json
{ "action": "log_access", "path": "SRC:src/build.o", "mode": "write", "pid": 1024, "version": 42 }
```

### Open Questions

- **IPC Performance:** How do we keep the latency between the Interceptor and Daemon low enough that it doesn't slow down the build? (In-memory caching of the versioned mount map?)
- **Offline Handling:** What happens if the Daemon is unreachable? Should the Interceptor cache the state?
- **Conflict Resolution:** What happens if two Interceptors try to mount different physical paths to the same logical name `SRC:`? (Centralized locking on the Daemon?)
- **Security:** How do we prevent a client from "spoofing" the Daemon or injecting malicious mount maps? (Mutual TLS? Authentication tokens?)
- **Parallelism:** Can the Daemon handle thousands of simultaneous Interceptors? (Single-threaded event loop vs. multithreaded?)
- **Client-Side Caching:** The Interceptor shouldn't ask the Daemon for *every* read. It should cache the mount map. How do we handle cache invalidation (versioning)?

---

## 22. Compiler Include Path Interception and Grafting

### The Question

By intercepting standard library headers (like those in `/usr/include`) and providing a "stack of directories" to the compiler (like GCC), can we make GCC (and the compiler's preprocessor) "see" the Shmoo volumes (e.g., `SRC:`, `OBJ:`, `SYS:`) as part of its standard include and library search path?

### The Approach: Daemon-Driven Include Stacks

The **Shmoo Daemon** maintains the global logical names (`SYS$`, `SRC:`, `OBJ:`) and their physical backing (e.g., `/mnt/src`, `/tmp/scratch`).

The **Interceptor** (injected into the `gcc` or `cc` process) intercepts file system operations (`open()`, `stat()`, `opendir()`, `readdir()`). It makes the VFS volumes visible to the compiler as virtual directories on a "stack."

**The Stack Layout:**
```
/shmoo/stack: (The Virtual Search Path)
├── /shmoo/stack/01_SRC    -> [VFS:SRC] (Source headers)
├── /shmoo/stack/02_OBJ    -> [VFS:OBJ] (Build object headers)
├── /shmoo/stack/03_SYS    -> [VFS:SYS] (System headers)
└── /shmoo/stack/04_HOST   -> /usr/include (System host headers)
```

When the compiler preprocessor asks for `math.h`:
1.  It checks `/shmoo/stack/01_SRC/math.h`.
2.  The Interceptor asks the Daemon: "Where is `SRC:/math.h`?"
3.  The Daemon replies: "It's at `/mnt/src/lib/math.h`."
4.  The Interceptor makes `math.h` appear to exist at `/shmoo/stack/01_SRC/math.h` by serving the file from `/mnt/src/lib/math.h`.
5.  If it's not found in `SRC`, the preprocessor checks `02_OBJ`, and so on.

### Parallelism and the "Illusion"

Because the **Daemon** manages the stack:
*   Every compiler process injected with the Interceptor sees the **exact same stack**.
*   If a build step changes the `OBJ:` volume (e.g., by mounting a new build artifact directory), all compiler processes see it immediately.
*   This ensures **parallel builds** use the same, consistent include order and source files, preventing "it worked on my machine" bugs.

### VMS/OS/2 Integration

VMS and OS/2 already do this natively:
*   **VMS:** Compilers automatically search `SYS$INCLUDE`, `SYS$LIBRARY`, and user-defined logical names.
*   **OS/2:** HPFS and Rexx allow defining custom logical names (e.g., `OBJ:`) that the compiler can see as directories.

Shmoo can replicate this behavior perfectly:
1.  The Daemon provides the "System" logical names (`SYS$:`, `SRC:`, `OBJ:`).
2.  The Interceptor exposes them as directories (e.g., `/shmoo/SYS$INCLUDE`, `/shmoo/SRC`, `/shmoo/OBJ`).
3.  We configure GCC (via a wrapper or `CC` environment variable) to look at these paths *in a specific order*.

### Open Questions

1.  **Interception Level:** Should we intercept `stat()`/`open()` for every file the compiler tries to read? (Heavy overhead)
2.  **Compiler Integration:** Should we provide a "Shmoo GCC" wrapper that injects `LD_PRELOAD` automatically?
3.  **Conflict Resolution:** If `math.h` exists in both `SRC:` and `OBJ:`, the "stack" order decides. How do we enforce this order consistently across all compiler calls?
4.  **Parallelism:** Can the Daemon handle thousands of compiler processes asking for the same headers simultaneously?
5.  **Cache Coherence:** If the Interceptor caches the include path stack, how do we ensure it's always up to date when the Daemon modifies the `SRC:` or `OBJ:` mounts?

---

## 23. Isolated Build Environments — The "Standard Root" Pattern

### The Question

Instead of sharing a common VFS illusion, can the Daemon give each build process its own **mount map** that makes the build *think* it's working from a standard root filesystem (like `/usr/include`, `/lib`, `/bin`), but actually only sees the specific volumes assigned to that build?

### The "Standard Root" Isolation Pattern

Each build process gets a **tailored mount map** that presents a completely standard-looking filesystem structure, but every path is a volume reference that the Daemon translates to the actual data the build is allowed to see:

**Build A's Mount Map:**
```
/          → [VFS:BUILD_ROOT_A] (scratch workspace)
   /include → [VFS:INC_A] (project A headers only)
   /lib     → [VFS:LIB_A] (project A libs only)
   /bin     → [VFS:TOOLCHAIN_A] (compiler binaries)
```

**Build B's Mount Map:**
```
/          → [VFS:BUILD_ROOT_B] (scratch workspace)
   /include → [VFS:INC_B] (completely different headers)
   /lib     → [VFS:LIB_B] (completely different libs)
   /bin     → [VFS:TOOLCHAIN_B] (different toolchain)
```

Each build process believes it's rooted at `/`, has `/include`, `/lib`, and `/bin`, and is compiling normally. The Daemon handles all the translation, and the Interceptor makes it transparent.

### How It Works

1. **Daemon assigns a mount map to each build.** The map is a set of volume definitions that the build is allowed to see. For example, `BUILD_ROOT_A` is a scratch directory, `INC_A` is a logical volume pointing to specific headers.

2. **The Interceptor intercepts every filesystem operation.** When Build A's compiler calls `open("/include/math.h")`, the Interceptor asks the Daemon: *"What is `/include` in this build's mount map?"*

3. **The Daemon translates the path.** It looks up Build A's mount map, finds that `/include` maps to `[VFS:INC_A]`, and returns the resolved path within that volume.

4. **The Interceptor applies the translation.** The compiler's `open("/include/math.h")` becomes a request to the Daemon for `INC_A:/math.h`, and the Interceptor translates that to the actual location within `INC_A`.

5. **The build sees no difference.** It thinks it's working from a standard filesystem. It never sees the volumes or the Daemon. The isolation is completely transparent.

### Why This Matters

1. **No kernel namespaces needed.** On macOS and Windows, where we can't do `clone(CLONE_NEWNS)`, the Daemon/Interceptor model achieves the same effect: each build sees only what it's meant to see, without any kernel-level isolation.

2. **Deterministic isolation.** The build environment is fully reproducible because the Daemon controls every volume mount. No accidental inclusion of `/usr/include` from the host system. Every file operation is within the build's assigned volumes.

3. **Dynamic mount maps.** The Daemon can modify the mount map at any time — adding a new volume, removing an old one, redirecting a path — and the build sees the change immediately (with version-based cache invalidation).

4. **Audit trail per build.** Every file operation in Build A is logged against Build A's context. If a build is suspected of tampering, you can replay the exact sequence of mount operations and file accesses for that specific build.

5. **Parallel builds, different environments.** You can have 100 builds running simultaneously, each with its own mount map, each seeing a completely different filesystem. No cross-build contamination.

6. **Dynamic chroot without privileges.** This is essentially a **dynamic chroot** — a jail that's built entirely through the VFS layer. No need for `chroot()`, `pivot_root()`, or `namespace(2)` calls. The Daemon is the "prison guard," the Interceptor is the "cell wall," and the mount map is the "prison rules."

### Implementation Considerations

**Mount Map Design:**
```yaml
build_map:
  id: build_a
  root: [VFS:BUILD_ROOT_A]
  mounts:
    - path: /include
      volume: [VFS:INC_A]
      read_only: true
      protection: direct
    - path: /lib
      volume: [VFS:LIB_A]
      read_only: false
      protection: scratch_copy_on_write
    - path: /bin
      volume: [VFS:TOOLCHAIN_A]
      read_only: true
      protection: direct
  working_directory: /
```

**Cache Invalidation:**
- The Interceptor caches the mount map for each build.
- The Daemon increments a version number whenever the mount map changes.
- The Interceptor checks the version on every filesystem operation (or at least periodically).
- On version mismatch, the Interceptor re-fetches the mount map from the Daemon.

**Path Translation:**
- Build A calls `open("/include/math.h")`.
- Interceptor looks up Build A's mount map: `/include` → `[VFS:INC_A]`.
- Interceptor asks Daemon: *"Where is `INC_A:/math.h`?"*
- Daemon resolves to actual path: `/mnt/volumes/inc_a/math.h`.
- Interceptor calls real `open()` with the resolved path.
- Build A never knows the truth.

### Comparison to Existing Approaches

| Approach | How it isolates | Privileges needed | Portability |
|----------|-----------------|-------------------|-------------|
| Linux mount namespaces | `clone(CLONE_NEWNS)` | `CAP_SYS_ADMIN` | Linux only |
| Docker containers | `namespace(2)` + cgroups | Root/Docker group | Linux only |
| macOS sandbox profiles | seatbelt/`sandbox_init()` | No special privileges | macOS only |
| Windows containers | Container namespaces | Admin/ContainerManager | Windows only |
| **Shmoo mount maps** | Daemon/Interceptor translation | None (user-level) | **All platforms** |

The Shmoo approach is the only one that:
- Works on all platforms (Linux, macOS, Windows, OS/2)
- Requires no special privileges (runs entirely in user space)
- Can be modified dynamically at runtime
- Provides per-build isolation without kernel intervention
- Integrates naturally with the VFS illusion model

### Open Questions

1. **Map granularity:** Should each build have its own mount map, or can builds share maps if they're identical? (Shared maps reduce Daemon overhead.)

2. **Map inheritance:** Can child processes inherit the parent's mount map? (If Build A spawns Build B, does B inherit A's map or get a new one?)

3. **Map versioning:** How fine-grained should versioning be? (Global version vs. per-volume version vs. per-path version?)

4. **Map serialization:** Can mount maps be saved/loaded? (This would enable build recording and replay.)

5. **Map security:** How do we prevent a build from "escaping" its mount map? (If Build A's compiler tries to `open("/etc/passwd")`, the Interceptor should return ENOENT or similar. What other attack vectors exist?)

6. **Performance impact:** Does translating every path through the Daemon add measurable overhead? (Cache invalidation, IPC latency, etc.)

7. **Nested maps:** Can a build mount another build's mount map? (e.g., Build A mounts Build B's map as a subdirectory for cross-compilation.)

8. **Dynamic map updates:** If the Daemon changes a volume's path (e.g., `/include` now points to a different volume), how do we notify all builds using that map? (Signal? Poll? Version check?)

### Sources

- **Linux `namespace(2)` man page** — Linux mount namespace implementation
- **Docker container architecture** — How Docker uses `clone(CLONE_NEWNS)` for container isolation
- **macOS Sandbox Profiles** — `sandbox_init()` and seatbelt for process sandboxing
- **Windows Container Architecture** — Container namespaces (network, PID, mount) introduced in Windows 10/Server 2016
