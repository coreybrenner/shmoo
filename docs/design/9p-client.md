# Design: 9p Client Library — Net/9p Protocol Over TCP/Unix Socket

*2025-01-23 | Status: Design Draft*
*Purpose: A lightweight C library implementing the 9p2000.L protocol over TCP and Unix domain sockets, providing transparent filesystem access for the Shmoo build system.*

---

## 1. Protocol Overview

9p2000.L is a simple, text-based client-server filesystem protocol from the Plan 9 project. The ".L" suffix indicates "long strings" — string fields use 4-byte big-endian length prefixes instead of 1-byte.

### Wire Format

All messages share the same header structure:

```
┌────────────┬─────────┬─────────┬──────────┐
│    Size    │  Type   │   Tag   │ Payload  │
│  (4B BE)   │ (1B)    │ (2B BE) │ Variable │
└────────────┴─────────┴─────────┴──────────┘
```

- **Size (4 bytes, big-endian):** Total message length including header (not counting the 4-byte size field itself)
- **Type (1 byte):** Message type (see §3)
- **Tag (2 bytes, big-endian):** Associative tag matching T/R pairs
- **Payload:** Message-specific data

### Message Types

| Type | T (Request) | R (Response) | Name |
|------|-------------|--------------|------|
| 6 | Tversion | Rversion | Version exchange |
| 8 | Tauth | Rauth | Authenticate |
| 10 | Tattach | Rattach | Attach to fid |
| 12 | Twalk | Rwalk | Walk path |
| 14 | Topen | Ropen | Open fid |
| 16 | Tcreate | Rcreate | Create file |
| 18 | Tread | Rread | Read from fid |
| 20 | Twrite | Rwrite | Write to fid |
| 22 | Tstat | Rstat | Stat fid |
| 24 | Twstat | Rwstat | Wstat fid |
| 26 | Tremove | Rremove | Remove fid |
| 28 | Tclunk | Rclunk | Destroy fid |
| 30 | Tflush | Rflush | Flush request |
| 32 | Tsymlink | Rsymlink | Symlink (9p2000.u) |
| 34 | Tmkdir | Rmkdir | Create directory |
| 36 | Trename | Rrename | Rename file |
| 38 | Tmknod | Rmknod | Create special file |
| 40 | Trenameat | Rrenameat | Rename at dirfd (9p2000.u) |
| 42 | Tunlinkat | Runlinkat | Unlink at dirfd (9p2000.u) |

### Tag Management

Tags are 16-bit values. The client maintains an active tag table mapping request tags to pending requests. When a response arrives, the tag is used to find the corresponding pending request. Tag 0 is reserved for the `Tflush` message.

---

## 2. Library Architecture

### Directory Structure

```
src/9pclient/
├── 9pclient.h          # Public API
├── 9pclient.c          # Core client implementation
├── 9p_wire.c           # Wire protocol encoding/decoding
├── 9p_fcall.c          # Fcall structure (request/response objects)
├── 9p_fid.c            # FID (file descriptor) management
├── 9p_io.c             # Read/write operations
├── 9p_mount.c          # Mount/attach logic
├── 9p_walk.c           # Path walking
├── 9p_stat.c           # Stat operations
├── 9p_fsops.c          # High-level filesystem operations
├── 9p_tcp.c            # TCP transport
├── 9p_unix.c           # Unix domain socket transport
├── 9p_buf.c            # Buffer management
└── test/
    ├── test_client.c   # Unit tests
    └── test_wire.c     # Wire protocol tests
```

### Library Dependencies

- **Zero external dependencies** — pure C99 with POSIX headers
- Uses only `stdio.h`, `stdlib.h`, `string.h`, `stdint.h`, `errno.h`
- Network: `unistd.h` (for `write()`, `read()`)
- TCP: `sys/socket.h`, `netinet/in.h`, `arpa/inet.h`
- Unix socket: `sys/un.h`

---

## 3. Public API

### Connection Management

