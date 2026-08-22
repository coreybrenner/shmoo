# Design: 9p Server Library — Local Directory Tree as 9p Endpoint

*2025-01-23 | Status: Design Draft*
*Purpose: A lightweight C library implementing a 9p2000.L server that exposes a local directory tree as a 9p endpoint, enabling the Shmoo build system to provide transparent filesystem access over TCP or Unix sockets.*

---

## 1. Architecture Overview

The 9p server exposes a local directory tree as a 9p filesystem, responding to 9p protocol requests from clients (the 9p client library, delshell, build actions).

### Directory Structure

```
src/9psrv/
├── 9psrv.h             # Public API
├── 9psrv.c             # Core server implementation
├── 9psrv_wire.c        # Wire protocol encoding/decoding (response side)
├── 9psrv_fcall.c       # Fcall structure (response creation)
├── 9psrv_fs.c          # Local filesystem operations (stat, readdir, etc.)
├── 9psrv_dir.c         # Directory tree management
├── 9psrv_tcp.c         # TCP server (accept connections)
├── 9psrv_unix.c        # Unix domain socket server
├── 9psrv_handle.c      # Request handlers (Tversion, Tattach, Twalk, etc.)
├── 9psrv_request.c     # Request parsing and dispatch
├── 9psrv_session.c     # Session management (fids, mounts)
├── 9psrv_acl.c         # Access control (optional permission filtering)
├── 9psrv_buf.c         # Buffer management
├── 9psrv_cache.c       # Metadata cache (stat cache for readdir)
└── test/
    ├── test_server.c   # Unit tests
    └── test_integration.c  # Integration tests
```

### Library Dependencies

- **Zero external dependencies** — pure C99 with POSIX headers
- Uses only `stdio.h`, `stdlib.h`, `string.h`, `stdint.h`, `errno.h`
- Network: `unistd.h`, `sys/socket.h`, `netinet/in.h`, `arpa/inet.h`, `sys/un.h`
- Filesystem: `fcntl.h`, `sys/stat.h`, `dirent.h`, `unistd.h`, `pwd.h`, `grp.h`

---

## 2. Public API

### Server Creation and Configuration

```c
#include "9psrv.h"

/*
 * Create a 9p server instance.
 *
 * Returns a 9p_srv* on success, NULL on error.
 *
 * Parameters:
 *   root_path:    The local directory tree to expose as 9p root
 *   msize:        Maximum message size (default: 65536)
 *   uname:        Allowed usernames (NULL = allow all)
 *   uname_check:  Callback to validate user names (NULL = allow all)
 *   dotu:         Whether to support 9p2000.u (extensions)
 */
typedef struct 9p_srv 9p_srv_t;

typedef struct {
    int            msize;
    int            dotu;
    char*          uname;           /* NULL = allow all */
    int          (*uname_check)(const char* uname);  /* NULL = allow all */
    int            acl_enabled;     /* Enable ACL filtering */
    int          (*acl_check)(const char* user, const char* path, int perm);  /* NULL = allow all */
    int            cache_ttl_us;    /* Stat cache TTL in microseconds (0 = disabled) */
} 9p_srv_config_t;

9p_srv_t*  9p_srv_new(const char* root_path, const 9p_srv_config_t* config);
void       9p_srv_free(9p_srv_t* srv);
```

### Listening and Connection Acceptance

```c
/*
 * Start listening on a TCP port.
 * Returns 0 on success, -1 on error.
 */
int  9p_srv_listen_tcp(9p_srv_t* srv, const char* host, int port);

/*
 * Start listening on a Unix domain socket.
 * Returns 0 on success, -1 on error.
 */
int  9p_srv_listen_unix(9p_srv_t* srv, const char* path);

/*
 * Accept a client connection.
 * Returns the file descriptor for the connection on success, -1 on error.
 */
int  9p_srv_accept(9p_srv_t* srv);

/*
 * Run the server loop (accept connections and process requests).
 * This is a blocking call. Call 9p_srv_stop() to break out.
 */
void  9p_srv_run(9p_srv_t* srv);

/*
 * Stop the server loop.
 */
void  9p_srv_stop(9p_srv_t* srv);

/*
 * Get the list of listening sockets (for select/poll/epoll integration).
 * Returns the number of sockets, fills out_fds array.
 */
int  9p_srv_get_fds(9p_srv_t* srv, int* out_fds, int max_fds);

/*
 * Process pending connections (for non-blocking/event-driven integration).
 * Returns the number of events processed.
 */
int  9p_srv_poll(9p_srv_t* srv);
```

