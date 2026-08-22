# Design: Client-Side Artifact Streaming and Cleanup Spectrum

*2025-01-23 | Status: Active Design*
*Purpose: Build clients stream artifacts and logs to a persistent network share as they execute, rather than accumulating loose files on the build node. A configurable cleanup spectrum controls how aggressively intermediates are deleted, from "ferret out silent dependencies" to "keep absolutely everything."*

---

## 1. The Problem

### Current State

When a build action (compile, link, etc.) executes on a build node:

```
Build Node (e.g., alpha.local):
├── build-tree/
│   ├── src/
│   ├── build/
│   │   ├── main.o
│   │   ├── util.o
│   │   ├── app.o
│   │   ├── libfoo.a
│   │   ├── app        ← final binary
│   │   └── ...        ← everything piles up
│   └── logs/
│       ├── build.log
│       └── audit.log
```

**Problems:**
- Loose files accumulate on the build node indefinitely
- If a node goes away (crash, migration, VM shutdown), artifacts may be lost
- The build node's filesystem state is fragile — it's not the authoritative record
- Intermediates (`.o` files) waste space
- Silent dependencies (code that compiles fine but depends on an intermediate that was accidentally deleted) go undetected

### Desired State

```
Build Node (e.g., alpha.local):
├── src/                           ← source tree (persistent)
├── build/                         ← working directory (ephemeral)
│   ├── main.o → streamed → deleted
│   ├── util.o → streamed → deleted
│   └── ...
└── [nothing else — everything cleaned]

Network Share (persistent, authoritative):
├── artifacts/
│   ├── sha256_main.o              ← archived object files
│   ├── sha256_app                 ← final binaries
│   └── sha256_libfoo.a            ← static libraries
├── logs/
│   ├── action-1001.tar            ← full build log tarball
│   ├── action-1002.tar            ← for every action
│   └── action-1003.tar
└── events/
    ├── action-1001.json           ← structured event log
    ├── action-1002.json
    └── action-1003.json
```

**Benefits:**
- Build artifacts are persisted on a **persistent** network share, not ephemeral build nodes
- Build nodes are cleaned aggressively — they can be migrated, replaced, or shut down without losing anything
- Interdependencies are exposed by aggressive cleanup
- Space on build nodes is minimized
- The network share is the single source of truth

---

## 2. Architecture Overview

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                              Network Share (NFS / SMB / 9p)                       │
│                                                                                  │
│  ┌────────────────────────┐  ┌────────────────────────┐  ┌────────────────────┐ │
│  │  artifacts/            │  │  logs/                 │  │  events/           │ │
│  │  ┌──────────────────┐  │  │  ┌──────────────────┐  │  │  ┌──────────────┐  │ │
│  │  │ sha256_main.o    │  │  │  │ action-1001.tar  │  │  │  │ action-1001  │  │ │
│  │  │ sha256_app       │  │  │  │ action-1002.tar  │  │  │  │ action-1002  │  │ │
│  │  │ sha256_libfoo.a  │  │  │  │ action-1003.tar  │  │  │  │ action-1003  │  │ │
│  │  └──────────────────┘  │  │  └──────────────────┘  │  │  └──────────────┘  │ │
│  └────────────────────────┘  └────────────────────────┘  └────────────────────┘ │
│                                                                                  │
├──────────────────────────────────────────────────────────────────────────────────┤
│  Build Node (alpha.local)                                                     │
│                                                                                  │
│  ┌─────────────────────────────────────────────────────────────────────┐        │
│  │  Host Daemon (shd-alpha)                                            │        │
│  │                                                                     │        │
│  │  ┌──────────────────┐  ┌──────────────────┐  ┌─────────────────┐  │        │
│  │  │ Job A            │  │ Job B            │  │ Job C           │  │        │
│  │  │ (compile:main.c) │  │ (compile:util.c) │  │ (link:app)      │  │        │
│  │  └───────┬──────────┘  └───────┬──────────┘  └───────┬─────────┘  │        │
│  │          │                      │                      │            │        │
│  │          ▼                      ▼                      ▼            │        │
│  │  ┌─────────────────────────────────────────────────────────────┐     │        │
│  │  │  Artifact Pipeline                                          │     │        │
│  │  │                                                             │     │        │
│  │  │  1. Execute action (gcc, make, perl, etc.)                │     │        │
│  │  │  2. Stream outputs to network share (archive + hash)      │     │        │
│  │  │  3. Write structured event log + audit to network share   │     │        │
│  │  │  4. Apply cleanup policy (see §4)                         │     │        │
│  │  │  5. Free node resources                                   │     │        │
│  │  └─────────────────────────────────────────────────────────────┘     │        │
│  └─────────────────────────────────────────────────────────────────────┘        │
└──────────────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Artifact Streaming

