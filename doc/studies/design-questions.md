# Design Questions — Open Issues for Shmoo

**Last updated:** 2026-08-19  
**Purpose:** Track unresolved design questions, open implementation approaches, and architectural decisions under review.  
These are NOT final decisions. They are areas where the approach must be determined, tested, and validated before code implementation begins.

---

## 1. Interposition via LD_PRELOAD

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

*End of design questions document.*

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
"AttachDir D: = C:\some\dir"  // D: now points to C:\some\dir
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

*End of design questions document.*