### Client Session Management

```c
/*
 * Get a list of connected clients.
 * Returns the number of clients, fills out_clients array.
 */
typedef struct 9p_client {
    int            fd;
    char           uname[256];
    time_t         connect_time;
} 9p_client_t;

int  9p_srv_clients(9p_srv_t* srv, 9p_client_t** clients_out);

/*
 * Disconnect a client.
 * Returns 0 on success, -1 on error.
 */
int  9p_srv_disconnect(9p_srv_t* srv, int fd);
```

### Directory Tree Management

```c
/*
 * Add a bind mount (expose a subdirectory at a specific path).
 * This allows the server to expose multiple local directories at different paths.
 *
 * Returns 0 on success, -1 on error.
 */
int  9p_srv_bind(9p_srv_t* srv, const char* path, const char* local_path);

/*
 * Remove a bind mount.
 * Returns 0 on success, -1 on error.
 */
int  9p_srv_unbind(9p_srv_t* srv, const char* path);

/*
 * Reload the directory tree (rescan for changes).
 * Call this if the underlying directory tree has changed.
 */
int  9p_srv_reload(9p_srv_t* srv);
```

---

## 3. Request Handlers

The server implements handlers for each 9p2000.L message type:

### Version Exchange (Tversion / Rversion)

```c
/*
 * Handle Tversion: Parse the client's version string and msize.
 * Accept version "9p2000.L" (the server's version).
 * The msize is the minimum of client and server msize.
 *
 * Returns 0 on success, -1 on error.
 */
int  9p_handle_tversion(9p_srv_t* srv, 9p_client_t* client, 9p_fcall_t* req_fc, 9p_fcall_t* resp_fc);
```

### Authentication (Tauth / Rauth)

```c
/*
 * Handle Tauth: Validate the user's identity.
 * The uname is checked against the uname_check callback (if set).
 * If no uname_check callback is set, all usernames are accepted.
 *
 * Returns 0 on success, -1 on error.
 */
int  9p_handle_tauth(9p_srv_t* srv, 9p_client_t* client, 9p_fcall_t* req_fc, 9p_fcall_t* resp_fc);
```

### Attach (Tattach / Rattach)

```c
/*
 * Handle Tattach: Attach to the root directory (or a bind-mounted subdirectory).
 * The aname is used to resolve bind mounts (if aname == "build", use the "build" bind mount).
 * A new FID is allocated for the client.
 *
 * Returns 0 on success, -1 on error.
 */
int  9p_handle_tattach(9p_srv_t* srv, 9p_client_t* client, 9p_fcall_t* req_fc, 9p_fcall_t* resp_fc);
```

### Walk (Twalk / Rwalk)

```c
/*
 * Handle Twalk: Walk a path from the given FID.
 * Each component of the path is walked sequentially.
 * The server returns the QIDs of the walked-to FIDs.
 *
 * Returns 0 on success, -1 on error.
 */
int  9p_handle_twalk(9p_srv_t* srv, 9p_client_t* client, 9p_fcall_t* req_fc, 9p_fcall_t* resp_fc);
```

### Open (Topen / Ropen)

```c
/*
 * Handle Topen: Open a FID for a file/directory.
 * The mode is checked against the file's permissions.
 * A new FID is allocated if O_CREAT is specified.
 *
 * Returns 0 on success, -1 on error.
 */
int  9p_handle_topen(9p_srv_t* srv, 9p_client_t* client, 9p_fcall_t* req_fc, 9p_fcall_t* resp_fc);
```