```c
#include "9pclient.h"

/*
 * Connect to a 9p server.
 *
 * Returns a 9p_conn* on success, NULL on failure.
 * 
 * Transport types:
 *   9P_CONN_TCP  — TCP connection: address is "host:port"
 *   9P_CONN_UNIX — Unix domain socket: address is a file path
 *
 * Examples:
 *   conn = 9p_connect("tcp:localhost:5640", 9P_CONN_TCP);
 *   conn = 9p_connect("/tmp/9p.sock", 9P_CONN_UNIX);
 */
typedef enum {
    9P_CONN_TCP = 1,
    9P_CONN_UNIX = 2
} 9p_conn_type_t;

typedef struct 9p_conn 9p_conn_t;

9p_conn_t* 9p_connect(const char* address, 9p_conn_type_t type);
int         9p_close(9p_conn_t* conn);

/* Set timeouts for the connection */
void 9p_set_timeout(9p_conn_t* conn, int ms_connect, int ms_io);
```

### Version Exchange and Mount

```c
/*
 * Perform version exchange and attach to the server's root.
 * Returns 0 on success, -1 on failure.
 * 
 * msize: Maximum message size (default 9p2000.L uses 8192-65536)
 * uname: Username for authentication (pass NULL for anonymous)
 * uname: Username for authentication (pass NULL for anonymous)
 * aname: Attachment name (the server path to mount, usually "")
 * 
 * After this call, the connection is mounted and ready for operations.
 */
int  9p_mount(9p_conn_t* conn, int msize, const char* uname,
              const char* uname, const char* aname);

/*
 * Get the FID of the root directory after mounting.
 * The FID must be clunked (freed) by the caller when done.
 */
int  9p_root_fid(9p_conn_t* conn);
```

### High-Level File Operations

```c
/*
 * Read an entire file into a buffer.
 * Caller must free(buf).
 * Returns number of bytes read, or -1 on error.
 */
int  9p_read_file(9p_conn_t* conn, const char* path,
                  char** buf_out, size_t* size_out);

/*
 * Write data to a file, creating intermediate directories as needed.
 * Returns 0 on success, -1 on error.
 */
int  9p_write_file(9p_conn_t* conn, const char* path,
                   const char* data, size_t len);

/*
 * Get file metadata (stat).
 */
typedef struct {
    uint64_t  size;
    uint64_t  mtime;
    uint64_t  atime;
    uint64_t  ctime;
    uint32_t  mode;
    uint32_t  uid;
    uint32_t  gid;
    uint32_t  nlink;
    uint32_t  dev;
    char     name[256];
    int      is_dir;
    int      is_symlink;
} 9p_stat_t;

int  9p_stat(9p_conn_t* conn, const char* path, 9p_stat_t* stat_out);
int  9p_lstat(9p_conn_t* conn, const char* path, 9p_stat_t* stat_out);
int  9p_is_dir(9p_conn_t* conn, const char* path);

/*
 * Read directory entries.
 * Returns count of entries on success, -1 on error.
 * Caller must free(dirs).
 */
typedef struct {
    char name[256];
    uint64_t size;
    int is_dir;
} 9p_dirent_t;

int  9p_readdir(9p_conn_t* conn, const char* path,
                9p_dirent_t** dirs_out, int* count_out);

/*
 * Remove a file or directory.
 */
int  9p_remove(9p_conn_t* conn, const char* path);
int  9p_mkdir(9p_conn_t* conn, const char* path, uint32_t mode);
```

### Low-Level FID Operations (for fine-grained control)

```c
/*
 * Open a FID for a path (walk + open).
 * Returns a FID number on success, -1 on error.
 * 
 * mode: O_RDONLY, O_WRONLY, O_RDWR
 * Returns a FID number on success, -1 on error.
 */
int  9p_open(9p_conn_t* conn, const char* path, uint8_t mode);

/*
 * Read from a FID.
 * Returns bytes read, or -1 on error.
 */
int  9p_read(9p_conn_t* conn, int fid, char* buf, int count);

/*
 * Write to a FID.
 * Returns bytes written, or -1 on error.
 */
int  9p_write(9p_conn_t* conn, int fid, const char* buf, int count);

/*
 * Walk a path from a FID.
 * Walks from fid along path components, returns new FID.
 * Returns a new FID number on success, -1 on error.
 * 
 * The caller must clunk() the returned FID when done.
 */
int  9p_walk(9p_conn_t* conn, int oldfid, const char* path);

/*
 * Clunk (destroy) a FID.
 */
int  9p_clunk(9p_conn_t* conn, int fid);

/*
 * Read the raw stat buffer for a FID.
 * Caller must free(buf).
 * Returns stat buffer size, or -1 on error.
 */
int  9p_fid_stat(9p_conn_t* conn, int fid, char** buf_out);
```