### Streaming Model

Instead of leaving build outputs on the local node, the client streams them to the network share as they are produced:

```perl
# During action execution:
#
# 1. gcc -O2 -c main.c -o /build/main.o
#    → Artifact streamer detects file creation at /build/main.o
#    → Streams /build/main.o to network share as:
#      /artifacts/sha256(main.o content)
#    → Updates artifact index: sha256 → /build/main.o (ephemeral)
#    → /build/main.o can be deleted immediately after streaming
#
# 2. ar rcs /build/libfoo.a /build/main.o /build/util.o
#    → Artifact streamer detects file creation at /build/libfoo.a
#    → Streams /build/libfoo.a to network share as:
#      /artifacts/sha256(libfoo.a content)
#    → Updates artifact index: sha256 → /build/libfoo.a (ephemeral)
#    → /build/main.o and /build/util.o can be deleted (they were already streamed)
#
# 3. gcc -o /build/app /build/app.o /build/libfoo.a
#    → Artifact streamer detects file creation at /build/app
#    → Streams /build/app to network share as:
#      /artifacts/sha256(app content)
#    → Updates artifact index: sha256 → /build/app (ephemeral)
#    → /build/app.o can be deleted (dependency of link:app)
#
# After completion:
# 4. Apply cleanup policy
# 5. Write event log + audit log to network share
# 6. Release node resources
```

### Streaming Implementation

```perl
package Shmoo::Artifact::Streamer;

use File::Slurp qw(read_file write_file);
use Digest::SHA qw(sha256_hex sha256);
use IO::Socket::UNIX;  # For local socket to NFS mount

/*
 * Stream an artifact to the network share.
 *
 * Returns the artifact ID (SHA256 of content) on success.
 * Returns undef on failure.
 *
 * The artifact is stored at:
 *   /artifacts/<sha256_of_content>
 */
sub stream_artifact {
    my ($self, $file_path) = @_;
    
    # Read the file content
    my $content = read_file($file_path);
    my $sha256 = sha256_hex($content);
    my $size   = length($content);
    
    # Write to network share
    my $dest = "/artifacts/$sha256";
    
    # Write atomically (write to tmp, then rename)
    my $tmp = "$dest.tmp.$$";
    write_file($tmp, { binmode => ':raw' }, $content);
    rename($tmp, $dest) or die "Failed to move $tmp → $dest: $!";
    
    # Update artifact index
    $self->_update_artifact_index($sha256, $file_path, $size);
    
    # Update event log
    $self->_log_event("artifact_stream", {
        sha256  => $sha256,
        size    => $size,
        path    => $file_path,
        dest    => $dest,
    });
    
    return $sha256;
}

/*
 * Stream multiple artifacts (batch operation).
 * Returns a hash of sha256 → file_path.
 */
sub stream_batch {
    my ($self, @file_paths) = @_;
    
    my %results;
    
    for my $path (@file_paths) {
        $results{$self->stream_artifact($path)} = $path;
    }
    
    return %results;
}
```

### Streaming via Socket (Avoids Local Copy)

For large artifacts, streaming avoids writing a full local copy by using **write-through mode**:

```
gcc -O2 -c main.c -o main.o
  │
  └─ syscall hook intercepts `open()` and `write()`
     │
     ├─ When gcc writes to main.o:
     │   1. Write data to local tmp file
     │   2. Stream data over socket to network share (streaming write)
     │   3. On close, compute SHA256 and finalize
     │
     └─ No full local copy needed — data flows through the syscall hook
```

This is more complex but avoids the "copy + archive" step for large binaries.

### Streaming via Tarball / CPIO Archive

For bulk operations (e.g., after a full build phase), stream a **tarball or CPIO archive** of all outputs:

```bash
# Stream a tarball of all build outputs
tar -cf - /build/*.o /build/*.a | ssh network-share "cat > /artifacts/build-phase1.tar"

# Stream a CPIO archive (preserves metadata like permissions, ownership)
find /build -type f | cpio -o | ssh network-share "cat > /artifacts/build-phase1.cpio"
```

This is more efficient than streaming individual files when there are many outputs.

### Artifact Index

The artifact index on the network share maps content hashes to local paths:

```json
// /artifacts/index.json
{
  "sha256:abc123...": {
    "path": "/build/main.o",
    "size": 4096,
    "streamed_at": "2025-01-23T12:00:00Z",
    "status": "streamed"
  },
  "sha256:def456...": {
    "path": "/build/libfoo.a",
    "size": 10240,
    "streamed_at": "2025-01-23T12:01:00Z",
    "status": "streamed"
  },
  "sha256:ghi789...": {
    "path": "/build/app",
    "size": 524288,
    "streamed_at": "2025-01-23T12:02:00Z",
    "status": "streamed"
  }
}
```

When a node goes away, the index allows the system to reconstruct which artifacts were streamed and which were lost (if streaming was interrupted).

---

## 4. Event Log + Audit Logging

### Structured Event Log

Each build action writes a **structured event log** (JSON) to the network share:

```json
// /events/action-1001.json
{
  "action_id": "cc_compile:main.c",
  "eid_start": 1001000,
  "eid_end": 1001050,
  "host": "alpha.local",
  "start_time": "2025-01-23T12:00:00Z",
  "end_time": "2025-01-23T12:00:05Z",
  "exit_code": 0,
  "cmd": "gcc -O2 -g -c /inputs/src/main.c -o /build/main.o",
  "stdout": "...\n",
  "stderr": "...\n",
  "env": {
    "CFLAGS": "-O2 -g",
    "PATH": "/tools/gcc/bin:/usr/bin"
  },
  "inputs": [
    { "path": "/inputs/src/main.c", "sha256": "sha256:abc123..." }
  ],
  "outputs": [
    { "path": "/build/main.o", "sha256": "sha256:def456...", "size": 4096 }
  ],
  "artifacts_streamed": [
    "sha256:def456..."
  ],
  "dependencies_used": [
    "/inputs/src/main.c",
    "/inputs/include/main.h"
  ]
}
```

### Build Log Tarball

Each action also produces a **tarball** of the build log, containing:
- All event records (from the binary event log)
- The structured event log (JSON)
- Build stdout/stderr
- Compiler flags and environment

```bash
# After action completion:
tar -czf /events/action-1001.tar \
  /events/action-1001.json \
  /build/logs/action-1001.binlog \
  /build/logs/action-1001.stdout \
  /build/logs/action-1001.stderr
```

The tarball is a **complete reconstruction package** — if you load the tarball, you can reconstruct the entire build environment state at any EID within the range.

### Audit Plugin Integration

The **audit plugin** snoops on the build pipeline and adds additional metadata:

```json
// /events/action-1001-audit.json (audit plugin output)
{
  "action_id": "cc_compile:main.c",
  "files_accessed": [
    { "path": "/inputs/src/main.c", "type": "read" },
    { "path": "/inputs/include/main.h", "type": "read" },
    { "path": "/build/main.o", "type": "write" },
    { "path": "/tmp/gcc-13.2.0/lib/gcc/x86_64-pc-linux-gnu/13.2.0/include/stddef.h", "type": "read" },
    { "path": "/tmp/gcc-13.2.0/lib/gcc/x86_64-pc-linux-gnu/13.2.0/include/stdarg.h", "type": "read" }
  ],
  "env_vars_read": [
    "CFLAGS", "PATH", "HOME", "PWD", "SHELL", "LANG",
    "TMPDIR", "GCC_EXEC_PREFIX", "COMPILER_PATH"
  ],
  "syscalls": {
    "open": 42,
    "read": 156,
    "write": 23,
    "stat": 18,
    "execve": 1
  },
  "files_created": [
    "/build/main.o"
  ],
  "files_deleted": [],
  "network_connections": [],
  "silent_dependencies": [
    "/tmp/gcc-13.2.0/lib/gcc/x86_64-pc-linux-gnu/13.2.0/include/stddef.h",
    "/tmp/gcc-13.2.0/lib/gcc/x86_64-pc-linux-gnu/13.2.0/include/stdarg.h"
  ],
  "warnings": [
    "File accessed outside declared inputs: stddef.h",
    "File accessed outside declared inputs: stdarg.h"
  ]
}
```

