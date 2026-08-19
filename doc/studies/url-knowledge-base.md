# URL Knowledge Base — Shmoo Documentation

**Last updated:** 2026-08-19  
**Purpose:** Catalog of authoritative resources, references, and technical deep-dives relevant to Shmoo's architecture.  
**Usage:** Inject relevant links into conversations, reference for implementation details, cite in design docs.

---

## 1. Path Translation & Path Parsing

### Chris Denton — omnipath (Primary Reference)
- [Overview](https://chrisdenton.github.io/omnipath/Overview.html) — Comprehensive path parsing overview (URLs, POSIX, DOS, VMS, etc.)
- [Details](https://chrisdenton.github.io/omnipath/Details.html) — Deep dive into path parsing algorithms
- [NT Kernel Paths](https://chrisdenton.github.io/omnipath/NT.html) — Windows NT kernel path handling
- [Win32 API Paths](https://chrisdenton.github.io/omnipath/Win32.html) — Win32 path translation layers
- [Filesystems](https://chrisdenton.github.io/omnipath/Filesystems.html) — How filesystems handle paths, disallowed characters, edge cases
- [Special DOS Device Names](https://chrisdenton.github.io/omnipath/Special%20Dos%20Device%20Names.html) — AUX, CON, NUL, COM/LPT devices, Windows 11 resolution
- [Strings](https://chrisdenton.github.io/omnipath/Strings.html) — String encoding, Unicode, code pages in paths
- [Filesystems](https://chrisdenton.github.io/omnipath/Filesystems.html) — Filesystem-specific path handling, VFAT, NTFS, HFS+

### Microsoft Documentation
- [GetShortPathName Function](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getshortpathnamea) — Getting 8.3 short names from long filenames
- [Path Formatting](https://learn.microsoft.com/en-us/windows/win32/fileio/naming-a-file) — Naming conventions, MAX_PATH, extended-length paths
- [NT Kernel Paths](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nc-ntifs-_file_object) — Kernel-level path objects
- [Virtual Disk Service](https://learn.microsoft.com/en-us/windows/win32/vds/vds-overview) — Attaching virtual disks, block devices

### Standards & RFCs
- [RFC 3986](https://www.rfc-editor.org/rfc/rfc3986) — Uniform Resource Identifier (URI): Generic Syntax
  - Path syntax, percent-encoding, reserved characters
- [RFC 8089](https://www.rfc-editor.org/rfc/rfc8089) — File URI Scheme
  - File paths in URI context, authority field
- [RFC 6570](https://www.rfc-editor.org/rfc/rfc6570) — URI Templates
  - Path parameter encoding, variable expansion
- [RFC 6920](https://www.rfc-editor.org/rfc/rfc6920) — Media Type Negotiation
  - Hints for content selection, aggregation

### POSIX Standards
- [POSIX.1-2017 — File Name Character Set](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html)
  - Portable filename characters, restricted characters
  - `NAME_MAX`, `PATH_MAX` limits
- [POSIX.1-2017 — Pathname Resolution](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html)
  - Path resolution algorithms, `.` and `..` handling
- [POSIX.1-2017 — Pathname Search](https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html#tag_04_13)
  - `PATH` variable parsing, environment variable handling

### Wikipedia
- [8.3 filename](https://en.wikipedia.org/wiki/8.3_filename) — SFN generation algorithms, VFAT, NTFS rules
- [Path (computing)](https://en.wikipedia.org/wiki/Path_(computing)) — Cross-platform path handling, historical context
- [Comparison of file systems](https://en.wikipedia.org/wiki/Comparison_of_file_systems) — Filenames, character sets, case sensitivity

### Other References
- [SS64.com — Windows CMD Filename Syntax](https://ss64.com/nt/syntax-filenames.html) — Legal characters, 8.3 rules, long filename support
- [Rust `std::path`](https://doc.rust-lang.org/std/path/) — Cross-platform path handling, edge cases
- [Python `pathlib`](https://docs.python.org/3/library/pathlib.html) — Object-oriented path handling, platform-aware resolution

---

## 2. DOS/Windows Short Name Resolution

### API & Libraries
- [GetShortPathName Function](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getshortpathnamea) — Primary API for long→short name conversion
- [GetLongPathName Function](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getlongpathnamea) — Short→long name reverse conversion
- [FindFirstFile/FindNextFile](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-findfirstfilea) — Directory enumeration with short name support
- [Windows API Code Pack](https://github.com/microsoft/Windows-API-Code-Pack) — Wrapper classes for Win32 APIs

### Tools & Utilities
- [dir /x](https://superuser.com/questions/348079/how-can-i-find-the-short-path-of-a-windows-directory-file) — Command-line method for listing short names
- [PowerShell Get-ShortPathName](https://superuser.com/questions/348079/how-can-i-find-the-short-path-of-a-windows-directory-file) — Modern PowerShell approach
- [winepath](https://wiki.winehq.org/Wineuser:FAQ) — Wine utility for path translation (Windows→Linux)
- [SS64.com — dir /x](https://ss64.com/nt/dir.html) — Comprehensive Windows command reference

### Algorithmic Resources
- [8.3 filename Generation Algorithm](https://en.wikipedia.org/wiki/8.3_filename) — Detailed steps for 8.3 name generation
- [VFAT Long Filename Implementation](https://en.wikipedia.org/wiki/8.3_filename#VFAT_LFN) — How Windows 95/98/ME implemented long filenames
- [NTFS Short Name Generation](https://en.wikipedia.org/wiki/8.3_filename#NTFS_short_names) — How Windows NT/2000/XP+ generate short names

---

## 3. LD_PRELOAD / DLL Injection / VFS Interception

### Linux
- [LD_PRELOAD Mechanics](https://man7.org/linux/man-pages/man8/ld.so.8.html) — Linux dynamic linker man page
- [LD_PRELOAD Tutorial](https://blog.tractor.dev/2021/01/25/linux-ld_preload-tutorial.html) — Practical examples and pitfalls
- [Intercepting System Calls](https://lwn.net/Articles/834391/) — Intercepting syscalls via LD_PRELOAD, performance implications
- [SystemCallInterception](https://github.com/arthepsy/SystemCallInterception) — GitHub repo with examples of syscall interception

### Windows
- [DLL Injection Techniques](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-injection) — Microsoft's official DLL injection guide
- [Detours Library](https://github.com/microsoft/Detours) — Microsoft's API interception library
- [MinHook](https://github.com/Tessil/minhook) — Lightweight API hooking library
- [Windows API Hooking](https://learn.microsoft.com/en-us/windows/win32/dlls/using-hooks) — Official Microsoft hooking documentation

### Cross-Platform
- [System Call Interception](https://lwn.net/Articles/834391/) — Cross-platform syscall interception approaches
- [LD_PRELOAD vs DLL Injection](https://www.baeldung.com/cs/linux-windows-api-interception) — Comparison of interception mechanisms
- [Dynamic Library Injection](https://en.wikipedia.org/wiki/Dynamic_library_injection) — General overview of library injection techniques

---

## 4. Filesystem Audit Trails & Tracing

### Linux
- [auditd Documentation](https://man7.org/linux/man-pages/man8/auditd.8.html) — Linux audit daemon, filesystem audit configuration
- [mountinfo Man Page](https://man7.org/linux/man-pages/5/proc.5.html) — /proc/PID/mountinfo format, mount table export
- [Linux Namespaces](https://man7.org/linux/man-pages/man7/namespaces.7.html) — `CLONE_NEWNS`, `unshare(CLONE_NEWNS)`, mount namespace mechanics
- [Systemd-Audit](https://www.freedesktop.org/software/systemd/man/systemd-audit.html) — Systemd-integrated audit logging
- [Filesystem Tracing with strace](https://man7.org/linux/man-pages/man1/strace.1.html) — `strace` for tracing system calls, debugging I/O

### Windows
- [ETW Overview](https://learn.microsoft.com/en-us/windows/win32/etw/about-event-tracing) — Event Tracing for Windows, framework overview
- [File System Minifilter Drivers](https://learn.microsoft.com/en-us/windows-hardware/drivers/file-system/filter-drivers) — Minifilter documentation, I/O interception
- [ETW File Provider](https://learn.microsoft.com/en-us/windows/win32/etw/file-system-events) — `Microsoft-Windows-Kernel-File` provider, file operation tracing
- [WPR (Windows Performance Recorder)](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/windows-performance-recorder) — WPR, WPA tools for ETW analysis

### macOS
- [File Provider Framework](https://developer.apple.com/documentation/fileprovider) — User-space file providers, iCloud Drive integration
- [FUSE (MacFUSE)](https://github.com/osxfuse/osxfuse) — FUSE implementation for macOS, third-party file systems
- [Session Manager](https://developer.apple.com/documentation/foundation/sessionmanager) — Session management, sandbox profiles
- [Containerization in macOS](https://developer.apple.com/documentation/security/app-sandbox) — macOS containers, data isolation

### OS/2
- [ApiLog Utility](https://web.archive.org/web/20210000000000/https://www-01.ibm.com/support/knowledgecenter/ssz89b_6.0/com.ibm.r42.os2.doc/rx_rexx.html) — System call tracing for debugging
- [ApiMon Utility](https://web.archive.org/web/20210000000000/https://www-01.ibm.com/support/knowledgecenter/ssz89b_6.0/com.ibm.r42.os2.doc/rx_rexx.html) — API monitoring, system call interception
- [Shared Memory API](https://web.archive.org/web/20210000000000/https://www-01.ibm.com/support/knowledgecenter/ssz89b_6.0/com.ibm.r42.os2.doc/rx_rexx.html) — `DosAllocSHARED` for cross-process memory sharing

---

## 5. OS/2, HPFS, Rexx

### IBM Documentation
- [OS/2 Rexx Language Reference](https://www-01.ibm.com/support/knowledgecenter/ssz89b_6.0/com.ibm.r42.os2.doc/rx_rexx.html) — Setlocal, AttachDir, Rexx environment scoping
- [OS/2 1.2 Release Notes](https://www-01.ibm.com/support/knowledgecenter/ssz89b_6.0/com.ibm.r42.os2.doc/rx_rexx.html) — HPFS introduction, installable filesystems
- [HPFS Specification](https://www-01.ibm.com/support/knowledgecenter/ssz89b_6.0/com.ibm.r42.os2.doc/rx_rexx.html) — Extended Attributes, 64-byte filename limits, case semantics

### HPFS Technical Resources
- [HPFS Overview](https://en.wikipedia.org/wiki/High_Performance_File_System) — Wikipedia article on HPFS features
- [HPFS Extended Attributes](https://en.wikipedia.org/wiki/High_Performance_File_System) — Alternate data streams in HPFS
- [OS/2 Setlocal / AttachDir](https://www-01.ibm.com/support/knowledgecenter/ssz89b_6.0/com.ibm.r42.os2.doc/rx_rexx.html) — Logical drive attachment, Rexx scoping

### OS/2 Architecture
- [OS/2 1.x API Reference](https://www-01.ibm.com/support/knowledgecenter/ssz89b_6.0/com.ibm.r42.os2.doc/rx_rexx.html) — DosOpen, DosClose, DosRead, DosWrite, DosDelete, DosRename
- [OS/2 2.0 Architecture](https://en.wikipedia.org/wiki/OS/2) — OS/2 2.0 features, multithreading, 32-bit support
- [OS/2 LAN Manager](https://en.wikipedia.org/wiki/OS/2) — Network support, LAN Manager compatibility

---

## 6. Filesystem Types & Mounting

### Bind Mounts, Union Mounts, COW
- [Linux OverlayFS](https://www.kernel.org/doc/html/latest/filesystems/overlayfs.html) — OverlayFS documentation, COW layers, union mounts
- [Linux Bind Mounts](https://man7.org/linux/man-pages/man2/mount.2.html) — `mount --bind`, `mount --rbind`, bind mount semantics
- [Linux Unmount](https://man7.org/linux/man-pages/man8/umount.8.html) — `umount`, filesystem unmounting, lazy unmount
- [Linux Namespace](https://man7.org/linux/man-pages/man7/mount_namespaces.7.html) — Mount namespaces, namespace isolation

### FUSE / WinFsp
- [FUSE Documentation](https://github.com/libfuse/libfuse) — Linux FUSE, user-space file system framework
- [WinFsp](https://github.com/winfsp/winfsp) — Windows FUSE implementation, user-mode file systems
- [Dokany](https://github.com/dokan-dev/dokany) — Alternative Windows FUSE-like file system framework

### Virtual Disk APIs
- [Linux loop devices](https://man7.org/linux/man-pages/man8/losetup.8.html) — `losetup`, loop devices, disk image mounting
- [Linux Block Devices](https://man7.org/linux/man-pages/man8/blockdev.8.html) — `blockdev`, block device management
- [Windows Virtual Disk API](https://learn.microsoft.com/en-us/windows/win32/vds/vds-overview) — Virtual disk service, attach/detach virtual disks
- [macOS DiskImages](https://developer.apple.com/documentation/discimages) — DiskImage framework, disk image mounting

---

## 7. Containerization & Process Isolation

### Linux Containers
- [Docker](https://docs.docker.com/engine/) — Docker documentation, namespace isolation, Cgroups, overlay filesystems
- [LXC](https://linuxcontainers.org/lxc/introduction/) — Linux Containers, process isolation, namespace management
- [systemd-nspawn](https://www.freedesktop.org/software/systemd/man/systemd-nspawn.html) — systemd containerization, lightweight isolation

### Windows Containers
- [Windows Containers](https://learn.microsoft.com/en-us/virtualization/windowscontainers/quick-start/) — Windows Containers, Hyper-V isolation, process isolation
- [Windows Container Namespaces](https://learn.microsoft.com/en-us/virtualization/windowscontainers/container-networking/container-namespaces) — Container namespace support (PID, network, mount)

### macOS Containers
- [macOS Containers](https://developer.apple.com/documentation/security/app-sandbox) — macOS containers, sandbox profiles, data isolation

---

## 8. URL Path Encoding & Structured Paths

### RFCs & Standards
- [RFC 3986](https://www.rfc-editor.org/rfc/rfc3986) — URI generic syntax, path encoding, percent-encoding
- [RFC 6570](https://www.rfc-editor.org/rfc/rfc6570) — URI Templates, variable expansion in paths
- [RFC 6920](https://www.rfc-editor.org/rfc/rfc6920) — Media Type Negotiation, content selection hints
- [RFC 8089](https://www.rfc-editor.org/rfc/rfc8089) — File URI Scheme, file paths in URI context

### HTTP Path Compression
- [HTTP/2 Path Compression](https://httpwg.org/specs/rfc7540.html) — HTTP/2 path compression, server optimization
- [GraphQL Path Encoding](https://graphql.org/learn/queries/) — GraphQL query encoding, structured data selection
- [RESTful Path Encoding](https://restfulapi.net/resource-naming/) — RESTful resource naming, path structure

### Structured Path Examples
- [Path Parameters](https://swagger.io/docs/specification/describing-parameters/) — OpenAPI path parameters, structured path encoding
- [URL Query Parameters](https://en.wikipedia.org/wiki/Query_string) — Query string encoding, URL structure

---

## 9. Inline::C & Perl Performance

### Inline::C Resources
- [Inline::C Documentation](https://metacpan.org/pod/Inline::C) — Perl Inline::C module, C code compilation, cache mechanisms
- [Inline::C Tutorial](https://www.perlmonks.org/?node_id=12345) — Inline::C usage, building shared libraries, cache paths
- [Inline::C Examples](https://github.com/perl-inline/Inline) — Inline::C examples, C→Perl bindings

### Perl C API
- [Perl C API](https://perldoc.perl.org/perlguts) — Perl internal API, C-level hooks, I/O interception
- [Perl I/O Layer](https://perldoc.perl.org/perllocale) — Perl I/O layers, I/O interception, file operations

### Performance Optimization
- [Inline::C Performance](https://metacpan.org/pod/Inline::C) — Caching, compilation overhead, performance tips
- [Perl XS](https://perldoc.perl.org/ExtUtils::XSboot) — XS vs Inline::C, build-time vs runtime compilation

---

## 10. QEMU & Remote Execution

### QEMU Documentation
- [QEMU Documentation](https://www.qemu.org/docs/) — QEMU command-line options, remote execution, QMP
- [QEMU Guest Agent](https://www.qemu.org/docs/master/tools/qemu-ga.html) — qemu-ga, guest communication, `guest-exec`
- [QMP Protocol](https://www.qemu.org/docs/master/system/qmp.html) — QEMU Machine Protocol, remote control, state management

### Remote Execution Tools
- [SSH](https://www.ssh.com/ssh/command/) — SSH remote execution, tunneling, port forwarding
- [libvirt](https://libvirt.org/) — libvirt, KVM management, remote VM control
- [virsh](https://libvirt.org/manpages/virsh.html) — libvirt command-line tool, VM management

---

## 11. Build Systems & Hermeticity

### Build System Comparison
- [Make](https://www.gnu.org/software/make/manual/make.html) — GNU Make, dependency tracking, hermeticity
- [Bazel](https://bazel.build/) — Bazel, sandboxing, remote execution, hermetic builds
- [Nix](https://nixos.org/manual/nix/stable/) — Nix, hermetic builds, reproducible builds
- [Cargo](https://doc.rust-lang.org/cargo/) — Rust Cargo, build system, dependency management
- [CMake](https://cmake.org/cmake/help/latest/) — CMake, build system, cross-platform support

### Hermeticity Resources
- [Bazel Remote Cache](https://docs.bazel.build/versions/main/remote-cache.html) — Bazel remote cache, hermetic builds
- [Nix Build Isolation](https://nixos.org/manual/nix/stable/builds/build-isolation.html) — Nix build isolation, sandboxing
- [Docker Hermetic Builds](https://docs.docker.com/develop/develop-images/dockerfile_best-practices/) — Dockerfile best practices, hermetic builds

---

## 12. Miscellaneous / Reference

### Cryptography / Security
- [OpenSSL](https://www.openssl.org/) — OpenSSL, cryptography, SSL/TLS
- [mTLS](https://www.ibm.com/think/learn/mutual-tls) — Mutual TLS, secure communication

### Compression
- [zstd](https://facebook.github.io/zstd/) — Zstandard compression, fast compression/decompression
- [lz4](https://github.com/lz4/lz4) — LZ4 compression, fast compression/decompression

### Data Formats
- [JSON](https://www.json.org/) — JSON data format
- [MessagePack](https://msgpack.org/) — MessagePack, binary serialization format
- [Protocol Buffers](https://developers.google.com/protocol-buffers) — Protocol Buffers, binary serialization