### Error Handling

```c
/*
 * Get the last error message.
 * Returns a static string (like strerror).
 */
const char*  9p_errstr(void);

/*
 * Check if a connection is still alive.
 * Returns 1 if alive, 0 if not.
 */
int  9p_alive(9p_conn_t* conn);
```

---

## 4. Wire Protocol Implementation

### Encoding Functions

```c
/* Encode/decode 32-bit and 16-bit big-endian integers */
void  9p_puth32(unsigned char* buf, int32_t val);
int32_t  9p_geth32(const unsigned char* buf);
void  9p_puth16(unsigned char* buf, int16_t val);
int16_t  9p_geth16(const unsigned char* buf);

/* Encode/decode 9p long strings (4-byte length prefix + bytes) */
int  9p_puts(unsigned char* buf, int buf_len, const char* str);
int  9p_gets(unsigned char* buf, int buf_len, int* offset,
             char* out, int out_max);

/* Encode/decode 9p 8-bit values */
void  9p_put8(unsigned char* buf, uint8_t val);
uint8_t  9p_get8(const unsigned char* buf);

/* Encode/decode 9p 64-bit values (for mode, size, etc.) */
void  9p_puth64(unsigned char* buf, uint64_t val);
uint64_t  9p_geth64(const unsigned char* buf);
```

### Fcall Structure (Request/Response Object)

```c
typedef struct 9p_fcall {
    unsigned char* data;    /* Raw wire data */
    int            len;     /* Total length of data */
    int            offset;  /* Current read position */
    uint8_t        type;    /* Message type */
    int16_t        tag;     /* Message tag */
    int            msize;   /* Maximum size of this fcall */
} 9p_fcall_t;

/* Create/destroy fcall objects */
9p_fcall_t*  9p_fcall_new(int msize);
void         9p_fcall_free(9p_fcall_t* fc);

/* Encode request structures */
int  9p_tversion(9p_fcall_t* fc, int msize, const char* version);
int  9p_tauth(9p_fcall_t* fc, int afid, const char* uname,
              const char* uname, const char* aname);
int  9p_tattach(9p_fcall_t* fc, int fid, int afid,
                const char* uname, const char* uname,
                const char* aname);
int  9p_twalk(9p_fcall_t* fc, int fid, int newfid,
              int count, char* wname[]);
int  9p_topen(9p_fcall_t* fc, int fid, uint8_t mode);
int  9p_tread(9p_fcall_t* fc, int fid, uint64_t offset, int count);
int  9p_twrite(9p_fcall_t* fc, int fid, uint64_t offset,
               int count, const char* data);
int  9p_tstat(9p_fcall_t* fc, int fid);
int  9p_twstat(9p_fcall_t* fc, int fid, const char* stat_data,
               int stat_len);
int  9p_tremove(9p_fcall_t* fc, int fid);
int  9p_tclunk(9p_fcall_t* fc, int fid);
int  9p_tflush(9p_fcall_t* fc, int oldtag);
int  9p_tmkdir(9p_fcall_t* fc, int fid, const char* name,
               uint32_t mode);
int  9p_tcreate(9p_fcall_t* fc, int fid, const char* name,
                uint8_t perm, uint8_t mode, const char* uname);
int  9p_tsymlink(9p_fcall_t* fc, int fid, const char* name,
                 int dfid, const char* dname, const char* uname);
int  9p_trename(9p_fcall_t* fc, int fid, int dfid,
                const char* dname);

/* Decode response structures */
int  9p_rversion(9p_fcall_t* fc, int* msize, char* version,
                 int version_max);
int  9p_rwalk(9p_fcall_t* fc, int* nwalk, uint16_t* wqid);
int  9p_rstat(9p_fcall_t* fc, char* statbuf, int statbuf_max);
```

---

## 5. Transport Layer

### TCP Transport

```c
/*
 * Open a TCP connection.
 * Returns file descriptor on success, -1 on error.
 * Connects to host:port.
 */
int  9p_tcp_connect(const char* host, int port,
                    int timeout_ms);

/*
 * Read exactly n bytes from the socket.
 * Returns bytes read, 0 on EOF, -1 on error.
 * Blocks until n bytes are read (with timeout).
 */
int  9p_tcp_read(int fd, char* buf, int n, int timeout_ms);

/*
 * Write exactly n bytes to the socket.
 * Returns bytes written, -1 on error.
 * Blocks until all bytes are written.
 */
int  9p_tcp_write(int fd, const char* buf, int n, int timeout_ms);
```

