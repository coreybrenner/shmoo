# Design: Event Notification & Context Propagation

*2025-01-23 | Status: Active Design*
*Purpose: A lightweight, push-based event notification system that allows subordinate execution contexts (build jobs, nested environments like WSL/VMs) to report status transitions and connection events back to the original execution context (the Engineer's UI) in real-time.*

---

## 1. Architecture: The Phone Home

The build system is a hierarchy of contexts. The "Original Execution Context" (OEC) is the one the engineer started (e.g., a terminal or a GUI app). All subordinate jobs, whether they are simple child processes or deeply nested environments (WSL, Parallels VM), eventually report back to the OEC.

```
OEC (Engineer's Terminal)
  │  [Socket: /tmp/shmoo.sock]
  │
  ├─ Host Daemon A (Mac)
  │   │  [Socket: localhost:9001]
  │   │
  │   ├─ WSL Host Daemon (Linux Subsystem)
  │   │   │  [Socket: localhost:9002]
  │   │   │
  │   │   ├─ Job A (Compiling main.c)
  │   │   │   → Sends: {type: TRANS_START, ctx: "main.c", ...}
  │   │   │   → Sends: {type: TRANS_END, ctx: "main.c", status: "success", ...}
  │   │   │
  │   │   └─ Parallels VM Job B (Linking app)
  │   │       → Sends: {type: LINK_UP, ctx: "parallels-vm", ...}
  │   │       → Sends: {type: TRANS_START, ctx: "link_app", ...}
  │   │       → Sends: {type: TRANS_END, ctx: "link_app", status: "failure", ...}
  │   │
  │   └─ ...
```

**Key Principle:**
- **Local Communication:** Jobs talk to their immediate parent (e.g., WSL to Mac host) via loopback TCP or Unix sockets.
- **Recursive Propagation:** The Mac host daemon receives the event, adds its own context ID (parent ID), and forwards the event up to the Engineer's UI.
- **Unified Interface:** The Engineer's UI sees a single flat stream of events, but each event carries a `context` and `parent` field to reconstruct the hierarchy.

---

## 2. The Notification Protocol

To minimize overhead, the notification protocol is a lightweight binary or JSON format sent over a simple socket.

### Packet Format (JSON)

```json
{
  "ver": "0.1",
  "ts": 1677000000.12345,
  "parent": "0",
  "context": "job-42",
  "type": "TRANS_START",
  "payload": {
    "action": "cc_compile",
    "file": "main.c",
    "flags": ["-O2", "-g"]
  }
}
```

### Fields
- **`ver`:** Protocol version.
- **`ts`:** Timestamp (epoch with microseconds).
- **`parent`:** UUID of the parent context (0 for the OEC).
- **`context`:** UUID of the current context (the sender).
- **`type`:** Event type (see below).
- **`payload`:** Event-specific data.

### Event Types

| Type | Description | Payload |
|------|-------------|---------|
| `LINK_UP` | A nested environment started (e.g., WSL, Parallels). | `{ "name": "wsl-host", "ip": "127.0.0.1", "port": 9002 }` |
| `LINK_DOWN` | A nested environment exited. | `{ "name": "wsl-host" }` |
| `TRANS_START` | A transition or action started. | `{ "action": "cc_compile", "file": "main.c" }` |
| `TRANS_END` | A transition or action finished. | `{ "status": "success|failure", "exit_code": 0 }` |
| `TRANS_WARN` | A transition warning. | `{ "message": "Unused variable..." }` |
| `STATE_CHANGE` | A variable changed in the environment. | `{ "var": "CFLAGS", "old": "-O0", "new": "-O2" }` |
| `LOG_ENTRY` | A log line from the action. | `{ "level": "INFO", "msg": "Linking..." }` |

---

## 3. Context Propagation

The "Original Execution Context" (OEC) needs to know the chain of commands that led to the current state.

### Chain of Custody

Every context has a unique UUID and a "Parent UUID".
- OEC UUID: `uuid-0`
- Host Daemon A UUID: `uuid-1`, Parent: `uuid-0`
- WSL Instance UUID: `uuid-2`, Parent: `uuid-1`
- Parallels Job UUID: `uuid-3`, Parent: `uuid-2`

When `uuid-3` sends an event, it includes `parent: "uuid-2"`. The OEC reconstructs the chain: `uuid-3` → `uuid-2` → `uuid-1` → `uuid-0`.

### Visualizing the Hierarchy

The OEC UI can render the hierarchy as a tree:

```
[ Engineer UI ] (uuid-0)
└── [ Mac Daemon ] (uuid-1)
    └── [ WSL Instance ] (uuid-2)
        └── [ Parallels VM ] (uuid-3)
            └── [ Job: Linking App ] (Current)
```

---

## 4. Implementation in the Host Daemon

The Host Daemon acts as a "Forwarder". It listens on a socket for local jobs, and forwards events up to its parent.

### Host Daemon Config

```yaml
monitor:
  type: "unix_socket"  # Or "tcp"
  path: "/tmp/shmoo.sock"  # Path to the OEC
  port: 9000            # Or port if TCP
```

### Forwarding Logic

```perl
sub on_event_received {
    my ($self, $event_json) = @_;
    
    my $event = from_json($event_json);
    
    # 1. Set the parent ID to THIS daemon's context ID
    $event->{parent} = $self->context_id;
    
    # 2. Set the context ID to the sender's context ID (preserved from child)
    # (The child already set this in the packet)
    
    # 3. Add this daemon's timestamp
    $event->{ts} = time_now_microseconds();
    
    # 4. Forward to the Monitor (OEC)
    $self->monitor_socket->send(to_json($event));
    
    # 5. (Optional) Log locally
    $self->local_logger->info("Forwarding event $event->{type} from $event->{context}");
}
```

---

## 5. Implementation in the Original Execution Context (OEC)

The OEC (Engineer's UI) is a "Consumer". It receives the stream and updates the UI.

### Event Loop

```perl
sub run_ui_loop {
    my ($self) = @_;
    
    my $sock = IO::Socket::UNIX->new(
        Type => SOCK_STREAM,
        Peer => "/tmp/shmoo.sock",
    );
    
    while (my $line = $sock->getline()) {
        my $event = from_json($line);
        
        # Update Context Tree
        $self->tree->upsert($event->{context}, $event->{parent});
        
        # Handle specific event types
        if ($event->{type} eq 'TRANS_START') {
            $self->tree->set_status($event->{context}, 'running');
        } elsif ($event->{type} eq 'TRANS_END') {
            $self->tree->set_status($event->{context}, $event->{payload}{status});
        } elsif ($event->{type} eq 'LINK_UP') {
            $self->tree->add_node($event->{payload}{name}, 'nested');
        } elsif ($event->{type} eq 'STATE_CHANGE') {
            $self->graph->highlight($event->{payload}{var});
        }
    }
}
```

---

## 6. Connection Events (Nested Environments)

When a nested environment starts (e.g., `wsl start` or `parallels start`), it must send a `LINK_UP` event.

### The Link Handshake

1. **Nested Env starts.**
2. **Nested Env sends `LINK_UP`** to the OEC (via its parent).
3. **OEC receives `LINK_UP`**, updates the Context Tree with the nested env's address.
4. **OEC can now connect directly** to the nested env for debugging (e.g., `shbuild debug --target wsl-host`).

### Address Propagation

The `LINK_UP` payload includes the nested env's *reachable* address.
```json
{
  "type": "LINK_UP",
  "payload": {
    "name": "wsl-host",
    "address": "tcp:localhost:9002",
    "type": "wsl"
  }
}
```
The OEC uses this address to forward connections from the engineer.

---

## 7. Summary

The **Event Notification** system adds a "phone home" layer to the build system:
- **Lightweight JSON:** Sent over local sockets, negligible overhead.
- **Recursive Propagation:** Every context forwards up, allowing deep nesting (Mac → WSL → Parallels) to report to the Engineer.
- **Context Tree:** The OEC builds a real-time visual tree of all active contexts.
- **Transition Events:** Every action (compile, link) reports start/end to the UI.
- **Connection Events:** Nested environments report `LINK_UP` so the UI knows where they are.

This gives the engineer a **live view of the entire nested build stack**, even across VM boundaries.