### Read (Tread / Rread)

```c
/*
 * Handle Tread: Read data from a FID.
 * Data is read from the underlying file and returned to the client.
 *
 * Returns 0 on success, -1 on error.
 */
int  9p_handle_tread(9p_srv_t* srv, 9p_client_t* client, 9p_fcall_t* req_fc, 9p_fcall_t* resp_fc);
```

### Write (Twrite / Rwrite)

```c
/*
 * Handle Twrite: Write data to a FID.
 * Data is written to the underlying file.
 * The write is committed immediately (no sync unless explicitly requested).
 *
 * Returns 0 on success, -1 on error.
 */
int  9p_handle_twrite(9p_srv_t* srv, 9p_client_t* client, 9p_fcall_t* req_fc, 9p_fcall_t* resp_fc);
```

### Stat (Tstat / Rstat)

```c
/*
 * Handle Tstat: Return the stat structure for a FID.
 * The stat structure is built from the underlying file's metadata.
 * The stat is cached in the metadata cache (if enabled).
 *
 * Returns 0 on success, -1 on error.
 */
int  9p_handle_tstat(9p_srv_t* srv, 9p_client_t* client, 9p_fcall_t* req_fc, 9p_fcall_t* resp_fc);
```

### Wstat (Twstat / Rwstat)

```c
/*
 * Handle Twstat: Modify the metadata of a FID.
 * Only certain fields can be modified (mode, mtime, etc.).
 * The modification is applied to the underlying file.
 *
 * Returns 0 on success, -1 on error.
 */
int  9p_handle_twstat(9p_srv_t* srv, 9p_client_t* client, 9p_fcall_t* req_fc, 9p_fcall_t* resp_fc);
```

### Create (Tcreate / Rcreate)

```c
/*
 * Handle Tcreate: Create a new file.
 * The file is created with the specified permissions and mode.
 *
 * Returns 0 on success, -1 on error.
 */
int  9p_handle_tcreate(9p_srv_t* srv, 9p_client_t* client, 9p_fcall_t* req_fc, 9p_fcall_t* resp_fc);
```

### Remove (Tremove / Rremove)

```c
/*
 * Handle Tremove: Remove a file or directory.
 * The file must be clunked before it can be removed.
 *
 * Returns 0 on success, -1 on error.
 */
int  9p_handle_tremove(9p_srv_t* srv, 9p_client_t* client, 9p_fcall_t* req_fc, 9p_fcall_t* resp_fc);
```

### Clunk (Tclunk / Rclunk)

```c
/*
 * Handle Tclunk: Destroy a FID.
 * The FID is deallocated and any associated resources are released.
 *
 * Returns 0 on success, -1 on error.
 */
int  9p_handle_tclunk(9p_srv_t* srv, 9p_client_t* client, 9p_fcall_t* req_fc, 9p_fcall_t* resp_fc);
```

### Flush (Tflush / Rflush)

```c
/*
 * Handle Tflush: Cancel a pending request.
 * The request with the specified oldtag is cancelled.
 *
 * Returns 0 on success, -1 on error.
 */
int  9p_handle_tflush(9p_srv_t* srv, 9p_client_t* client, 9p_fcall_t* req_fc, 9p_fcall_t* resp_fc);
```

### Mkdir (Tmkdir / Rmkdir)

```c
/*
 * Handle Tmkdir: Create a new directory.
 * The directory is created with the specified permissions.
 *
 * Returns 0 on success, -1 on error.
 */
int  9p_handle_tmkdir(9p_srv_t* srv, 9p_client_t* client, 9p_fcall_t* req_fc, 9p_fcall_t* resp_fc);
```

### Rename (Trename / Rrename)