### Unix Domain Socket Transport

```c
/*
 * Open a Unix domain socket connection.
 * Returns file descriptor on success, -1 on error.
 */
int  9p_unix_connect(const char* path, int timeout_ms);

/* Use the same read/write functions as TCP (fd-based) */
```

### Unified Transport Interface

```c
typedef struct 9p_transport {
    int           fd;        /* File descriptor */
    int           type;      /* 9P_CONN_TCP or 9P_CONN_UNIX */
    int           timeout_ms;
    
    /* Transport-specific operations */
    int  (*read_fn)(int fd, char* buf, int n, int timeout_ms);
    int  (*write_fn)(int fd, const char* buf, int n, int timeout_ms);
    void (*close_fn)(int fd);
} 9p_transport_t;
```

---

## 6. Path Walking Implementation

The core algorithm for walking a path from a FID:

```
1. Parse the path into components (e.g., "/a/b/c" → ["", "a", "b", "c"])
2. Start from the given FID
3. For each component (except the last):
   a. Call Twalk(oldfid, newfid, [component])
   b. Use the returned newfid as oldfid for the next component
4. For the final component:
   a. Call Twalk(oldfid, newfid, [component]) for stat
   b. OR Topen(oldfid, mode) for file access
```

**Key details:**
- The first component of a path starting with "/" is empty — Twalk from the root FID
- Each Twalk call returns the QIDs of the walked-to FIDs
- If any component fails, the entire walk fails
- Path normalization: collapse `//`, strip trailing `/`, resolve `.` and `..` locally before sending to the server

---

## 7. Buffer Management

### Message Buffer

All message exchange uses a fixed-size buffer (configurable, default 65536 bytes):

```c
typedef struct 9p_buf {
    unsigned char* data;  /* Buffer data */
    int            len;   /* Current length */
    int            cap;   /* Allocated capacity */
} 9p_buf_t;

9p_buf_t*  9p_buf_new(int capacity);
void       9p_buf_free(9p_buf_t* buf);
void       9p_buf_reset(9p_buf_t* buf);
int        9p_buf_append(9p_buf_t* buf, const unsigned char* data, int len);
int        9p_buf_reserve(9p_buf_t* buf, int needed);  /* Ensures capacity */
```

### Read Buffer (Receiving)

A separate receive buffer accumulates incoming data until a complete message is available:

```c
typedef struct 9p_rbuf {
    unsigned char* data;
    int            pos;   /* Read position */
    int            len;   /* Total received data */
    int            cap;
} 9p_rbuf_t;

/* Returns number of bytes consumed, 0 if more data needed, -1 on error */
int  9p_rbuf_process(9p_rbuf_t* rbuf, const unsigned char* input, int in_len);
```

### Message Framing (Read-Echo)

The 9p protocol uses a simple framing scheme:
1. Read 4 bytes → get message size
2. Read `size` bytes → get the full message
3. Parse the message header → get type, tag, payload
4. Process the message

---

## 8. Error Handling

### Error Convention

- Functions return `0` on success, `-1` on error
- On error, `errno` is set to the underlying OS error
- `9p_errstr()` returns a human-readable error string (like `strerror`)
- Specific 9p protocol errors are encoded in the response payload

### Error Mapping

| 9p Error Code | Meaning | errno |
|---------------|---------|-------|
| ENAMETOOLONG | Path too long for server | ENAMETOOLONG |
| ENOENT | File does not exist | ENOENT |
| EIO | I/O error (e.g., truncated message) | EIO |
| EPERM | Permission denied | EPERM |
| EEXIST | File already exists | EEXIST |
| ENOSPC | No space on device | ENOSPC |
| EACCES | Access denied | EACCES |
| EISDIR | Is a directory | EISDIR |
| EAGAIN | Resource temporarily unavailable | EAGAIN |

---

## 9. Implementation Plan

### Phase 1: Core Wire Protocol (Week 1)
- [ ] Implement buffer management (`9p_buf.c`, `9p_rbuf.c`)
- [ ] Implement wire encoding/decoding (`9p_wire.c`)
- [ ] Implement fcall structure and message creation (`9p_fcall.c`)
- [ ] Write unit tests for wire encoding (Tversion, Tattach, etc.)

