# Path Translation — A Technical Study Guide

**Study guide for:** Cross-environment path resolution, virtual filesystem mapping, inter-execution context path forwarding  
**Related Shmoo feature:** Layer 3 Stream Bridge (State Bridge), Layer 1 Launcher, configuration chain, volume logical system  
**Date:** 2026-08-19  
**Author's note:** This guide includes detailed exposition on VMS logical names, as requested.

---

## Table of Contents

1. [The Problem: Paths Don't Cross Environments](#1-the-problem-paths-dont-cross-environments)
2. [Path Classification Taxonomy](#2-path-classification-taxonomy)
3. [POSIX Paths](#3-posix-paths)
4. [DOS/Windows/OS2 Paths](#4-doswindowsos2-paths)
5. [URLs and URIs](#5-urls-and-uris)
6. [VMS Logical Names — Deep Dive](#6-vms-logical-names--deep-dive)
7. [Character Sets and Character Restrictions](#7-character-sets-and-character-restrictions)
8. [Path Normalization](#8-path-normalization)
9. [The Resolution Pipeline](#9-the-resolution-pipeline)
10. [Cross-Environment Translation](#10-cross-environment-translation)
11. [Path Lists — Colon-Separated Search Lists](#11-path-lists--colon-separated-search-lists)
12. [The Shmoo Virtual Filesystem Layer](#12-the-shmoo-virtual-filesystem-layer)
13. [Configuration Stash Integration](#13-configuration-stash-integration)
14. [Implementation Considerations](#14-implementation-considerations)
15. [Sources and Prior Art](#15-sources-and-prior-art)

---

## 1. The Problem: Paths Don't Cross Environments

A build starting in `/home/corey/src/shmoo` on a Linux host must be representable inside a Windows QEMU VM, and the final execution environment must be able to pick up execution in what would be the same directory the original invocation started from.

At each boundary transition, the path format changes. The translation must be **reversible in context** — if a build step in the guest produces a relative path like `obj/main.o`, that path must be traceable back to the original host path through the entire chain.

**The core insight:** Path translation is not syntax conversion. It is a **semantic operation** that carries context: which volume, which namespace, which filesystem hierarchy. A path like `C:\src` is meaningless without knowing what drive letter `C:` maps to in the current context.

Furthermore, a **path is not only a filesystem location**. It is a path to information, which may lead off the current machine — via URLs, SSH tunnels, FTP, or other protocols. The path library must handle all these forms uniformly.

---

## 2. Path Classification Taxonomy

Before parsing or translating, every path must be **classified**. A single string can be one of several path types, and the classification determines which parsing rules apply.

### Path Type Registry

| Path Type | Format | Example | Key Characteristics |
|-----------|--------|---------|---------------------|
| **POSIX absolute** | `/` prefix | `/home/user/src` | Single root `/`, forward slashes, case-sensitive |
| **POSIX relative** | No leading `/`, no drive letter | `./src/shmoo` or `src/shmoo` | Resolved against current directory |
| **DOS absolute** | `X:\` prefix | `C:\Users\src` | Drive letter + colon + backslash, case-insensitive |
| **DOS UNC** | `\\server\share` | `\\fileserver\builds` | Universal Naming Convention |
| **DOS device** | `\\.\PIPE\name` | `\\.\COM1` | Direct device access |
| **DOS verbatim** | `\\?\X:\path` | `\\?\C:\Windows` | No normalization, up to 32,767 chars |
| **DOS short** | 8.3 format | `C:\PROGRA~1\app.exe` | Legacy 8.3 filename format |
| **DOS root-relative** | `\path` | `\file.ext` | Root of current directory |
| **DOS drive-relative** | `X:path` | `D:file.ext` | Relative to drive X's current directory |
| **URL** | `scheme://authority/path` | `https://example.com/path` | RFC 3986 URI, scheme:authority/path?query#fragment |
| **file: URL** | `file:///path` | `file:///home/user/src` | Filesystem path encoded as URL |
| **Volume logical** | `NAME:/path` | `SRC:/src/shmoo` | Multi-character volume name, multi-char drive-equivalent |
| **Path list** | `DIR1:DIR2:DIR3` | `$PATH` | Colon-separated list of directory paths |
| **Relative** | No special prefix | `foo\bar` or `foo/bar` | Resolved against current directory of current drive/env |

### Single-Pass Classification Logic

The classifier must process the input in **a single pass** through the data. This is important because the input may come as a list of paths separated by a token character (e.g., `$PATH` on POSIX, or `PATH` on DOS).

```
algorithm classify_list(raw_input, separator):
    components = split(raw_input, separator)
    results = []
    for component in components:
        results.push(classify_and_parse(component))
    return results
```

The classification function itself operates on a **single string** (one component of the list), and the list-level function handles the splitting and reassembly.

### Classification Priority

Classification must follow a strict priority order to avoid ambiguity:

1. **URL** (scheme `://` prefix) — highest priority, unambiguous
2. **DOS verbatim** (`\\?\` or `\\\\?\\`) — explicit non-normalization flag
3. **DOS device** (`\\.\` or `\\\\.\\`) — device access
4. **DOS UNC** (`\\server\` or `\\\\server\\`) — network share
5. **DOS absolute** (`X:\` or `X:/`) — single letter, colon, separator
6. **DOS drive-relative** (`X:path` — letter, colon, NO separator)
7. **DOS root-relative** (`\` or `\\` prefix, but not UNC)
8. **Volume logical** (`NAME:/` — multi-char name, colon, separator)
9. **POSIX absolute** (`/` but not `//`)
10. **POSIX UNC** (`//` but not followed by `//`)
11. **POSIX relative / DOS relative** (everything else)

---

## 3. POSIX Paths

### Format

**Absolute paths:**
```
/prefix/path/to/file.ext
```

**Special cases:**
- `/` — root directory (the only path that IS the root)
- `//` — implementation-defined (POSIX says behavior is unspecified; some treat as hostname)
- `/home//corey` — double slash is normalized to single slash by POSIX path resolution

**Relative paths:**
```
./src/shmoo        # Explicit current directory
src/shmoo           # Implicit current directory
../src/shmoo        # Parent directory
../../src/shmoo     # Grandparent
foo/./bar/../baz    # Mixed
```

### POSIX Path Rules (POSIX.1-2017)

1. **Path components are separated by `/`.**
2. **`/` is the root.** There is exactly one root per filesystem hierarchy.
3. **`.` refers to the current directory.** Always implicit in resolution, but must be preserved in certain contexts.
4. **`..` refers to the parent directory.** Cannot go above `/`.
5. **Multiple consecutive slashes are normalized to one.** `/foo///bar` → `/foo/bar`.
6. **Trailing slashes have no effect** for most operations.
7. **Case-sensitive.** `/home/User` and `/home/user` are different paths.
8. **NUL character (`\0`) is never valid** in a path component.
9. **Maximum path length:** `{PATH_MAX}` bytes (including null terminator). POSIX minimum: 256 bytes (`_POSIX_PATH_MAX`).
10. **Maximum filename length:** `{NAME_MAX}` bytes. POSIX minimum: 14 bytes (`_POSIX_NAME_MAX`), but many systems support 255 bytes.

### Portable Filename Character Set (POSIX.1-2017 §4.12)

POSIX defines a **Portable Filename Character Set** — the subset of characters that are safe to use in filenames across all conforming implementations.

**Allowed characters:**
- `a-z` (lowercase letters)
- `A-Z` (uppercase letters)
- `0-9` (digits)
- `.` (period)
- `-` (hyphen-minus)
- `_` (underscore)

**Restricted characters:**
- `/` — **absolute disallowed** — it is the path separator and cannot appear in a filename component
- `:` — **restricted** — POSIX does not explicitly forbid `:` in filenames, but some systems use it for special purposes (e.g., NFS, NFSv4, and some network filesystems use `:` as a separator or stream indicator). POSIX itself says `:` is allowed in filenames but should be avoided for portability.
- `\0` (NUL) — **absolute disallowed** — it terminates C strings
- Characters with the high-bit set (`0x80`-`0xFF`) — implementation-defined, not portable

**Filenames with special meaning:**
- `.` — current directory (special, cannot be created as a regular file)
- `..` — parent directory (special, cannot be created as a regular file)

### POSIX Canonicalization (POSIX.1-2017 §4.13)

The POSIX canonicalization algorithm is well-defined:

```
algorithm posix_canonicalize(path):
    if path is empty:
        return "."
    
    result_parts = []
    parts = split(path, "/")
    
    for part in parts:
        if part == "" or part == ".":
            continue
        if part == "..":
            if result_parts and result_parts[-1] != "..":
                result_parts.pop()
            # At root, ".." has no effect
        else:
            result_parts.push(part)
    
    prefix = "/" if path starts with "/" else ""
    return prefix + join(result_parts, "/")
```

**Note:** This is **syntactic canonicalization**. It does not follow symlinks. To resolve symlinks (full canonicalization), a second pass with `realpath()` is required.

### POSIX Environment Variables Affecting Path Resolution

| Variable | Purpose |
|----------|---------|
| `$PATH` | Search path for executable lookup (colon-separated list) |
| `$HOME` | Home directory (used for `~` expansion) |
| `$PWD` | Current working directory |
| `$OLDPWD` | Previous working directory |
| `$TMPDIR` / `$TEMP` / `$TMP` | Temporary directory (varies) |
| `$XDG_CONFIG_HOME` | XDG base directory config path |
| `$XDG_DATA_HOME` | XDG base directory data path |
| `$IFS` | Internal field separator (default: space, tab, newline) |

**`~` expansion:** Shell syntax, not filesystem feature. Expanded by the shell or application. `~` alone → home directory of current user. `~/path` → home directory + `/path`. `~user/path` → home directory of specified user (requires password database lookup).

---

## 4. DOS/Windows/OS2 Paths

### Format

**Absolute paths:**
```
C:\Users\src\shmoo\
C:/Users/src/shmoo/   # Forward slashes also accepted
Z:\
```

**UNC paths:**
```
\\fileserver\share\builds\
\\.\C:\               # Direct device path
\\?\C:\Users\...      # Extended-length path (no normalization, 32,767 chars)
```

**Relative paths:**
```
..\src\shmoo            # Path relative
\src\shmoo               # Root relative
D:\src\shmoo             # Drive relative (note: no backslash after D:)
```

**Short (8.3) paths:**
```
C:\PROGRA~1\APP.EXE
```

### DOS Path Rules

1. **Drive letter:** Single letter `[A-Za-z]` followed by `:`. The colon is **mandatory** — `C\src` is NOT a drive path (Windows treats this as relative).
2. **Separator:** Backslash `\` is primary. Forward slash `/` is also accepted.
3. **Case-insensitive:** `C:\Users\src` and `c:\USERS\SRC` are the same path.
4. **Root-only:** `C:` alone refers to the current directory on drive C, NOT the root. `C:\` is the root.
5. **Trailing dots and spaces:** Legacy FAT behavior — trailing dots and spaces stripped from the final component.
6. **Reserved names:** `CON`, `PRN`, `AUX`, `NUL`, `COM1-COM9`, `LPT1-LPT9` (see Chris Denton, Special DOS Device Names).
7. **Maximum path length:** 260 characters (`MAX_PATH`). Up to 32,767 with `\\?\` prefix.
8. **Component limit:** 255 UTF-16 code units per component (NTFS).

**Historical note — slash vs backslash split:**

The reason Windows uses backslash while Unix uses forward slash has a concrete historical origin. MS-DOS 1 used `/` for both command-line switches AND paths, like Unix. When MS-DOS 2.0 introduced subdirectories, it needed a new path separator. According to Microsoft's own MS-DOS 2.0 source code README.txt (intended to guide OEMs on building custom DOS builds), **IBM requested that the path separator be changed from `/` to `\`** — Microsoft had originally planned to use `/`, but the change happened late in the development process. This is also why the MS-DOS kernel ended up supporting both characters: it was too late to change over fully.

Additionally, DOS 2.0 introduced `SWITCHAR` — a configurable switch character accessible via INT 21h functions 3700h/3701h and a `CONFIG.SYS` option. Setting `SWITCHAR=-` would make DOS syntax more Unix-like (using `-` instead of `/` for switches). All manufacturer-supplied commands would obey this character. However, `SWITCHAR` was removed from CONFIG.SYS in DOS 3.0, though the syscalls remain available to this day.

In practice, this means Windows kernels internally support both `/` and `\` as path separators — `/` is accepted by most Windows APIs for backward compatibility, though `\` is the canonical form for display.

### Disallowed Characters in Path Components (Chris Denton, Filesystems)

From Chris Denton's analysis of Windows filesystem drivers ([omnipath/Filesystems.html](https://chrisdenton.github.io/omnipath/Filesystems.html)):

| Disallowed | Description |
|------------|-------------|
| `\` `/` | Path separators |
| `:` | DOS drive and NTFS file stream separator |
| `*` `?` | Wildcards |
| `<` `>` `"` | DOS wildcards |
| `|` | Pipe |
| `NUL` to `US` (U+0000 to U+001F) | ASCII control codes (C0). Note: DEL (U+007F) is allowed. |

**Note:** Path separators and wildcards **must** be disallowed in normal filesystems, otherwise Win32 APIs will be unusable in some situations.

### Windows 11 DOS Device Name Resolution (Chris Denton, Special DOS Device Names)

Windows 11 simplified device name handling:

1. ASCII letters are uppercased
2. Trailing dots and spaces are removed
3. `NUL` in the filename of an absolute DOS drive or relative path will resolve to `\\.\NUL` if the parent directory exists

Before Windows 11:
1. ASCII letters uppercased
2. Everything after a `.` and the `.` itself removed
3. Trailing spaces stripped

### NT Kernel Paths (Chris Denton, NT Kernel Paths)

The Win32 API is a compatibility layer on top of NT kernel paths. NT kernel paths look similar to Unix paths but with key differences:

- Separator is `\`
- Components are UTF-16 code units
- **Any character except `\` is allowed** in component names — including NUL (U+0000)
- `.` and `..` have **no special meaning** at the kernel level
- Device paths like `\Device\HarddiskVolume2` are resolved by the Object Manager, then delegated to the device driver

Win32 paths are converted to NT kernel paths:

| Win32 Type | Example | Kernel Path |
|------------|---------|-------------|
| Drive | `C:\Windows` | `\??\C:\Windows` |
| UNC | `\\server\share\file` | `\??\UNC\server\share\file` |
| Device | `\\.\PIPE\name` | `\??\PIPE\name` |
| Verbatim | `\\?\C:\Windows` | `\??\C:\Windows` (passed through) |

The `\??` folder is a virtual folder that merges two directories:
- `\GLOBAL??` — symlinks common to all users
- Per-user `DosDevices` directory (e.g., `\Sessions\0\DosDevices\00000000-00053ce2`)

### DOS Device Names (Chris Denton, Special DOS Device Names)

These filenames may be interpreted as DOS devices:

```
AUX, CON, CONIN$, CONOUT$, NUL, PRN
COM1 through COM9 (including COM², COM³, COM¹)
LPT1 through LPT9 (including LPT², LPT³, LPT¹)
```

These are case-insensitive (canonically uppercase). On Windows 11, the resolution algorithm:
1. Uppercase ASCII letters
2. Remove trailing dots and spaces
3. Special `NUL` handling only in the filename (last component) of absolute DOS drive or relative paths

On Windows 10 and earlier, these additional rules applied:
1. Anything after a `.` and the `.` itself removed from the filename
2. Path must resolve against an existing parent directory

---

## 5. URLs and URIs

### Format (RFC 3986, Berners-Lee, Fielding, Masinter)

```
scheme:[//authority]path[?query][#fragment]
```

| Component | Example | Description |
|-----------|---------|-------------|
| `scheme` | `https`, `file`, `ssh`, `data` | Protocol identifier |
| `authority` | `user@host:port` | Optional — authentication and host |
| `path` | `/src/shmoo` | Resource path |
| `query` | `key=value&foo=bar` | Optional parameters |
| `fragment` | `section1` | Optional anchor |

### URL Character Sets (RFC 3986, §2)

**Unreserved characters** (no percent-encoding needed):
- `A-Z`, `a-z`, `0-9`
- `-` (hyphen)
- `.` (period)
- `_` (underscore)
- `~` (tilde)

**Reserved characters** (have special meaning, must be percent-encoded if used as data):
- `:` ` /` ` ?` ` #` — component delimiters
- `[` `]` — IPv6 address delimiters
- `@` — user information delimiter
- `!` `$` `&` `'` `(` `)` `*` `+` `,` `;` `=` — sub-delimiters

**Percent-encoded characters** (data that would otherwise be reserved):
- `%HH` where HH are two hex digits (e.g., `%20` for space, `%2F` for `/`)

### URL Path Rules

1. **Scheme:** Alphabetic start, followed by alphanumeric, `+`, `-`, `.`.
2. **Authority:** `//` delimiter. Can contain username, password, hostname, port.
3. **Path:** With authority, path is absolute. Without authority, can be relative.
4. **`.` and `..` in URL paths:** Resolved relative to URL path hierarchy. `https://example.com/a/b/../c` → `https://example.com/a/c`.
5. **Case:** Scheme is case-insensitive. Path and authority may be case-sensitive depending on scheme.

### file: URL Scheme (RFC 8089)

```
file:///home/user/src     → /home/user/src          (Linux)
file:///C:/Users/src      → C:\Users\src            (Windows)
```

Three slashes after `file:` because: `file:` (scheme) + `//` (authority — empty host) + `/` (root) + `C:` (drive letter in path). `file://C:/path` (two slashes) is **not valid** — the `C:` would be interpreted as authority (hostname).

### RFC 3986 References

- **RFC 3986** — Uniform Resource Identifier (URI): Generic Syntax. Berners-Lee, Fielding, Masinter. January 2005. Available at: [https://www.rfc-editor.org/rfc/rfc3986](https://www.rfc-editor.org/rfc/rfc3986)
- **RFC 8089** — Uniform Resource Identifier (URI) Schemes. The `file:` URI scheme specification. Available at: [https://www.rfc-editor.org/rfc/rfc8089](https://www.rfc-editor.org/rfc/rfc8089)
- **RFC 7320** — Updates RFC 3986. Available at: [https://www.rfc-editor.org/rfc/rfc7320](https://www.rfc-editor.org/rfc/rfc7320)
- **RFC 8820** — Updates RFC 3986. Available at: [https://www.rfc-editor.org/rfc/rfc8820](https://www.rfc-editor.org/rfc/rfc8820)

---

## 6. VMS Logical Names — Deep Dive

### What Are VMS Logical Names?

VMS logical names (also called "logical name tables") are a **name resolution system** that maps a symbolic name to a string value. They are defined by DEC (Digital Equipment Corporation) and are a core feature of OpenVMS (and its predecessors: RSX-11M, RT-11, VMS).

Unlike environment variables (which are expanded by the shell or application), **logical names are expanded by the file system itself**. This is a critical distinction.

### Basic Syntax

```
$ DEFINE LOGICAL_NAME "value"
$ DEFINE/USER SYS$LOGIN "DISK$:[USR.PRIVATE]"
$ DEFINE/TRAN SLNK "DISK$:[PRODUCT.SLNK]"
$ DEFINE/TRANS=CONCEAL HOME "DISK$:[USERS.username.]"
```

A logical name is defined with a name and a value. The value is typically a file specification (disk device, directory, filename), but can be any string. The name is case-insensitive and typically uppercase.

### How Logical Names Work

When a process references a logical name in a file specification, the file system **substitutes the value**:

```
$ EDIT HOME:README.TXT
```

The file system looks up `HOME`, finds `DISK$:[USERS.username.]`, and resolves the command to:

```
$ EDIT DISK$:[USERS.username.]README.TXT
```

The substitution happens **before** the file system parses the resulting string as a file specification.

### Logical Name Properties

**1. Nesting:**
Logical names may reference other logical names, up to a predefined nesting limit (typically 10 levels). The file system recursively resolves all references:

```
$ DEFINE A "DISK$:[X]"
$ DEFINE B "A:[Y]"        # B → A → DISK$:[X]:[Y]
$ DEFINE C "B:[Z]"         # C → B → A → DISK$:[X]:[Y]:[Z]
```

**2. Search lists:**
Logical names can reference multiple directories separated by semicolons:

```
$ DEFINE ABC_LIB "DISK$:[ABC.LIBS.PROD];DISK$:[ABC.LIBS.DEBUG];DISK$:[ABC.LIBS.TEST]"
```

When the system searches for `ABC_LIB:MODULE.OBJ`, it searches each directory in order and returns the first match. This is analogous to Unix `$PATH` but at the **file resolution level**, not the executable search level.

**3. Concealed logical names:**
Using `DEFINE/TRANSLATION=CONCEALED`, a logical name can be used in file specifications as a true volume/directory prefix:

```
$ DEFINE/TRANS=CONCEAL HOME "DISK$:[USERS.username.]"
```

With the trailing `.` in the directory specification, `HOME:[DIR]FILE` is valid — `HOME` acts as if it were a disk/device name.

**4. Access control:**
Logical names have levels (SYSTEM, PROCESS, USER, GLOBAL).
- **SYSTEM** — defined by system administrator, visible to all processes
- **PROCESS** — visible only to the defining process and its children
- **USER** — visible to the defining user's processes
- **GLOBAL** — visible to all processes but persistent

**5. Security:**
Logical names can be defined with various access controls and search lists. The file system checks the caller's access rights when resolving a logical name reference.

### Comparison to Other Systems

| Feature | VMS Logical Names | DOS Drive Letters | POSIX Mount Points |
|---------|-------------------|-------------------|--------------------|
| Name length | Multi-character | Single letter | N/A (not a naming concept) |
| Nesting | Up to 10 levels | No | No |
| Search lists | Yes (semicolon-separated) | No | No |
| Defined by | System (admin) or user | OS/BIOS | System (mount) |
| Expansion context | File system | Shell/command line | Kernel (VFS) |
| Persistence | Can be per-process | Per-boot | Permanent (fstab) |
| Case sensitivity | Case-insensitive | Case-insensitive | Case-sensitive |
| Scope | SYSTEM/PROCESS/USER/GLOBAL | Global | Global (kernel) |

### What Makes Logical Names Interesting

**1. Search lists are like `$PATH` at the filesystem layer.**
A POSIX `$PATH` environment variable tells the shell where to find executables. A VMS search list tells the file system where to find **any file**. This is more powerful — it means logical names can serve as aliases for directories that don't have a fixed location.

**2. Concealed logical names act like virtual volumes.**
When defined with `CONCEALED` and a trailing `.`, a logical name can be used as a volume/directory prefix in file specifications:

```
HOME:[DIR]FILE    # Valid — HOME acts as a volume name
```

This is conceptually similar to DOS drive letters but with multi-character names and nesting.

**3. Nesting enables layered abstractions.**
```
SYS     → DISK$:[SYSTEM]
SYS$SYS → SYS:[SYS]
SYS$SYS:FILES → DISK$:[SYSTEM]:[FILES]
```

Each level adds an abstraction layer. This is similar to symbolic links but at the name resolution level, not the filesystem level.

**4. Concealed logical names can be used as true disk names.**
The `CONCEALED` attribute allows a logical name to be used where a disk device name would normally go. This means applications can refer to directories by name without knowing their physical location.

### How VMS Logical Names Might Inform Shmoo's Design

VMS logical names suggest a model where:
- **Volume names are searchable** (like `$PATH` but for directories)
- **Nesting is supported** (logical names can reference logical names)
- **Search lists are a first-class concept** (multiple directories, first match wins)
- **Concealment enables virtual volumes** (a logical name can act as a mount point)
- **Access scope is per-process, per-user, or system-wide**

This is a more flexible model than DOS drive letters (single-char, no nesting, no search) and a different model than POSIX mount points (permanent, global, kernel-managed).

For Shmoo, this means volumes could:
- Be searchable: `SRC:` resolves to the first `SRC` directory in a search list
- Be nested: `SRC:BUILD:OBJ` could mean `SRC` contains `BUILD` which contains `OBJ`
- Be scoped: `SRC` in one build context might point to a different physical location than `SRC` in another context

### VMS Logical Names in Practice

**Standard input/output channels:**

| Logical Name | Meaning |
|-------------|---------|
| `SYS$INPUT` | Standard input (keyboard interactively, batch lines in batch) |
| `SYS$OUTPUT` | Standard output (terminal display or batch log) |
| `SYS$ERROR` | Standard error |
| `SYS$COMMAND` | Command channel |

These are analogous to Unix file descriptors 0, 1, 2, 3 but expressed as logical names rather than numbers.

### VMS File System: Files-11

The Files-11 file system (the native OpenVMS filesystem) integrates logical names directly:

- Files-11 supports volumes up to 2 TiB (as of Files-11 v6.0)
- Logical names are a layer **above** the filesystem, not part of it
- The filesystem itself handles file specifications of the form: `[directory]filename.ext`
- Logical names resolve to disk:directory specifications

**Directory structure:**
```
DISK$:[PRODUCER.PROJECTS.MYPROJECT.SOURCE]
DISK$:[PRODUCER.PROJECTS.MYPROJECT.BUILD]
DISK$:[PRODUCER.PROJECTS.MYPROJECT.OBJECTS]
```

The dot (`.`) separator is used instead of `/` or `\` for directory nesting.

**File specifications:**
```
DISK$:[USER]FILE.EXT;1
```
The `;1` suffix is a **version number**. Files-11 supports multiple versions of the same file, and the version number is part of the file specification. The default version (if omitted) is the latest.

### Sources on VMS/OpenVMS

- **Wikipedia: OpenVMS** — Comprehensive history and architecture overview. Available at: [https://en.wikipedia.org/wiki/OpenVMS](https://en.wikipedia.org/wiki/OpenVMS)
- **Wikipedia: Files-11** — The native OpenVMS filesystem, including logical names section. Available at: [https://en.wikipedia.org/wiki/Files-11](https://en.wikipedia.org/wiki/Files-11)
- **OpenVMS User's Guide** (DEC/HP/VSI) — Complete reference for logical name syntax and semantics.
- **VSI OpenVMS Programming Concepts Manual, Volume II** — VSI (VMS Software Inc.), April 2020. Available at: [https://vmssoftware.com](https://vmssoftware.com)
- **The OpenVMS FAQ** — HPE/OpenVMS community resource.

### Assessment for Shmoo

**What's relevant:**
- Multi-character volume names with search lists
- Nesting and concealment enabling virtual volumes
- Search-list semantics (first match wins)
- Process-level scoping (logical names can be per-process)

**What's not:**
- The `.ext;version` file specification format (specific to Files-11)
- The dot-separated directory nesting (`[DIR.SUBDIR]`) — Shmoo uses `/` or `\` separators
- The `DEFINE/CONCEALED` syntax — implementation-specific

The **search list concept** is the most transferable idea. A Shmoo volume like `SRC:` could resolve to the first `SRC` mount point found in a search list, enabling flexible path resolution without hard-coding physical locations.

---

## 7. Character Sets and Character Restrictions

### 7.1 POSIX Filename Character Sets

**Portable Filename Character Set** (POSIX.1-2017 §4.12):

| Character | Allowed | Reason |
|-----------|---------|--------|
| `a-z`, `A-Z`, `0-9` | Yes | Safe everywhere |
| `.` | Yes | Special meaning (`.` and `..`), but valid in filenames |
| `-` | Yes | Hyphen-minus; should not be first character (CLI argument confusion) |
| `_` | Yes | Safe everywhere |
| `:` | **Restricted** | Allowed by POSIX, but some implementations use it specially |
| `*` `?` | **Implementation-defined** | Wildcard characters; may be disallowed |
| `\0` (NUL) | **No** | Terminates C strings |
| `/` | **No** | Path separator — cannot appear in a filename component |
| High-bit (`0x80`-`0xFF`) | **Implementation-defined** | Not portable |

**Maximum sizes** (POSIX minimums):
- `{NAME_MAX}`: 14 bytes (many systems support 255)
- `{PATH_MAX}`: 256 bytes

### 7.2 DOS/Windows Filename Character Sets (Chris Denton, Filesystems)

**Disallowed in path components:**

| Character | Why Disallowed |
|-----------|----------------|
| `\` `/` | Path separators |
| `:` | DOS drive separator AND NTFS file stream separator (`file:stream`) |
| `*` `?` | Wildcards — must be disallowed so Win32 APIs are usable |
| `<` `>` `"` | DOS wildcards |
| `|` | Pipe character |
| U+0000 to U+001F (NUL through US) | ASCII control codes (C0) |
| U+007F (DEL) | **Allowed** by NTFS (unusual — most systems disallow it) |

**Allowed but restricted:**
- U+007F (DEL) — allowed by NTFS but rare
- Trailing dots and spaces — stripped by legacy compatibility rules (Windows FAT heritage)
- Components ending with `.` — final `.` removed unless another `.` follows

**Maximum sizes:**
- 255 UTF-16 code units per component (NTFS)
- 260 characters total (`MAX_PATH`) — or 32,767 with `\\?\` prefix
- 8.3 short names: 8 characters + 3 character extension

### 7.3 URL Character Sets (RFC 3986, §2)

**Unreserved characters** (no encoding needed):
```
ALPHA / DIGIT / "-" / "." / "_" / "~"
```

**Reserved characters** (have syntactic meaning):
```
":" "/" "?" "#" "[" "]" "@" "!" "$" "'" "(" ")" "*" "+" "," ";" "="
```

**Percent-encoding** (`%HH`):
- Used when a reserved character is needed as data
- Example: `%2F` for literal `/`, `%3A` for literal `:`

**Encoding considerations:**
- Paths may contain non-ASCII characters, which must be percent-encoded (UTF-8 octets)
- `file:` URLs use the local filesystem encoding (UTF-8 or locale-specific)
- Windows `file:` URLs are typically UTF-8 or UTF-16

### 7.4 NT Kernel Path Character Sets (Chris Denton, NT Kernel Paths)

The NT kernel is extremely permissive:
- **Any character except `\` (U+005C) is allowed** in component names
- Including NUL (U+0000) — though this makes the string unusable by the Windows API
- Components are UTF-16 code units (16-bit)
- No length limit enforced by the kernel (but Win32 APIs have limits)

This permissiveness means the kernel can create paths that Win32 APIs cannot handle — which is why the Win32 namespace applies transformations and restrictions.

### 7.5 Character Set Comparison Summary

| Character | POSIX | DOS/Windows | URL | NT Kernel |
|-----------|-------|-------------|-----|-----------|
| `a-z`, `A-Z`, `0-9` | Yes | Yes | Yes | Yes |
| `-` `.` `_` | Yes | Yes | Yes (unreserved) | Yes |
| `:` | Restricted | No (drive/stream) | Reserved | Yes |
| `/` | No (separator) | No (separator) | Reserved | Yes |
| `\` | Yes | No (separator) | Yes | No |
| `<` `>` `"` `|` | Yes | No | Reserved | Yes |
| `*` `?` | Implied | No (wildcards) | Reserved | Yes |
| `~` | Yes | Yes | Yes (unreserved) | Yes |
| U+0000 (NUL) | No | No | No | Yes (kernel), No (Win32) |
| U+007F (DEL) | Implied | Yes | Yes | Yes |
| U+0080-U+00FF | Implied | Yes (UTF-16) | Percent-encoded | Yes (UTF-16) |

### 7.6 Implications for Shmoo's Path Library

The path library must:
1. **Know which character set applies** based on the path type (POSIX, DOS, URL, etc.)
2. **Validate characters** at the appropriate level — syntactic classification happens first, then type-specific validation
3. **Encode/decode** when crossing boundaries — e.g., `:` in a POSIX path component is safe, but when that path is represented in a DOS-style notation, `:` might be ambiguous
4. **Preserve semantics** — `:` means drive separator in DOS but may be valid data in POSIX. The path library must not lose this information during translation.

---

## 8. Path Normalization

Normalization is the process of converting a path to a canonical form without resolving symlinks.

### Normalization Operations

| Operation | Description | Example |
|-----------|-------------|---------|
| Resolve `.` | Remove current directory markers | `foo/./bar` → `foo/bar` |
| Resolve `..` | Move up one level | `foo/bar/../baz` → `foo/baz` |
| Collapse multi-slash | `/foo///bar` → `/foo/bar` | — |
| Remove trailing slash | `/foo/bar/` → `/foo/bar` | — |
| Normalize separators | `C:\foo/bar` → `C:\foo\bar` (DOS) | — |
| DOS case normal | `C:\Foo\BAR` → `c:\foo\bar` | — |
| Strip reserved suffixes | `file.` → `file` (DOS FAT) | — |

### POSIX Normalization (POSIX.1-2017 §4.13)

```
function posix_normalize(path):
    if path is empty:
        return "."
    
    result_parts = []
    parts = split(path, "/")
    
    for part in parts:
        if part == "" or part == ".":
            continue
        if part == "..":
            if result_parts and result_parts[-1] != "..":
                result_parts.pop()
            # At root, ".." has no effect
        else:
            result_parts.push(part)
    
    prefix = "/" if path starts with "/" else ""
    return prefix + join(result_parts, "/")
```

### DOS Normalization (Win32 namespace, Chris Denton, Win32 Paths)

The Win32 namespace applies these transformations when converting Win32 paths to kernel paths:

1. `/` → `\` (separator normalization)
2. Remove components that are only `.`
3. Collapse sequences of more than one `\` to a single `\`
4. Resolve `..` — remove component along with parent (root is never removed)
5. Remove final `.` from component names (unless another `.` precedes it)
6. Strip trailing dots and spaces from the **filename only** (last component)

Example:
```
C:/path////../../../to/.////file.. ..
    ↓
C:\to\file
```

### URL Path Normalization (RFC 3986, §5.2.4)

The `remove_dot_segments` algorithm (RFC 3986) is well-defined:

```
function remove_dot_segments(path):
    input = path
    output = ""
    
    while input is not empty:
        A. If input starts with "../" or "./":
           Remove that prefix from input
        B. If input starts with "/./" or "/.":
           Replace with "/"
        C. If input starts with "/../" or "/..":
           Replace with "/" and remove last segment from output
        D. If input is ".":
           Remove it from input
        E. Otherwise:
           Move the first path segment (including initial "/" if any)
           to the end of output
           Remove it from input
    
    return output
```

---

## 9. The Resolution Pipeline

The full resolution pipeline for a path in a Shmoo execution chain:

```
1. CLASSIFY the path
   └─ Determine its type: POSIX, DOS, UNC, URL, Volume, Relative, Path List

2. NORMALIZE the path
   └─ Resolve ., .., multi-slash, separator normalization, trailing dots

3. RESOLVE to concrete path (if needed)
   └─ For POSIX: resolve symlinks, resolve to real filesystem path
   └─ For DOS: resolve junctions, reparse points, short-name resolution
   └─ For Volume: resolve logical name to concrete path using current environment

4. TRANSLATE for target environment
   └─ POSIX → DOS: /home/src → C:\home\src (requires mount mapping)
   └─ DOS → POSIX: C:\home\src → /mnt/win/c/home/src (requires mount)
   └─ POSIX → Volume: /home/src → SRC:/src (if SRC maps to /home/src)
   └─ Volume → POSIX: SRC:/src → /home/src (lookup volume mapping)

5. RECORD the translation in the configuration chain
   └─ Log: original path → type → normalization → resolution → translation → target path
   └─ The log carries the reverse mapping for traceability
```

### Translation Flow Example

```
Original: /home/corey/src/shmoo (POSIX absolute, Linux host)
  │
  │ 1. CLASSIFY: POSIX absolute
  │ 2. NORMALIZE: /home/corey/src/shmoo (already normalized)
  │ 3. RESOLVE: /home/corey/src/shmoo → /srv/shmoo (symlink)
  │ 4. TRANSLATE to QEMU Windows guest:
  │    ├── Mount mapping: /srv → Z:\
  │    └── Result: Z:\shmoo
  │ 5. RECORD: {orig: "/srv/shmoo", type: "posix", norm: "/srv/shmoo",
  │              resolved: "/srv/shmoo", translated_to: "Z:\\shmoo",
  │              mount_map: {"/srv": "Z:\\"}, reverse: {"Z:\\shmoo": "/srv/shmoo"}}
  │
  ▼ (path is now Z:\shmoo in the guest)
```

---

## 10. Cross-Environment Translation

### 10.1 Linux Host → QEMU Windows VM

**Mount-based approach (virtio-fs / shared folder):**
```
Host: /srv/shmoo (Linux filesystem path)
  │
  │ Mount: /srv/shmoo exported as shared folder → mapped to Z:\ in Windows guest
  │
Guest: Z:\shmoo (DOS path)
```

**QEMU configuration:**
```bash
qemu-system-x86_64 \
  -device virtio-fs,tag=shmoo \
  -device virtio-blk,drive=win_disk \
  -drive file=windows.qcow2,format=qcow2
```

**Translation rule:**
```
/srv/* → Z:\*
/Z:\shmoo → /srv/shmoo
```

### 10.2 Linux Host → Docker Container

**Bind mount approach:**
```
Host: /srv/shmoo
  │
  │ --volume /srv/shmoo:/build/shmoo
  │
Container: /build/shmoo
```

**Translation rule:**
```
/srv/* → /build/shmoo/*  (only if path starts with /srv/shmoo/)
/build/shmoo/* → /srv/*  (reverse)
```

### 10.3 Linux Host → SSH Remote

**Path translation over SSH:**
```
Host: /srv/shmoo
  │
  │ ssh user@remote "cd /srv/shmoo && make"
  │
Remote: /srv/shmoo (same path on remote system)
```

If the remote uses a different path structure, an SSH alias/profile maps local to remote:
```
/srv/shmoo → ssh://user@remote:/opt/build/shmoo
```

### 10.4 Linux → Wine → Windows

**Wine prefix approach:**
```
Host: /home/corey/src/shmoo (Linux)
  │
  │ Wine maps: /home/corey → Z:\
  │            /tmp → T:\
  │
Wine: Z:\src\shmoo (DOS path under Wine)
  │
  │ Windows Perl interpreter under Wine sees DOS paths
  │
Windows: Z:\src\shmoo (native DOS path)
```

### 10.5 Translation Table Structure

The translation table carries with each environment transition:

```
Entry: {
    from_env: "linux-host"
    to_env: "windows-virt"
    source_prefix: "/srv/"
    target_prefix: "Z:\\"
    source_mount: "/srv"
    target_mount: "Z:\\"
    note: "virtio-fs shared folder"
}
```

Each entry documents the source and target prefixes, the actual mount points on both systems, and any relevant context.

### 10.6 Reverse Translation

The translation map is bidirectional:

```
Forward: /srv/shmoo (linux-host) → Z:\shmoo (windows-virt)
Reverse: Z:\shmoo (windows-virt) → /srv/shmoo (linux-host)
```

The reverse mapping is automatically generated by reversing prefix mappings and swapping source/target.

---

## 11. Path Lists — Colon-Separated Search Lists

### POSIX $PATH and Similar Variables

POSIX defines `$PATH` as a colon-separated list of directories:

```
/bin:/usr/bin:/usr/local/bin:/home/corey/bin
```

**Rules:**
- Colon (`:`) is the separator
- Empty entries (e.g., `::`) mean "current directory"
- A trailing `:` means "include the path up to the colon, plus empty entry"
- The entries are searched **left to right**, first match wins
- Each entry is a directory path (not a file path)

### DOS/Windows PATH Variable

Windows uses semicolon-separated:

```
C:\Windows\System32;C:\Windows;C:\Program Files\Shmoo\bin
```

**Rules:**
- Semicolon (`;`) is the separator
- Empty entries are ignored
- Entries are searched left to right
- Each entry is a directory path

### Implications for Shmoo

Shmoo's path library must handle **both colon and semicolon as list separators**, depending on the path type:

| Path Type | Separator | Empty Entry Meaning |
|-----------|-----------|-------------------|
| POSIX path list | `:` | Current directory |
| DOS/Windows path list | `;` | Ignored |
| VMS search list | `;` | — |
| URL base path list | `:` | — |

**The path library's list handling:**

```
function parse_path_list(raw_input, separator = ':'):
    components = split(raw_input, separator)
    results = []
    for component in components:
        results.push(classify_and_parse(component))
    return results
```

When a Shmoo application receives a path like `$PATH`, the library should:
1. Identify it as a **colon-separated list**
2. Classify each component individually
3. Handle empty entries (current directory in POSIX)
4. Return the results as a structured list

This allows applications to use the same path API regardless of the underlying operating environment.

---

## 12. The Shmoo Virtual Filesystem Layer

### Concept

Shmoo will have its own idea about volumes. Each named volume is a **mount point**, and volumes are **impermeable to `..` path references** — meaning a `..` component cannot escape the volume boundary.

This is a key design feature: a volume acts as a **semantic boundary**. A `..` from inside the volume stays within the volume; it cannot go to the parent of the volume's root.

### Volume Rooting Options

A Shmoo volume may be rooted in any of these ways:

| Root Type | Example | Description |
|-----------|---------|-------------|
| **DOS/Windows drive letter** | `C:` | Physical device-backed volume |
| **Local directory** | `/srv/shmoo` | POSIX directory mount point |
| **FTP site directory** | `ftp://ftp.example.com/pub/` | Remote file access |
| **Webserver directory** | `http://server/builds/` | HTTP-based content serving |
| **Completely virtual** | `MEM://scratch` | In-memory, application-scope only |

### Volume Grafting

Different directories can be **grafted together** under a single Shmoo logical volume:

```
Volume: SRC
  ├── /home/corey/src → SRC:/home
  ├── /srv/builds/src → SRC:/builds
  └── ftp://ftp.example.com/source → SRC:/ftp
```

The volume `SRC` appears as a single entity to the application, but its content is distributed across multiple physical sources.

### Protection Modes

Grafted directories can be presented in different ways:

**Protected (write-through with scratch overlay):**
```
Read: SRC:/README.md → /home/corey/src/README.md (read from original)
Write: SRC:/README.md → /tmp/scratch/src/README.md (write to scratch)
```

Files can be read from the underlying directories but writes go to a scratch filesystem mounted **above** the presented file tree. This protects the original data while allowing the application to modify it.

**Direct-write:**
```
Read: SRC:/README.md → /home/corey/src/README.md
Write: SRC:/README.md → /home/corey/src/README.md (direct)
```

No protection — writes go directly to the underlying filesystem.

**Read-only:**
```
Read: SRC:/README.md → /home/corey/src/README.md
Write: Error (filesystem is read-only)
```

### Filtered File Operations

The Shmoo application environment has its own virtual filesystem layer through which all path and file operations are filtered:

```
application → Shmoo VFS → (filter: protection, volume resolution, translation) → OS/filesystem
```

This filter handles:
1. **Volume name resolution** — `SRC:` → actual mount point
2. **`..` boundary enforcement** — prevent escaping the volume
3. **Protection enforcement** — redirect writes to scratch
4. **Cross-environment translation** — POSIX ↔ DOS ↔ URL ↔ etc.
5. **Path normalization** — per the resolution pipeline (§8)
6. **Graft resolution** — find content across multiple underlying sources

---

## 13. Configuration Stash Integration

### Configuration Stash Structure

The configuration stash (`SHMOO_STASH`) carries all volume definitions, translation maps, and environment-specific configurations:

```
SHMOO_STASH = {
    volumes => {
        SRC  => {
            root      => "/home/corey/src",
            type      => "posix",
            protection => "protected",  # protected, direct, read-only
            grafts    => [
                { source => "/srv/builds/src",    path => "/builds" },
                { source => "ftp://ftp.example.com/source", path => "/ftp" },
            ],
        },
        BUILD => {
            root      => "/tmp/build",
            type      => "posix",
            protection => "direct",
        },
    },
    
    translations => [
        {
            from_env    => "linux-host",
            to_env      => "windows-virt",
            mount_map   => {
                "/srv"  => "Z:\\",
                "/tmp"  => "T:\\",
            },
            reverse_map => {
                "Z:\\"  => "/srv",
                "T:\\"  => "/tmp",
            },
            note => "virtio-fs shared folder mount",
        },
        {
            from_env    => "linux-host",
            to_env      => "docker-build",
            mount_map   => {
                "/srv" => "/build",
                "/tmp" => "/tmp",
            },
            reverse_map => {
                "/build" => "/srv",
                "/tmp"   => "/tmp",
            },
            note => "Docker bind mount",
        },
    ],
    
    original_execution => { ... },
}
```

### Progressive Disclosure (Redaction)

Each environment in the chain **consumes** the settings it needs and **removes** them from the stash before passing to the next environment:

```
Step 1 (Linux host):
  └─ SHMOO_STASH contains all volume definitions and translations
  
Step 2 (Redaction):
  └─ Linux host removes original_execution settings (no subordinate will be the original)
  └─ Linux host removes docker-build translations (if going to Windows, not Docker)
  └─ Linux host keeps volume definitions needed by Windows guest
  └─ Linux host logs the redaction:
       { action: "remove", key: "translations.docker-build", note: "not target env" }
       { action: "remove", key: "original_execution", note: "subordinate cannot be original" }

Step 3 (Forward):
  └─ Windows guest receives a filtered SHMOO_STASH with only relevant entries
  └─ Windows guest logs the alteration
```

This progressive disclosure has two benefits:
1. **Security**: Subordinate environments only see what they need
2. **Audit**: Every alteration is logged, creating a chain of custody for the configuration

---

## 14. Implementation Considerations

### Initial Implementation: Perl with Large Regex

The initial implementation will be in Perl, likely as a large regular expression that classifies and parses paths in a single pass:

```perl
# Simplified — the actual regex would be much more complex
my $path_regex = qr{
    ^(
        (?:([a-zA-Z][a-zA-Z0-9+.-]*)://)    # URL scheme
        |
        (?:\\[?](?i:PIPE|COM\d+|NUL|AUX|CON)(?:[\\/].*)?$)  # Device
        |
        (?:([A-Za-z]):([\\/](.*))?)           # DOS drive + path
        |
        (?:\\[\\/](.*))                        # UNC or device
        |
        (\/.*)                                 # POSIX absolute
        |
        (([A-Za-z_][A-Za-z0-9_]*):[\\/](.*))  # Volume logical
    )
}x;
```

The key design principle is: **single pass through the data**. The regex must classify, parse, and extract all components in one operation.

### High-Performance C Implementation

The C implementation (with Inline::C) would:

1. **Parse the string character by character** with a state machine
2. **Detect the path type** by examining leading characters (classification priority order)
3. **Extract components** (scheme, authority, path, drive, volume name, etc.)
4. **Validate characters** against the appropriate character set for the detected type
5. **Return a structured result** (type, components, validation errors)

### Performance Target

- Classification: O(n) — single pass, no backtracking
- Normalization: O(n) — linear component processing
- Resolution: O(n · m) — where n is path depth, m is number of mount points (for volume resolution)
- Translation: O(n) — linear prefix matching

### Key Design Decisions for Shmoo

1. **Volume names are multi-character** — like VMS logical names, not single-char DOS drive letters. They act as mount point identifiers.
2. **Volumes are impermeable to `..`** — `..` cannot escape a volume's root. This makes volumes semantic boundaries.
3. **Volumes can be grafted** — multiple physical sources under one logical volume name.
4. **Volumes can be protected** — writes redirected to a scratch filesystem.
5. **Volumes can be remote** — FTP, HTTP, or other protocol-backed volumes.
6. **The path library is environment-agnostic** — applications don't need to know the current OS; the library handles everything.
7. **Colon-separated lists** — handled natively by the path library, not by shell or application.
8. **Progressive disclosure** — configuration stashes are consumed and filtered as they pass through the execution chain.
9. **Every translation is logged** — the full chain of custody is recorded in the configuration stash.

---

## 15. Sources and Prior Art

### Primary Source — Chris Denton's omnipath

- **Chris Denton**, *An In-Depth Guide to Windows File Paths*
  - Overview: [https://chrisdenton.github.io/omnipath/Overview.html](https://chrisdenton.github.io/omnipath/Overview.html)
  - NT Kernel Paths: [https://chrisdenton.github.io/omnipath/NT.html](https://chrisdenton.github.io/omnipath/NT.html)
  - Strings: [https://chrisdenton.github.io/omnipath/Strings.html](https://chrisdenton.github.io/omnipath/Strings.html)
  - Filesystems: [https://chrisdenton.github.io/omnipath/Filesystems.html](https://chrisdenton.github.io/omnipath/Filesystems.html)
  - Win32 Paths: [https://chrisdenton.github.io/omnipath/Win32.html](https://chrisdenton.github.io/omnipath/Win32.html)
  - Special DOS Device Names: [https://chrisdenton.github.io/omnipath/Special%20Dos%20Device%20Names.html](https://chrisdenton.github.io/omnipath/Special%20Dos%20Device%20Names.html)
  - Print version (all chapters combined): [https://chrisdenton.github.io/omnipath/print.html](https://chrisdenton.github.io/omnipath/print.html)
- **Key contribution**: Comprehensive breakdown of NT kernel paths, Win32 namespace transformations, file system character restrictions, special device names, DOS device name handling (pre-Windows 11 vs Windows 11 differences), and string encoding (UTF-16, Multibyte).

### POSIX Specifications

- **POSIX.1-2017** (IEEE Std 1003.1-2017), The Open Group.
  - Chapter 4: General Concepts — [pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html)
  - Chapter 3: Base Definitions — Portable Filename Character Set — [pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html)
  - Section 4.12: Portable Filename Character Set
  - Section 4.13: Pathname Resolution
  - Section 3.170: Filename definition (bytes, NAME_MAX, NUL, slash)
  - Section 3.353: Pathname definition (PATH_MAX, portable filename character set)
  - Section 4.8: Filename Portability (portable filename character set compliance)
  - **Key contribution**: Canonical pathname resolution algorithm, portable filename character set specification, PATH_MAX and NAME_MAX limits, NUL and slash restrictions.

### URL/URI Specifications

- **RFC 3986** — Uniform Resource Identifier (URI): Generic Syntax. Berners-Lee, Fielding, Masinter. January 2005. [https://www.rfc-editor.org/rfc/rfc3986](https://www.rfc-editor.org/rfc/rfc3986)
  - Section 2: Characters (unreserved, reserved, percent-encoding)
  - Section 3: Syntax Components (scheme, authority, path, query, fragment)
  - Section 5: Reference Resolution (resolve_dot_segments algorithm)
  - **Key contribution**: Generic URI syntax, character classification, dot-segment resolution algorithm.
- **RFC 7320** — Updates RFC 3986. [https://www.rfc-editor.org/rfc/rfc7320](https://www.rfc-editor.org/rfc/rfc7320)
- **RFC 8820** — Updates RFC 3986. [https://www.rfc-editor.org/rfc/rfc8820](https://www.rfc-editor.org/rfc/rfc8820)
- **RFC 8089** — Uniform Resource Identifier (URI) Schemes. [https://www.rfc-editor.org/rfc/rfc8089](https://www.rfc-editor.org/rfc/rfc8089)
  - The `file:` URI scheme specification.
- **WHATWG URL Standard** — [https://url.spec.whatwg.org/](https://url.spec.whatwg.org/)
  - Living standard for URL parsing (used by browsers).

### VMS/OpenVMS Documentation

- **Wikipedia: OpenVMS** — [https://en.wikipedia.org/wiki/OpenVMS](https://en.wikipedia.org/wiki/OpenVMS)
  - Comprehensive history and architecture overview, including logical names section.
- **Wikipedia: Files-11** — [https://en.wikipedia.org/wiki/Files-11](https://en.wikipedia.org/wiki/Files-11)
  - Native OpenVMS filesystem, logical names, record-oriented I/O, directory structure.
  - Logical names section: definition, nesting, search lists, concealed logical names, access control levels.
  - Standard logical names: `SYS$INPUT`, `SYS$OUTPUT`, `SYS$ERROR`, `SYS$COMMAND`.
- **VSI OpenVMS Programming Concepts Manual, Volume II** (VSI, April 2020) — [https://vmssoftware.com](https://vmssoftware.com)
  - Complete reference for logical name syntax and semantics.
- **The OpenVMS FAQ** (HPE/OpenVMS community) — [http://vms.documentacion.tk/vmsdoc/](http://vms.documentacion.tk/vmsdoc/)
  - DEFINE command and logical name table management.

### Case Sensitivity and Filesystem Representation

This is a critical factor that must be tracked at every level: the path parser, the path resolver, and the VFS layer. Case sensitivity affects path matching, hash computation, conflict detection, and file I/O semantics.

**Why case sensitivity matters in Shmoo:**
1. **Path equality** — `src/Makefile` and `src/makefile` are different paths on POSIX but the same path on DOS/Windows
2. **Hash collision detection** — building a path hash for change detection must account for the FS's case rules
3. **VFS write operations** — `touch src/Makefile` on a case-insensitive FS creates one file; on POSIX, it creates another
4. **Cross-environment translation** — a case-preserved path on Windows may become ambiguous on POSIX
5. **Conflict resolution** — detecting when two paths refer to the same file on a case-insensitive FS

**Case sensitivity rules by filesystem:**

| Filesystem | Case | Case-Preserving | Example |
|------------|------|-----------------|---------|
| POSIX/Linux (ext4, xfs, btrfs) | **Case-sensitive** | Yes | `File.txt` ≠ `file.txt` (different files) |
| POSIX/macOS (HFS+) | **Case-insensitive** | Yes | `File.txt` = `file.txt` (same file, original case preserved) |
| POSIX/macOS (APFS) | **Case-insensitive** (default) / **Case-sensitive** (option) | Yes | Configurable at volume creation |
| DOS/Windows (FAT12/16/32) | **Case-insensitive** | No (DOS mode) / **Yes** (LFN) | `FILE.TXT` = `file.txt`; LFN stores original case but lookup is case-insensitive |
| DOS/Windows (NTFS) | **Case-insensitive** | Yes | `FILE.TXT` = `file.txt`; case preserved in metadata |
| OS/2 (HPFS) | **Case-insensitive** | Yes | Like NTFS, case-insensitive but case-preserving |
| Classic MacOS (HFS) | **Case-insensitive** | Yes | `: ` delimiter, case-insensitive comparison, case preserved |
| Classic MacOS (HFS+) | **Case-insensitive** | Yes | Unicode NFD normalization, case-insensitive comparison |
| Amiga (FFS/OFS, AmigaDOS) | **Case-sensitive** | Yes | `FILE.OBJ` ≠ `file.obj` (different files) |

**POSIX filesystems:**
- **ext4, xfs, btrfs**: Case-sensitive by default. Can be case-insensitive if compiled with `d_type` support and mounted with `d_type=casefold`, but this is not default.
- **HFS+**: Case-insensitive by default, case-preserving. The filesystem normalizes filenames to Unicode NFD form and compares them case-insensitively.
- **APFS**: Can be created as case-insensitive (default, macOS) or case-sensitive (`-c ascii`). Must be chosen at volume creation.
- **NFSv4**: Case-sensitive by default, but can be configured for case-insensitive semantics.

**DOS/Windows filesystems:**
- **FAT12/16/32**: Case-insensitive by definition. In 8.3 mode, case is lost (stored as uppercase). In LFN mode, case is preserved in metadata but comparison is case-insensitive.
- **NTFS**: Case-insensitive by default, case-preserving. The file name is stored in Unicode and compared case-insensitively using Windows locale rules (including handling of diacritics).
- **exFAT**: Case-insensitive like FAT, case-preserving like NTFS.

**OS/2 filesystems:**
- **HPFS** (High Performance File System): Case-insensitive, case-preserving, with 64-byte filename limit (8.3 mode: 8+3+2).
- **JFS** (Journaled File System, ported from OS/2): Case-insensitive, case-preserving.

**Classic MacOS (HFS/HFS+):**
- Uses `:` as path separator instead of `/`
- Case-insensitive, case-preserving
- HFS: Filename length limited to 31 characters (255 UTF-8 in HFS+)
- HFS+: Uses Unicode NFD normalization. `é` (U+00E9, composed form) and `é` (U+0065 + U+0301, decomposed form) are the same filename.
- In macOS, the FUSE-based filesystems (like FUSE-HFS) can preserve case sensitivity.

**Amiga filesystems:**
- **OFS/FFS**: Case-sensitive, case-preserving.
- **AmigaDOS**: The command-line layer is case-sensitive.
- **SFS** (Smart File System): Case-sensitive, case-preserving.

### Tracking Case Sensitivity in the Path Library

The path library must track case sensitivity at multiple levels:

**1. Path classification:**
When classifying a path, note the **expected case sensitivity** based on the path type:
- POSIX absolute (`/home/...`): Case-sensitive
- DOS absolute (`C:\...`): Case-insensitive
- Volume logical (`SRC:...`): Depends on the volume's underlying filesystem

```
function classify(path):
    result = classify_type(path)
    result.case_sensitive = depends_on_type(result.type)
    return result
```

**2. Path comparison:**
When comparing two paths, the comparison must be context-aware:
```
function paths_equal(a, b, case_sensitive):
    if case_sensitive:
        return normalize(a) == normalize(b)
    else:
        return normalize(a).lower() == normalize(b).lower()
```

**3. Hash computation:**
When building a path hash for change detection, hash the **canonical form** based on the FS rules:
```
function path_hash(path, case_sensitive):
    canon = normalize(path)
    if not case_sensitive:
        canon = canon.lower()  # or unicode-normalize + lowercase
    return hash(canon)
```

**4. VFS operations:**
When the VFS receives a path, it must apply the correct case rules based on the target filesystem:
```
vfs_open(path, flags):
    fs_type = get_filesystem_for_path(path)
    case_sensitive = fs_type.is_case_sensitive()
    
    # Find existing file with correct case sensitivity
    file_handle = vfs_find(path, case_sensitive)
    
    if flags.WRITE and file_handle and not case_sensitive:
        # Case-insensitive FS: update case to match original if different
        vfs_preserve_case(file_handle, path)
    
    return file_handle
```

### Cross-Environment Case Translation

When translating paths across environments with different case rules:

**POSIX → DOS:**
```
/home/src/Makefile → C:\src\Makefile
# POSIX case-sensitive: file is named Makefile
# DOS case-insensitive: stored as MAKEFILE (8.3) or Makefile (LFN)
# When reading back: MAKEFILE (DOS 8.3) or Makefile (LFN)
# Both refer to the same file on the DOS side
```

**DOS → POSIX:**
```
C:\src\MAKEFILE → /home/src/makefile
# DOS case-insensitive: MAKEFILE, Makefile, makefile are all the same file
# POSIX case-sensitive: must choose one form
# The VFS layer must record the original case preserved in DOS
# Result: /home/src/Makefile (preserving the case Windows stored it as)
```

**POSIX → Amiga:**
```
/home/src/Makefile → DF0:src/Makefile
# POSIX case-sensitive
# Amiga case-sensitive
# Direct mapping, no ambiguity
```

**Amiga → POSIX:**
```
DF0:src/MAKEFILE → /home/src/MAKEFILE
# Amiga case-sensitive: MAKEFILE is a different file from makefile
# POSIX case-sensitive: same
# Direct mapping, no ambiguity
```

### Case Preservation Across Transitions

For case-preserving filesystems (NTFS, HFS+, APFS, LFN FAT, Amiga FFS), the VFS layer must:
1. Record the **original case** when a file is created or first encountered
2. Use the **original case** when reading back or comparing paths
3. Preserve the case when translating to other environments that support case preservation

For case-loosing filesystems (FAT 8.3 mode), the VFS layer must:
1. Accept that case is lost (converted to uppercase)
2. Warn when case sensitivity is important and the target FS doesn't support it
3. Track the **loss** in the configuration chain for audit purposes

```
vfs_operation_log: {
    operation: "create_file",
    path: "/home/src/Makefile",
    target_fs: "FAT32",
    case_sensitive: false,
    case_preserving: false,
    stored_as: "MAKEFILE",  # Case lost
    warning: "Case preservation not supported by target filesystem (FAT32 8.3 mode)"
}
```

### Conflict Detection

When case-insensitive filesystems are involved, the VFS layer must detect and prevent conflicts:

```
function detect_case_conflict(paths, case_sensitive):
    seen = {}
    conflicts = []
    
    for path in paths:
        key = normalize_case(path, case_sensitive)
        
        if key in seen:
            if seen[key] != path:
                conflicts.append({
                    "canonical": key,
                    "conflicting_paths": [seen[key], path],
                    "fs_case_sensitive": case_sensitive,
                })
        else:
            seen[key] = path
    
    return conflicts
```

**Example:**
On a case-insensitive DOS filesystem:
```
src/Makefile   → key: src/makefile
src/makefile   → key: src/makefile  → CONFLICT
src/MAKEFILE   → key: src/makefile  → CONFLICT
```

The VFS layer must flag all three as referring to the same physical file, and prevent operations that assume they are distinct.

---

## 16. Cross-Platform Character Sets and Case Sensitivity

### Comprehensive Character Set Comparison

| Character | POSIX (ext4) | FAT32 (8.3) | NTFS | HFS+ | Amiga FFS | Notes |
|-----------|--------------|-------------|------|------|-----------|-------|
| `A-Z`, `a-z`, `0-9` | ✅ | ✅ | ✅ | ✅ | ✅ | Allowed everywhere |
| `_` (underscore) | ✅ | ✅ | ✅ | ✅ | ✅ | Safe |
| `-` (hyphen) | ✅ | ✅ | ✅ | ✅ | ✅ | Allowed, but POSIX warns against leading `-` |
| `.` (period) | ✅ | ✅ | ✅ | ✅ | ✅ | Special (`.` and `..`), but allowed in filenames |
| `:` (colon) | ⚠️ Restricted | ❌ No | ❌ No | ⚠️ Converted to `/` | ⚠️ Volume separator | POSIX: allowed but restricted. DOS: drive/stream separator. MacOS: converted to `/`. Amiga: device/volume separator. |
| `/` (slash) | ❌ No (separator) | ❌ No | ❌ No | ✅ (on HFS+, but converted to `:`) | ✅ | POSIX/DOS: path separator. HFS+: converted to `:`. Amiga: not separator (but `/` may have special meaning in some tools). |
| `\` (backslash) | ✅ | ✅ | ⚠️ Separator | ✅ | ✅ | DOS: primary separator. POSIX/Amiga: allowed. MacOS: ambiguous (backslash vs backspace). |
| ` ` (space) | ✅ | ✅ (leading OK) | ✅ | ✅ | ✅ | DOS: trailing spaces stripped in 8.3 mode. Leading spaces OK everywhere. |
| `*` `?` | ⚠️ FS-dependent | ❌ No | ❌ No | ⚠️ FS-dependent | ⚠️ FS-dependent | Wildcards must be disallowed for API usability. |
| `<` `>` `"` `|` | ✅ (ext4) / FS-dependent | ❌ No | ❌ No | ✅ | ✅ | DOS: reserved. POSIX: allowed by POSIX but some FSs (ext4) allow. |
| `+` `,` `;` `=` `[` `]` | ✅ | ❌ No | ✅ | ✅ | ✅ | DOS: some reserved (+, ,). POSIX: all allowed. |
| `!` `$` `'` `(` `)` `@` `~` | ✅ | ✅ | ✅ | ✅ | ✅ | All allowed |
| `0x00` (NUL) | ❌ No | ❌ No | ❌ No | ❌ No | ❌ No | NUL terminates C strings |
| `0x01`–`0x1F` (C0) | ❌ No | ❌ No | ❌ No | ❌ No | ❌ No | Control codes, disallowed |
| `0x7F` (DEL) | ✅ (ext4) | ❌ No | ✅ (NTFS) | ✅ | ✅ | Some FSs allow DEL |
| `0x80`–`0xFF` | ✅ (UTF-8) | ✅ (OEM codepage) | ✅ (UTF-16) | ✅ (UTF-8/16) | ✅ (ASCII) | High-bit bytes vary by encoding |
| Non-ASCII Unicode | ✅ (UTF-8) | ✅ (LFN) | ✅ (UTF-16) | ✅ (UTF-16) | ✅ (AmigaDOS UTF-8) | High-bit characters vary by FS encoding |

### Character Set Summary by Type

**DOS 8.3 (SFN):**
- Allowed: `A-Z`, `0-9`, space, `! # $ % & ' ( ) - @ ^ _ ` { } ~`
- Disallowed: lowercase `a-z`, `" * / : < > ? \ | + , . ; = [ ]`, `0x00`–`0x1F`, `0x7F`, `0xE5` (in some implementations)
- OEM codepage 0x80–0xFF allowed
- Case: stored as uppercase, case lost

**DOS/Windows LFN (Long Filename):**
- All Unicode characters except: `\`, `/`, `:`, `?`, `*`, `"`, `<`, `>`, `|`, `NUL`
- Case-insensitive, case-preserving (NTFS) / case-insensitive, case-lossing (FAT)

**POSIX:**
- All characters except: NUL (`0x00`), `/` (path separator)
- High-bit characters allowed (UTF-8 on modern systems)
- Wildcard characters (`*`, `?`) allowed but must be disallowed for API usability
- Some filesystems (ext4) allow `"`, `<`, `>`, `|`; others (JFS) may restrict them

**HFS+ (Classic MacOS):**
- All Unicode characters except: `:`, `NUL`, `/`
- `:` is the path separator (converted from `/`)
- Case-insensitive, case-preserving
- Unicode NFD normalization applied

**Amiga FFS:**
- All characters except: `:`, `;` (volume/directory separators), NUL
- 30-character filename limit
- Case-sensitive, case-preserving
- `/` is not a separator but may have special meaning in AmigaDOS commands

**OS/2 HPFS:**
- All Unicode characters except: `\`, `/`, `:`, `?`, `*`, `"`, `<`, `>`, `|`, `NUL`
- 64-character filename limit (8.3 mode: 8+3+2)
- Case-insensitive, case-preserving

### URL Character Sets (RFC 3986, §2)

**Unreserved (no encoding needed):**
- `A-Z`, `a-z`, `0-9`
- `-`, `.`, `_`, `~`

**Reserved (special meaning, must be percent-encoded as data):**
- Component delimiters: `:`, `/`, `?`, `#`
- Authority delimiters: `[`, `]`, `@`
- Sub-delimiters: `!`, `$`, `&`, `'`, `(`, `)`, `*`, `+`, `,`, `;`, `=`

**Percent-encoding:**
- Format: `%HH` (where HH are two hex digits)
- Used when a reserved character is needed as data

### Key Design Decisions for Shmoo

1. **Every path component carries its case-sensitivity flag** — not just the path itself
2. **VFS operations are always case-aware** — based on the target filesystem's rules
3. **Cross-environment case translation is logged** — when case is lost or ambiguous, the configuration chain records it
4. **Case-preserving filesystems get special handling** — the original case is stored and preserved across translations
5. **Conflict detection is automatic** — the VFS layer detects when case-insensitive filesystems might have collisions
6. **The path library's comparison function takes a case-sensitive flag** — based on the environment type
7. **Hash computation is context-aware** — hashes are case-sensitive or case-insensitive based on the FS
8. **Every translation between environments carries the FS type** — the configuration stash records whether the source and target are case-sensitive

---

### Cross-Platform Path Libraries (for reference)

- **Rust `std::path`** — [https://doc.rust-lang.org/std/path/](https://doc.rust-lang.org/std/path/)
  - Thorough cross-platform path handling with platform-specific edge cases documented.
- **Python `pathlib`** — [https://docs.python.org/3/library/pathlib.html](https://docs.python.org/3/library/pathlib.html)
  - Object-oriented path handling with platform-aware resolution.

---

*End of study guide.*