```c
/*
 * Handle Trename: Rename a file or directory.
 * The file is moved from the source path to the destination path.
 *
 * Returns 0 on success, -1 on error.
 */
int  9p_handle_trename(9p_srv_t* srv, 9p_client_t* client, 9p_fcall_t* req_fc, 9p_fcall_t* resp_fc);
```

---

## 4. Wire Protocol — Response Encoding

The server encodes responses in the same wire format as the client (9p2000.L):

```
┌────────────┬─────────┬─────────┬──────────┐
│    Size    │  Type   │   Tag   │ Payload  │
│  (4B BE)   │ (1B)    │ (2B BE) │ Variable │
└────────────┴─────────┴─────────┴──────────┘
```

### Response Creation

```c
/*
 * Create a response fcall for a given request.
 * The response type and tag are set automatically.
 */
9p_fcall_t*  9p_fcall_response(9p_fcall_t* req_fc);

/*
 * Encode various response types:
 */

/* Rversion response */
int  9p_rversion(9p_fcall_t* resp_fc, int msize, const char* version);

/* Rauth response */
int  9p_rauth(9p_fcall_t* resp_fc, int qid_type, uint64_t qid_ver,
              const char* qid_hash);

/* Rattach response */
int  9p_rattach(9p_fcall_t* resp_fc, uint16_t qid_type, uint64_t qid_ver,
                const char* qid_hash);

/* Rwalk response */
int  9p_rwalk(9p_fcall_t* resp_fc, int nwalk, uint16_t qid_type,
              uint64_t qid_ver, const char* qid_hash);

/* Ropen response */
int  9p_ropen(9p_fcall_t* resp_fc, uint16_t iounit, uint16_t qid_type,
              uint64_t qid_ver, const char* qid_hash);

/* Rcreate response */
int  9p_rcreate(9p_fcall_t* resp_fc, uint16_t iounit, uint16_t qid_type,
                uint64_t qid_ver, const char* qid_hash);

/* Rread response */
int  9p_rread(9p_fcall_t* resp_fc, const char* data, int count);

/* Rwrite response */
int  9p_rwrite(9p_fcall_t* resp_fc, int count);

/* Rstat response */
int  9p_rstat(9p_fcall_t* resp_fc, const char* stat_data, int stat_len);

/* Rwstat response */
int  9p_rwstat(9p_fcall_t* resp_fc);

/* Rremove response */
int  9p_rremove(9p_fcall_t* resp_fc);

/* Rclunk response */
int  9p_rclunk(9p_fcall_t* resp_fc);

/* Rflush response */
int  9p_rflush(9p_fcall_t* resp_fc);

/* Rmkdir response */
int  9p_rmkdir(9p_fcall_t* resp_fc, uint16_t qid_type, uint64_t qid_ver,
               const char* qid_hash);

/* Trename response */
int  9p_trename(9p_fcall_t* resp_fc);
```

---

## 5. Local Filesystem Operations

### Stat Operations

```c
/*
 * Get file metadata from the local filesystem.
 * Populates a 9p stat structure.
 *
 * Returns 0 on success, -1 on error.
 */
typedef struct {
    uint8_t  type;
    uint32_t dev;
    uint64_t qid;
    uint32_t mode;
    uint32_t atime;
    uint32_t mtime;
    uint64_t length;
    uint32_t name[256/4];  /* UTF-8 name */
    uint32_t uid[256/4];
    uint32_t gid[256/4];
    uint32_t muid[256/4];
    uint32_t nlink;
} 9p_stat_t;

int  9p_fs_stat(const char* path, 9p_stat_t* stat_out);
int  9p_fs_lstat(const char* path, 9p_stat_t* stat_out);
int  9p_fs_fstat(int fd, 9p_stat_t* stat_out);
```

### Read Operations

```c
/*
 * Read directory entries.
 * Returns the number of entries on success, -1 on error.
 * Caller must free(entries).
 */
typedef struct {
    uint64_t   offset;  /* Offset for next readdir */
    uint32_t   qid;
    uint32_t   type;
    uint32_t   name_len;
    char       name[256];
} 9p_dirent_t;

int  9p_fs_readdir(const char* path, 9p_dirent_t** entries_out, int* count_out);
```