### Phase 2: Transport Layer (Week 2)
- [ ] Implement TCP transport (`9p_tcp.c`)
- [ ] Implement Unix socket transport (`9p_unix.c`)
- [ ] Implement connection management (`9p_mount.c`)
- [ ] Write integration tests (TCP and Unix socket connections)

### Phase 3: FID Operations (Week 3)
- [ ] Implement FID management (`9p_fid.c`)
- [ ] Implement path walking (`9p_walk.c`)
- [ ] Implement open/read/write (`9p_io.c`)
- [ ] Implement stat (`9p_stat.c`)

### Phase 4: High-Level API (Week 4)
- [ ] Implement high-level file operations (`9p_fsops.c`)
- [ ] Implement error handling (`9p_errstr`)
- [ ] Implement directory operations (readdir, mkdir, remove)
- [ ] Write end-to-end integration tests

### Phase 5: Testing & Hardening (Week 5)
- [ ] Stress test: 1000+ operations, large files
- [ ] Test error recovery (broken pipes, truncated messages)
- [ ] Test with real 9p servers (9pd, 9pfuse, Go9pServer)
- [ ] Benchmark: latency, throughput

---

## 10. Integration with Shmoo

The 9p client library will be used by:

### Syscall Interceptor
- When `open()`, `stat()`, `read()`, etc. are intercepted, the interceptor checks if the path is on a 9p mount
- If so, the interceptor delegates to the 9p client library
- If not, the call falls through to the real libc function

### Delusion Shell
- The delshell mounts remote filesystems via the 9p client
- The engineer's file operations inside the delshell are transparently forwarded over 9p

### Build Actions
- Each build action mounts its inputs via the 9p client
- Tools within the action read sources and write outputs through the 9p client

### Content-Addressed Cache
- Cached build outputs are stored on a remote 9p server
- The client retrieves cached results for fast replay

---

## 11. Design Decisions & Rationale

### Why 9p2000.L (not 9p2000.u)?

The ".L" (long string) format is sufficient for the Shmoo use case:
- No need for Unicode path names (build paths are ASCII)
- Simpler wire format (4-byte length vs. 8-byte for ".u")
- More compact on the wire
- Can upgrade to ".u" later if needed

### Why TCP + Unix Sockets (not just one)?

- **TCP:** For remote 9p servers, cross-host builds, VM-to-host communication
- **Unix Socket:** For local 9p servers, higher performance (no TCP overhead), no network namespace issues

### Why Not Use lib9p (Plan 9 port)?

Plan 9 port's `lib9.a` has issues:
- Non-standard `mk` build system (already abandoned for Shmoo)
- Circular dependency issues (see `rfork`/`lib9` problem)
- Monolithic static library (not suitable for `LD_PRELOAD`)
- Not suitable for sharing as a clean C library with documented API

### Why C (not Perl/Python)?

- **Zero dependencies:** The 9p client must work in minimal environments
- **Performance:** Critical path (syscall interception) needs minimal overhead
- **LD_PRELOAD compatibility:** Can be linked directly into the interceptor
- **Embeddable:** Can be statically linked into the Shmoo build engine

---

## 12. Performance Targets

| Metric | Target |
|--------|--------|
| Single operation latency (local unix socket) | < 100µs |
| Single operation latency (remote TCP, LAN) | < 1ms |
| Throughput (sequential read, 4KB blocks) | > 100MB/s |
| Throughput (sequential write, 4KB blocks) | > 50MB/s |
| Memory overhead (per connection) | < 1MB |
| Wire protocol overhead (per message) | 8 bytes (header) + string lengths |

---

## 13. Summary

The 9p client library is a lightweight, zero-dependency C implementation of the 9p2000.L protocol over TCP and Unix domain sockets. It provides:

- **Full 9p2000.L support:** version, auth, attach, walk, open, read, write, stat, create, remove, clunk, flush, mkdir, symlink, rename
- **Simple API:** Connect, mount, read/write/stat/remove — four operations
- **Transparent transport:** TCP and Unix sockets with unified interface
- **No external dependencies:** Pure C99, POSIX headers only
- **LD_PRELOAD-ready:** Can be linked into the syscall interceptor
- **Comprehensive testing:** Unit tests for wire protocol, integration tests for transport

This library is the foundation for transparent remote filesystem access in the Shmoo build system.