The audit plugin tracks:
- **Files accessed** (reads and writes)
- **Environment variables read** (getenv calls)
- **Syscall counts** (for profiling)
- **Silent dependencies** (files accessed that weren't declared as inputs)
- **Warnings** (suspicious access patterns)

**Silent dependencies** are particularly important — they are files that the build action reads but were not declared as inputs. If those files are on the network share and archived, the build remains reproducible. If they are on the local node and deleted, the build would fail if the node went away. This is how aggressive cleanup **ferrets out silent dependencies**.

---

## 5. The Cleanup Spectrum

The core design space. A spectrum from "aggressive cleanup" to "keep absolutely everything."

### The Spectrum

```
                    CLEANUP POLICY
│
│  CLEANING-EXTREME        CLEANING-STRICT        CLEANING-MINIMAL        CLEANING-EVERYTHING
│  (Ferret out)           (Production)           (Debug)                (Developer)
│  ████████████████████████░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
│
│  Delete: Delete intermediates Delete only: Delete: Keep: Keep:
│  (e.g., .o after .a Delete: Keep Delete: Delete:
│   is built) all artifacts Keep all Keep all
│   Keep only: outputs outputs
│   Keep final Keep outputs,
│   outputs,
│   outputs, no
│   outputs,
│   no intermediates
│   No silent deps
│   recorded
```

### Policy Levels

#### Level 0: CLEANING-EXTREME — "Ferret Out Silent Dependencies"

Maximum cleanup. Delete everything that can be deleted. Force the build to declare every dependency explicitly.

| File Type | Action | Reason |
|-----------|--------|--------|
| `.o` files (object files) | **Delete** after static lib is built | Only needed for lib creation |
| `.i` files (preprocessed) | **Always delete** | Never needed by anyone |
| `.s` files (assembly) | **Always delete** | Never needed by anyone |
| Intermediate `.o` files | **Delete** after they are linked | Silent dependency detection |
| All artifacts | **Streamed** to network share before deletion | Nothing is lost |
| Event logs | **Archived** to network share | Full reconstruction possible |
| Audit data | **Archived** to network share | Silent dependencies recorded |
| Source tree | **Keep** | Needed for future builds |

**Result:** After a build completes with CLEANING-EXTREME:
- Build node has ONLY the source tree and the build script
- Everything else (artifacts, logs, audit data) is on the network share
- If a file was deleted but was needed elsewhere, the build will fail next time → **silent dependency exposed**
- The audit plugin records ALL files accessed, so even silent dependencies are archived on the network share

**Use when:** You want maximum build integrity, are debugging dependency issues, or need to verify that a build is truly hermetic.

#### Level 1: CLEANING-STRICT — "Production Build"

Delete intermediates after they've served their purpose. Keep all outputs.

| File Type | Action | Reason |
|-----------|--------|--------|
| `.o` files | **Delete** after static lib is built | Not needed after lib creation |
| `.o` files (final outputs) | **Keep** on node for debugging | Can be used for incremental builds |
| `.i` files | **Always delete** | Never needed |
| `.s` files | **Always delete** | Never needed |
| All artifacts | **Streamed** to network share | Nothing is lost |
| Event logs | **Archived** to network share | Full reconstruction possible |
| Audit data | **Archived** (but NOT silent dependencies) | Performance-focused audit |
| Source tree | **Keep** | Needed for future builds |

**Result:** After a build with CLEANING-STRICT:
- Build node has sources + final outputs (`.o`, `.a`, binaries)
- Everything else is on the network share
- Silent dependencies are NOT recorded in audit data → faster build but less visibility
- If a dependency was silent, the build node can still recover from it (because the `.o` file is still there)

**Use when:** This is the default for production builds. Balances integrity and performance.

#### Level 2: CLEANING-MINIMAL — "Debug Build"

Keep all outputs, minimal cleanup. Useful for debugging.

| File Type | Action | Reason |
|-----------|--------|--------|
| `.o` files | **Keep** | Needed for debugging |
| `.i` files | **Keep** | Useful for debugging |
| `.s` files | **Keep** | Useful for debugging |
| All artifacts | **Streamed** to network share | Nothing is lost |
| Event logs | **Archived** to network share | Full reconstruction possible |
| Audit data | **Full audit** (all files, all env vars, all syscalls) | Maximum visibility |
| Source tree | **Keep** | Needed for future builds |

**Result:** After a build with CLEANING-MINIMAL:
- Build node has everything
- Slow cleanup, but full visibility
- Excellent for debugging build failures

**Use when:** You're debugging a build failure and need full visibility into what the compiler did.

#### Level 3: CLEANING-EVERYTHING — "Developer Mode"

Keep absolutely everything, including copies of pipeline data snooped by the audit plugin.

| File Type | Action | Reason |
|-----------|--------|--------|
| Everything | **Keep** | Maximum reproducibility |
| All artifacts | **Streamed** to network share | Nothing is lost |
| Event logs | **Archived** to network share | Full reconstruction possible |
| Audit data | **Full audit with everything** | Full visibility |
| Source tree | **Keep** | Needed for future builds |
| Pipeline data | **Keep** (copies of all compiler outputs, intermediate files, etc.) | Maximum reproducibility |
| Compiler temp files | **Keep** | In case something goes wrong |

**Result:** After a build with CLEANING-EVERYTHING:
- Build node has everything — sources, intermediates, outputs, logs, audit data, compiler temps
- Maximum reproducibility — you can rebuild from any point
- Maximum space usage on build node
- Maximum visibility — every file, every variable, every syscall is recorded

**Use when:** You need maximum debuggability, are reproducing a reported build failure, or are a developer who wants to poke around the build state.

---

## 6. Cleanup Implementation

### Cleanup Policy Configuration

```yaml
build:
  cleanup:
    policy: "cleaning-strict"     # cleaning-extreme | cleaning-strict | cleaning-minimal | cleaning-everything
    delete_intermediates: true     # Delete .o after .a is built
    delete_preprocessed: true      # Always delete .i files
    delete_assembly: true          # Always delete .s files
    stream_to_share: true          # Stream artifacts to network share before delete
    archive_logs: true             # Archive event logs + audit to network share
    silent_deps: "archive"         # What to do with silent dependencies:
                                   #   "delete" — don't archive, force declaration
                                   #   "archive" — archive but don't error
                                   #   "error" — error on silent deps
```

### Cleanup Process

```perl
package Shmoo::Artifact::Cleaner;

use Shmoo::Artifact::Streamer;
use Shmoo::Audit::Plugin;

/*
 * Run the cleanup process after a build action completes.
 *
 * 1. Stream all outputs to network share
 * 2. Apply cleanup policy (delete intermediates, etc.)
 * 3. Write event log + audit log to network share
 * 4. Clean node (delete unneeded files)
 */
sub run_cleanup {
    my ($self, $action) = @_;
    
    # Step 1: Stream all outputs to network share
    my @outputs = $self->_find_outputs($action);
    my $streamer = Shmoo::Artifact::Streamer->new;
    for my $output (@outputs) {
        $streamer->stream_artifact($output->path);
    }
    
    # Step 2: Get cleanup policy
    my $policy = $action->cleanup_policy || 'cleaning-strict';
    
    # Step 3: Apply cleanup policy
    if ($policy =~ /extreme/) {
        # Delete ALL intermediates except sources
        $self->_delete_intermediates();
    } elsif ($policy =~ /strict/) {
        # Delete .o files that were used to build .a files
        $self->_delete_obsolete_objects();
    } elsif ($policy =~ /minimal/) {
        # Minimal cleanup — keep most things
        $self->_delete_preprocessed_and_assembly();
    } else {
        # CLEANING-EVERYTHING — nothing to delete
        return;
    }
    
    # Step 4: Write event log + audit log
    $self->_archive_event_log($action);
    $self->_archive_audit_log($action);
    
    # Step 5: Check for silent dependencies (extreme policy)
    if ($policy =~ /extreme/ && $self->_has_undeclared_accesses($action)) {
        warn "CLEANING-EXTREME: Silent dependencies detected!\n" .
             "  The build accessed files that were not declared as inputs.\n" .
             "  These files have been archived on the network share.\n" .
             "  To fix, add these files to the action's input list.";
    }
}
```

### Find Outputs

The cleaner finds all outputs by examining the action's declared outputs and the actual files created:

```perl
sub _find_outputs {
    my ($self, $action) = @_;
    
    my @outputs;
    
    # Added to the declared outputs by the action
    for my $declared (@{$action->outputs}) {
        if (-f $declared->path) {
            push @outputs, $declared;
        }
    }
    
    # Added by the audit plugin (silent dependencies that were actually used)
    if ($action->cleanup_policy eq 'cleaning-extreme') {
        my $audit = $action->audit_data;
        for my $dep (@{$audit->{silent_dependencies}}) {
            if (-f $dep) {
                push @outputs, { path => $dep, sha256 => sha256_hex(read_file($dep)) };
            }
        }
    }
    
    return @outputs;
}
```

### Delete Intermediates

The cleanup policy controls what gets deleted:

```perl
sub _delete_intermediates {
    my ($self) = @_;
    
    my @files = glob("/build/*");
    
    for my $file (@files) {
        next unless -f $file;
        
        my $ext = $self->_file_ext($file);
        
        # Always delete preprocessed and assembly files
        if ($ext eq 'i' || $ext eq 's') {
            unlink $file;
            $self->_log_delete($file, "cleanup");
            next;
        }
        
        # Delete .o files that were used to build .a files
        if ($ext eq 'o') {
            my $base = $self->_file_base($file);  # "main.o" → "main"
            
            # Check if a .a file was built from this .o file
            for my $lib (glob("/build/*.a")) {
                my $lib_base = $self->_file_base($lib);  # "libfoo.a" → "libfoo"
                
                # If the .a file was created AFTER the .o file was streamed,
                # it means the .o was used to build the .a → safe to delete
                if ($self->_is_obsolete_object($file, $lib)) {
                    unlink $file;
                    $self->_log_delete($file, "cleanup");
                    last;  # Found the .a that used this .o, stop looking
                }
            }
        }
    }
}

sub _is_obsolete_object {
    my ($self, $o_file, $a_file) = @_;
    
    # Check if the .a file's creation time is after the .o file was streamed
    my $o_streamed_at = $self->_get_stream_time($o_file);
    my $a_created_at  = (stat($a_file))[9];
    
    # If .a was created after .o was streamed, .o is obsolete
    return $a_created_at >= $o_streamed_at;
}
```

### Archive Event Logs

```perl
sub _archive_event_log {
    my ($self, $action) = @_;
    
    my $eid_range = "$action->eid_start-$action->eid_end";
    my $tarball   = "/events/action-${eid_range}.tar";
    
    # Create tarball with event log, structured log, stdout, stderr
    system("tar", "-czf", $tarball,
           "/events/action-${eid_range}.json",
           "/build/logs/action-${action->eid_start}.binlog",
           "/build/logs/action-${action->eid_start}.stdout",
           "/build/logs/action-${action->eid_start}.stderr");
    
    # Move tarball to network share
    system("mv", $tarball, "/build/logs/$tarball");
    system("rsync", "/build/logs/$tarball", "/network-share/$tarball");
}
```

---

## 7. Node Lifecycle

### Node Takeaway

When a node has completed all its actions and been cleaned, it can be "taken away" (migrated, shut down, replaced):

```perl
package Shmoo::Host::Lifecycle;

/*
 * Take a node away after cleanup.
 *
 * 1. Verify all artifacts were streamed
 * 2. Verify all logs were archived
 * 3. Verify all audit data was archived
 * 4. Unmount the build tree
 * 5. Release the node
 */
sub take_away {
    my ($self, $node) = @_;
    
    # 1. Verify artifact streaming
    my $artifacts = $self->_get_artifact_index($node);
    for my $sha256 (keys %$artifacts) {
        if ($artifacts->{$sha256}{status} ne 'streamed') {
            warn "Node $node has unstreamed artifact: $sha256";
            return undef;  # Don't take away
        }
    }
    
    # 2. Verify log archiving
    my $logs = $self->_get_event_logs($node);
    for my $action_id (keys %$logs) {
        if (!-f "/network-share/events/$action_id.tar") {
            warn "Node $node has unarchived log: $action_id";
            return undef;  # Don't take away
        }
    }
    
    # 3. Verify audit archiving
    my $audits = $self->_get_audit_logs($node);
    for my $action_id (keys %$audits) {
        if (!-f "/network-share/events/$action_id-audit.json") {
            warn "Node $node has unarchived audit: $action_id";
            return undef;  # Don't take away
        }
    }
    
    # 4. Unmount build tree
    system("umount", "/build");
    
    # 5. Release the node
    $node->release();
    
    return 1;
}
```

---

## 8. Network Share Integration

### Network Share Configuration

```yaml
network_share:
  type: "nfs"        # nfs | smb | 9p | http
  host: "artifact-server.local"
  path: "/shmoo/artifacts"
  mount_point: "/network-share"
  sync_interval: 60  # Sync to share every 60 seconds
```

### Network Share Types

| Type | Use Case | Performance | Reliability |
|------|----------|-------------|-------------|
| **NFS** | Linux-to-Linux, shared artifact store | High | Medium (can fail) |
| **SMB** | Windows-to-Linux, shared artifact store | Medium | High (persistent) |
| **9p** | Shmoo-native, integrates with build system | High | High (9p-aware) |
| **HTTP** | Cloud artifact store (S3-compatible) | Medium | Very High (distributed) |

### 9p Integration

Since Shmoo already has a 9p client library, using 9p for the network share is ideal:

```bash
# Mount the artifact store via 9p
mount -t 9p -o trans=tcp,version=9p2000.L artifact-server:5640 /network-share

# Stream artifacts to network share via 9p client
shbuild stream --share /network-share --action cc_compile:main.c
```

The 9p client writes artifacts to the network share's 9p server:

```perl
package Shmoo::Artifact::Streamer;

sub stream_artifact_via_9p {
    my ($self, $file_path, $share_path) = @_;
    
    # Use the 9p client to write the file to the network share
    my $conn = 9p_connect("tcp:artifact-server:5640", 9P_CONN_TCP);
    9p_mount($conn);
    
    my $sha256 = sha256_hex(read_file($file_path));
    my $dest   = "$share_path/artifacts/$sha256";
    
    9p_write_file($conn, $dest, read_file($file_path));
    
    9p_close($conn);
    
    return $sha256;
}
```

---

## 9. Silent Dependency Detection

### How Cleanup Exposes Silent Dependencies

When the cleanup policy is CLEANING-EXTREME, object files are deleted as soon as they are no longer needed. If another build action later tries to use a deleted `.o` file that wasn't declared as an input, the build will fail:

```
Action A: cc_compile:libutil.a → creates /build/util.o → streamed → deleted
Action B: cc_compile:libfoo.a → depends on /build/util.o (undeclared input)

Before cleanup:
  /build/util.o exists → Action B works ✓

After cleanup (CLEANING-EXTREME):
  /build/util.o deleted → Action B fails ✗

The build fails → silent dependency exposed → engineer declares /build/util.o as an input → fix ✓
```

The audit plugin helps by recording ALL files accessed, so even silent dependencies are archived on the network share:

```json
// /events/action-B-audit.json
{
  "silent_dependencies": [
    "/build/util.o",
    "/tmp/gcc-13.2.0/include/stddef.h"
  ],
  "warnings": [
    "File accessed outside declared inputs: /build/util.o",
    "File accessed outside declared inputs: /tmp/gcc-13.2.0/include/stddef.h"
  ]
}
```

---

## 10. Implementation Plan

### Phase 1: Artifact Streaming
- [ ] Implement `Shmoo::Artifact::Streamer` (stream to network share)
- [ ] Implement `stream_artifact()` (file → network share)
- [ ] Implement `stream_batch()` (batch operation)
- [ ] Implement write-through mode (stream during build, no local copy)
- [ ] Test with NFS and 9p mounts

### Phase 2: Artifact Index
- [ ] Implement artifact index (`/artifacts/index.json`)
- [ ] Implement index updates on stream
- [ ] Implement index verification (check all artifacts were streamed)
- [ ] Test index consistency after node crash

### Phase 3: Cleanup Policy
- [ ] Implement `Shmoo::Artifact::Cleaner`
- [ ] Implement policy levels (extreme, strict, minimal, everything)
- [ ] Implement `_delete_obsolete_objects()`
- [ ] Implement `_archive_event_log()`
- [ ] Implement silent dependency detection

### Phase 4: Audit Integration
- [ ] Integrate with audit plugin (silent dependency recording)
- [ ] Implement full audit data archiving
- [ ] Implement cleanup-aware audit (what's recorded depends on policy)
- [ ] Test audit data quality at each policy level

### Phase 5: Node Lifecycle
- [ ] Implement `Shmoo::Host::Lifecycle`
- [ ] Implement `take_away()` (verify, unmount, release)
- [ ] Test node takeover after cleanup
- [ ] Test node recovery from network share

### Phase 6: Network Share Integration
- [ ] Implement NFS mount integration
- [ ] Implement SMB mount integration
- [ ] Implement 9p mount integration (via 9p client)
- [ ] Implement HTTP/S3 integration (optional)
- [ ] Test all share types

### Phase 7: Testing & Hardening
- [ ] Test with real builds (gcc, make, perl)
- [ ] Test aggressive cleanup (extreme policy)
- [ ] Test node crash + recovery from network share
- [ ] Test silent dependency detection
- [ ] Test audit data at each policy level
- [ ] Benchmark streaming performance (should be < 50ms per artifact)

---

## 11. Summary

The client-side artifact streaming and cleanup spectrum turns the build system from a fragile, local-state machine into a **persistent, stateless execution engine**:

- **Artifact streaming** — every build output is streamed to the network share immediately, before deletion
- **Structured event logs** — each action produces a tarball of logs + audit data on the network share
- **Cleanup spectrum** — from CLEANING-EXTREME (ferret out silent dependencies) to CLEANING-EVERYTHING (keep everything including audit snoops)
- **Node lifecycle** — after cleanup, the node can be taken away without losing anything
- **Silent dependency detection** — aggressive cleanup exposes undeclared dependencies
- **Audit integration** — the audit plugin records ALL files accessed, providing full visibility

This makes the build system **resilient, reproducible, and observable** — artifacts live on the network share (persistent), build nodes are ephemeral (can be migrated or replaced), and the audit trail is complete (every action is recorded).