### Write Operations

```c
/*
 * Write data to a file.
 * Returns the number of bytes written on success, -1 on error.
 */
int  9p_fs_write(const char* path, const char* data, int offset, int count);

/*
 * Create a new file with specified permissions.
 * Returns 0 on success, -1 on error.
 */
int  9p_fs_create(const char* path, uint32_t mode);

/*
 * Remove a file or directory.
 * Returns 0 on success, -1 on error.
 */
int  9p_fs_remove(const char* path);

/*
 * Create a directory with specified permissions.
 * Returns 0 on success, -1 on error.
 */
int  9p_fs_mkdir(const char* path, uint32_t mode);

/*
 * Rename a file or directory.
 * Returns 0 on success, -1 on error.
 */
int  9p_fs_rename(const char* src, const char* dst);
```

---

## 6. FID Management

### FID Table

Each client has its own FID table (a hash map of FID → open file/dir):

```c
typedef struct 9p_fid {
    int            fid_num;
    char           path[512];
    uint8_t        mode;        /* O_RDONLY, O_WRONLY, O_RDWR */
    int            flags;       /* O_CREAT, O_EXCL, etc. */
    int            fd;          /* Underlying file descriptor */
    time_t         created_at;
} 9p_fid_t;

typedef struct 9p_fid_table {
    9p_fid_t**     fids;
    int            count;
    int            capacity;
} 9p_fid_table_t;

/* Create/destroy FID tables */
9p_fid_table_t*  9p_fid_table_new(int capacity);
void             9p_fid_table_free(9p_fid_table_t* table);

/* Allocate a new FID */
int  9p_fid_table_alloc(9p_fid_table_t* table, const char* path, uint8_t mode);

/* Get an existing FID */
9p_fid_t*  9p_fid_table_get(9p_fid_table_t* table, int fid_num);

/* Destroy a FID */
int  9p_fid_table_free(9p_fid_table_t* table, int fid_num);
```

### FID Lifecycle

1. **Tattach:** Allocate a FID for the root directory (or bind mount)
2. **Twalk:** Walk the path from the FID, returning new FIDs
3. **Topen:** Open a FID for reading/writing
4. **Tread/Twrite:** Operate on an open FID
5. **Tclunk:** Destroy a FID

---

## 7. Directory Tree Management

### Bind Mounts

The server supports bind mounts (exposing different local directories at different 9p paths):

```c
typedef struct 9p_bind_mount {
    char           path[256];       /* 9p path (e.g., "/build") */
    char           local_path[512]; /* Local directory path */
    time_t         last_scan;       /* Last time the directory was scanned */
} 9p_bind_mount_t;

/*
 * Add a bind mount.
 * Returns 0 on success, -1 on error.
 */
int  9p_bind_add(9p_bind_mount_t** mounts, int* count, const char* path,
                 const char* local_path);

/*
 * Remove a bind mount.
 * Returns 0 on success, -1 on error.
 */
int  9p_bind_remove(9p_bind_mount_t** mounts, int* count, const char* path);

/*
 * Resolve a 9p path to a local path.
 * Returns the local path on success, NULL on error.
 */
const char*  9p_bind_resolve(9p_bind_mount_t* mounts, int count, const char* path);
```

### Directory Scanning

The server periodically rescans bind mounts to detect changes:

```c
/*
 * Rescan a directory and update the inode cache.
 * Returns 0 on success, -1 on error.
 */
int  9p_scan_dir(const char* local_path, 9p_stat_t** entries_out, int* count_out);
```

---

## 8. Metadata Cache

### Stat Cache

The server caches stat results for readdir operations to avoid frequent filesystem syscalls:

```c
typedef struct 9p_stat_cache {
    char       path[512];
    9p_stat_t  stat;
    time_t     cached_at;
    time_t     expires_at;
} 9p_stat_cache_t;

/*
 * Get a cached stat result, or fetch from filesystem.
 * Returns 0 on success, -1 on error.
 */
int  9p_stat_cache_get(9p_stat_cache_t* cache, int cache_size,
                       const char* path, 9p_stat_t* stat_out);

/*
 * Invalidate a cache entry.
 * Returns 0 on success, -1 on error.
 */
int  9p_stat_cache_invalidate(9p_stat_cache_t* cache, int cache_size,
                              const char* path);
```

### Cache Invalidation

When a file is modified, the cache is invalidated:

```c
/*
 * Invalidate cache entries affected by a write.
 * Returns 0 on success, -1 on error.
 */
int  9p_stat_cache_invalidate_on_write(const char* path);
```

---

## 9. Access Control (ACL)

### Permission Checking

The server checks file permissions before allowing access:

```c
/*
 * Check if a user can access a path with given permissions.
 * Returns 1 if access is allowed, 0 if not.
 * 
 * perm values:
 *   0x01 — Execute
 *   0x02 — Write
 *   0x04 — Read
 *   0x07 — Read/Write/Execute
 */
int  9p_acl_check(const char* user, const char* path, uint8_t perm);
```

### ACL Rules

ACL rules are enforced at the FID level:

```c
typedef struct 9p_acl_rule {
    char           user[256];
    char           path[512];
    uint8_t        perm;
    int            is_regex;  /* If true, path is a regex */
} 9p_acl_rule_t;

/*
 * Add an ACL rule.
 * Returns 0 on success, -1 on error.
 */
int  9p_acl_add_rule(9p_acl_rule_t* rules, int* count, const char* user,
                     const char* path, uint8_t perm);
```

---

## 10. Request Parsing and Dispatch

### Message Parsing

```c
/*
 * Parse an incoming 9p message from a client.
 * Returns the fcall on success, NULL on error.
 */
9p_fcall_t*  9p_request_parse(9p_rbuf_t* rbuf);

/*
 * Get the message type from a parsed fcall.
 */
uint8_t  9p_fcall_type(9p_fcall_t* fc);
```

### Request Dispatch

```c
/*
 * Dispatch a request to the appropriate handler.
 * Returns 0 on success, -1 on error.
 */
int  9p_dispatch_request(9p_srv_t* srv, 9p_client_t* client, 9p_fcall_t* req_fc);
```

### Handler Registration

```c
/*
 * Register a handler for a specific message type.
 * The handler function takes (srv, client, req_fc, resp_fc) and returns 0 on success.
 */
typedef int (*9p_handler_fn)(9p_srv_t* srv, 9p_client_t* client,
                              9p_fcall_t* req_fc, 9p_fcall_t* resp_fc);

void  9p_register_handler(uint8_t type, 9p_handler_fn handler);
```

---

## 11. Connection Handling

### TCP Server

```c
/*
 * Create a TCP server socket.
 * Returns the file descriptor on success, -1 on error.
 */
int  9p_tcp_server_create(const char* host, int port);

/*
 * Accept a TCP connection.
 * Returns the file descriptor on success, -1 on error.
 */
int  9p_tcp_accept(int srv_fd);
```

### Unix Domain Socket Server

```c
/*
 * Create a Unix domain socket.
 * Returns the file descriptor on success, -1 on error.
 */
int  9p_unix_server_create(const char* path);

/*
 * Accept a Unix socket connection.
 * Returns the file descriptor on success, -1 on error.
 */
int  9p_unix_accept(int srv_fd);
```

### Event Loop Integration

```c
/*
 * Run the server event loop (select/poll/epoll based).
 * This is a blocking call. Call 9p_srv_stop() to break out.
 */
void  9p_srv_run(9p_srv_t* srv);

/*
 * Process pending connections (non-blocking).
 * Returns the number of events processed.
 */
int  9p_srv_poll(9p_srv_t* srv);
```

---

## 12. Integration with Shmoo

### Syscall Interceptor

The 9p server provides the remote filesystem that the syscall interceptor uses:
- When `open()`, `stat()`, `read()` are intercepted, the interceptor checks if the path is on a 9p mount
- If so, the interceptor uses the 9p client library to forward the request to the 9p server
- If not, the call falls through to the real libc function

### Delusion Shell

The delshell mounts the build environment via the 9p server:
- The server exposes the build tree, tools, and inputs
- The engineer's file operations inside the delshell are transparently forwarded to the 9p server

### Build Actions

Each build action mounts the build environment via the 9p server:
- The server provides the build tree, tools, and inputs
- Actions read sources and write outputs through the 9p server

### Content-Addressed Cache

Cached build outputs are stored on the 9p server:
- The server exposes the cache directory
- The client retrieves cached results for fast replay

---

## 13. Implementation Plan

### Phase 1: Core Server (Week 1)
- [ ] Implement buffer management (`9psrv_buf.c`)
- [ ] Implement wire encoding for responses (`9psrv_wire.c`)
- [ ] Implement fcall structure and response creation (`9psrv_fcall.c`)
- [ ] Write unit tests for wire encoding

### Phase 2: Connection Handling (Week 2)
- [ ] Implement TCP server (`9psrv_tcp.c`)
- [ ] Implement Unix socket server (`9psrv_unix.c`)
- [ ] Implement connection management (`9psrv_session.c`)
- [ ] Write integration tests (TCP and Unix socket connections)

### Phase 3: Request Handlers (Week 3)
- [ ] Implement version exchange handler (`9p_handle_tversion`)
- [ ] Implement attach handler (`9p_handle_tattach`)
- [ ] Implement walk handler (`9p_handle_twalk`)
- [ ] Implement open/read/write handlers (`9p_handle_topen`, `9p_handle_tread`, `9p_handle_twrite`)
- [ ] Implement stat handlers (`9p_handle_tstat`, `9p_handle_twstat`)

### Phase 4: Filesystem Operations (Week 4)
- [ ] Implement local filesystem operations (`9psrv_fs.c`)
- [ ] Implement directory tree management (`9psrv_dir.c`)
- [ ] Implement bind mounts
- [ ] Implement FID management (`9psrv_session.c`)

### Phase 5: ACL and Caching (Week 5)
- [ ] Implement access control (`9psrv_acl.c`)
- [ ] Implement metadata cache (`9psrv_cache.c`)
- [ ] Implement cache invalidation
- [ ] Write end-to-end integration tests

### Phase 6: Testing & Hardening (Week 6)
- [ ] Stress test: concurrent clients, large files
- [ ] Test error handling (broken pipes, permission denied, etc.)
- [ ] Test with real 9p clients (Go9pClient, 9ping)
- [ ] Benchmark: latency, throughput

---

## 14. Performance Targets

| Metric | Target |
|--------|--------|
| Single operation latency (local) | < 50µs |
| Single operation latency (TCP, LAN) | < 500µs |
| Throughput (sequential read, 4KB blocks) | > 200MB/s |
| Throughput (sequential write, 4KB blocks) | > 100MB/s |
| Memory overhead (per client) | < 512KB |
| Wire protocol overhead (per message) | 8 bytes (header) + string lengths |

---

## 15. Summary

The 9p server library is a lightweight, zero-dependency C implementation of a 9p2000.L server that exposes a local directory tree as a 9p endpoint. It provides:

- **Full 9p2000.L support:** version, auth, attach, walk, open, read, write, stat, create, remove, clunk, flush, mkdir, rename
- **Simple API:** Create server, bind mounts, start listening — three operations
- **Transparent filesystem:** Local directory tree exposed as 9p
- **No external dependencies:** Pure C99, POSIX headers only
- **ACL support:** Optional permission filtering
- **Metadata caching:** Stat cache for readdir performance

This library is the foundation for transparent remote filesystem access in the Shmoo build system, enabling the 9p client library to connect and provide filesystem operations to build actions and the delshell.
