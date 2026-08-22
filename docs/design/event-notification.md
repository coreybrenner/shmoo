# Design: Event Notification & Context Propagation

*2025-01-23 | Status: Active Design*
*Purpose: A lightweight, push-based event notification system that allows subordinate execution contexts (build jobs, nested environments like WSL/VMs) to report status transitions and connection events back to the original execution context (the Engineer's UI) in real-time. Includes immediate-alert handling for anomalies, transport negotiation, and service consistency across nested network topologies.*

---

## 1. Architecture: The Phone Home

The build system is a hierarchy of contexts. The "Original Execution Context" (OEC) is the one the engineer started (e.g., a terminal or a GUI app). All subordinate jobs, whether they are simple child processes or deeply nested environments (WSL, Parallels VM), eventually report back to the OEC.

```
OEC (Engineer's Terminal)
  │  [Socket: /tmp/shmoo.sock]
  │  [Transport: Direct or Relay]
  │
  ├─ Mode: DIRECT (Flat LAN / Host-Native)
  │   └─ Socket → 192.168.1.50:9001 → Parallels VM
  │
  └─ Mode: RELAY/PROXY (NAT / Nested VMs / WSL)
      └─ Socket → Mac Daemon:9000 → [Tunnel] → WSL Daemon:9002 → [Tunnel] → QEMU:9003
          │  [Socket: localhost:9002]
          │
          ├─ WSL Host Daemon (Linux Subsystem)
          │   │  [Socket: localhost:9002]
          │   │
          │   ├─ Job A (Compiling main.c)
          │   │   → Sends: {type: TRANS_START, ctx: "main.c", ...}
          │   │   → Sends: {type: TRANS_END, ctx: "main.c", status: "success", ...}
          │   │   → Sends: {type: FILE_PERM_CHANGE, alert: true, ...}
          │   │   → Sends: {type: UNEXPECTED_FILE, alert: true, ...}
          │   │
          │   └─ Parallels VM Job B (Linking app)
          │       → Sends: {type: LINK_UP, ctx: "parallels-vm", ...}
          │       → Sends: {type: TRANS_START, ctx: "link_app", ...}
          │       → Sends: {type: TRANS_END, ctx: "link_app", status: "failure", ...}
          │
          └─ ...
```

**Key Principle:**
- **Local Communication:** Jobs talk to their immediate parent (e.g., WSL to Mac host) via loopback TCP or Unix sockets.
- **Recursive Propagation:** The Mac host daemon receives the event, adds its own context ID (parent ID), and forwards the event up to the Engineer's UI.
- **Unified Interface:** The Engineer's UI sees a single flat stream of events, but each event carries a `context` and `parent` field to reconstruct the hierarchy.
- **Transport Negotiation:** The UI negotiates whether to use `DIRECT` mode (single-hop, stable network) or `RELAY` mode (multi-hop, NAT, dynamic IP), and falls back automatically if direct connectivity fails.

---

## 2. The Notification Protocol

To minimize overhead, the notification protocol is a lightweight JSON format sent over a simple socket.

### Packet Format (JSON)

```json
{
  "ver": "0.1",
  "ts": 1677000000.12345,
  "parent": "0",
  "context": "job-42",
  "type": "TRANS_START",
  "alert": false,
  "transport": "direct",
  "hop_count": 0,
  "hop_chain": [],
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
- **`alert`:** Boolean flag. `true` if this is an interrupting event that requires immediate UI feedback (red node).
- **`transport`:** Negotiated transport mode — `"direct"` (single-hop) or `"relay"` (multi-hop proxy).
- **`hop_count`:** Number of relay nodes the event traversed.
- **`hop_chain`:** List of context UUIDs that relayed the event (e.g., `["host-mac", "wsl-linux"]`).
- **`payload`:** Event-specific data.

### Event Types

| Type | Description | `alert`? | Payload |
|------|-------------|----------|---------|
| `LINK_UP` | A nested environment started (e.g., WSL, Parallels). | No | `{ "name": "wsl-host", "ip": "127.0.0.1", "port": 9002, "transport": "relay" }` |
| `LINK_DOWN` | A nested environment exited. | No | `{ "name": "wsl-host" }` |
| `TRANS_START` | A transition or action started. | No | `{ "action": "cc_compile", "file": "main.c" }` |
| `TRANS_END` | A transition or action finished. | No | `{ "status": "success|failure", "exit_code": 0 }` |
| `TRANS_WARN` | A transition warning. | No | `{ "message": "Unused variable..." }` |
| `STATE_CHANGE` | A variable changed in the environment. | Yes* | `{ "var": "CFLAGS", "old": "-O0", "new": "-O2" }` |
| `LOG_ENTRY` | A log line from the action. | No | `{ "level": "INFO", "msg": "Linking..." }` |
| `FILE_PERM_CHANGE` | File permissions changed unexpectedly. | **Yes** | `{ "path": "/outputs/main.o", "mode": "0755", "why": "chmod" }` |
| `UNEXPECTED_FILE` | A file appeared where it shouldn't. | **Yes** | `{ "path": "/outputs/build/tmp", "reason": "unknown creation" }` |
| `SECURITY_ALERT` | Dangerous command detected by security filter. | **Yes** | `{ "command": "rm -rf /", "context": "shell" }` |
| `ENV_LEAK` | Unprotected environment variable read. | Yes | `{ "var": "SECRET_KEY", "source": "getenv" }` |
| `TOPO_CHANGE` | Network topology changed (IP, port, NAT). | No | `{ "name": "wsl-host", "new_addr": "tcp:localhost:9003", "old_addr": "tcp:192.168.1.50:9003" }` |

*Note: `STATE_CHANGE` alerts are only shown if the change happens *during* the build (not the initial setup) and affects the build graph.

---

## 3. Transport Negotiation: Direct vs. Relay

The system uses a dual-mode routing layer that treats execution contexts as either direct endpoints or transparent relay nodes, depending on network reachability and topology complexity.

### Mode A: DIRECT (Flat LAN / Host-Native)

Single-hop, stable network. The OEC connects directly to the target context.

```
OEC → [Socket] → Target Context (e.g., 192.168.1.50:9003)
```

**Properties:**
- **Minimal latency:** No intermediate hops.
- **Topology:** Requires stable IPs, open ports, no NAT boundaries.
- **UI complexity:** Must handle multiple endpoints, authentication, certificates.
- **Security:** Direct exposure to host network.

**When to use:** Flat LAN, same-host builds, or when the target context has a stable, reachable IP.

### Mode B: RELAY/PROXY (NAT / Nested VMs / WSL)

Multi-hop, transparent routing. Each intermediate context acts as a relay, forwarding events upstream.

```
OEC → [Socket] → Host Daemon:9000 → [Tunnel] → WSL Daemon:9002 → [Tunnel] → QEMU:9003
```

**Properties:**
- **Topology awareness:** Works through NAT, DHCP, VM networking churn.
- **Service consistency:** The OEC always speaks the same protocol over the same local socket. It never needs to know that the target is inside a WSL subsystem, a Parallels NAT router, or a remote host.
- **Security:** Traffic stays within the controlled daemon chain, easier firewall/ACL.
- **Failover:** Automatic fallback if direct path drops.

**When to use:** Nested VMs, WSL, remote hosts, NAT boundaries, or dynamic IP environments.

### Auto-Negotiation

The UI attempts `DIRECT` first. If it fails (ECONNREFUSED, timeout, NAT drop), it falls back to `RELAY`.

```perl
sub negotiate_transport {
    my ($self, $context_info) = @_;
    
    # Try direct connection first
    my $direct_sock = IO::Socket::INET->new(
        PeerAddr => $context_info->{direct},
        Timeout  => 5,
    );
    
    if ($direct_sock) {
        return { transport => "direct", sock => $direct_sock };
    }
    
    # Fall back to relay mode
    return {
        transport => "relay",
        hop_chain => $context_info->{relay_chain},
        sock      => $self->relay_sock,  # Connected to local daemon
    };
}
```

### Transport Header

When using relay mode, each hop adds its context UUID to the `hop_chain`:

```json
{
  "transport": "relay",
  "hop_count": 2,
  "hop_chain": ["host-mac", "wsl-linux"],
  "context": "qemu-job-42"
}
```

The UI treats this as metadata. The actual event payload is unchanged — the relay daemons simply unwrap, add their UUID, and re-wrap for the next hop.

---

## 4. Alert Handling in the OEC

The Engineer's UI treats `alert: true` events differently from standard notifications.

### Immediate UI Response

1.  **Node Highlighting:** The node associated with the event is immediately highlighted in **Red** (if failure/security) or **Yellow** (if warning/change).
2.  **Modal/Toast:** A "toast" notification or a modal window appears in the OEC.
    *   **Message:** "Unexpected file created: `/outputs/build/tmp`"
    *   **Action:** "Inspect", "Ignore", "Kill Build"
3.  **Context Expansion:** The UI automatically expands the context tree to show the specific step where the alert occurred.

### Example: `FILE_PERM_CHANGE`

```json
{
  "type": "FILE_PERM_CHANGE",
  "alert": true,
  "context": "link:app",
  "payload": {
    "path": "/outputs/main.o",
    "mode": "0777",
    "reason": "chmod"
  }
}
```
**UI Action:**
*   Node `link:app` turns Yellow.
*   OEC displays: "Permission Change on `main.o` to 0777."
*   UI shows the "Red Path" tracing back to who ran `chmod` (e.g., a shell script inside the build rule).

### Example: `UNEXPECTED_FILE`

```json
{
  "type": "UNEXPECTED_FILE",
  "alert": true,
  "context": "link:app",
  "payload": {
    "path": "/outputs/build/scratch.dat",
    "reason": "write"
  }
}
```
**UI Action:**
*   Node `link:app` turns Yellow.
*   OEC displays: "Unexpected file written: `scratch.dat`."
*   UI highlights the file in the build graph as a "Ghost File" (not part of the declared DAG).

### Example: `SECURITY_ALERT`

```json
{
  "type": "SECURITY_ALERT",
  "alert": true,
  "context": "shell:build-script",
  "payload": {
    "command": "rm -rf /",
    "reason": "security filter match"
  }
}
```
**UI Action:**
*   Node `shell:build-script` turns Red.
*   OEC auto-kills the build.
*   UI displays: "SECURITY ALERT: Build terminated. `rm -rf /` detected."

---

## 5. Context Propagation

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

### Topology Changes

When a nested environment's network address changes (e.g., DHCP lease renewal, VM migration), it sends a `TOPO_CHANGE` event:

```json
{
  "type": "TOPO_CHANGE",
  "context": "qemu-guest",
  "payload": {
    "new_addr": "tcp:localhost:9003",
    "old_addr": "tcp:192.168.1.50:9003",
    "transport": "relay"  // Auto-switched from direct
  }
}
```
The UI automatically updates its routing table without interrupting the build.

---

## 6. Implementation in the Host Daemon

The Host Daemon acts as a "Forwarder". It listens on a socket for local jobs, and forwards events up to its parent.

### Host Daemon Config

```yaml
monitor:
  type: "unix_socket"  # Or "tcp"
  path: "/tmp/shmoo.sock"  # Path to the OEC
  port: 9000            # Or port if TCP
  transport_mode: "auto"  # direct | relay | auto
```

### Forwarding Logic (Relay Mode)

```perl
sub on_event_received {
    my ($self, $event_json) = @_;
    
    my $event = from_json($event_json);
    
    # 1. Set the parent ID to THIS daemon's context ID
    $event->{parent} = $self->context_id;
    
    # 2. Set the context ID to the sender's context ID (preserved from child)
    # (The child already set this in the packet)
    
    # 3. Add this daemon's context UUID to the hop chain
    push @{$event->{hop_chain}}, $self->context_id;
    $event->{hop_count} = @{$event->{hop_chain}};
    
    # 4. Set transport to relay (if not already)
    $event->{transport} = "relay" if $event->{transport} ne "relay";
    
    # 5. Add this daemon's timestamp
    $event->{ts} = time_now_microseconds();
    
    # 6. Forward to the Monitor (OEC) via relay socket
    $self->relay_socket->send(to_json($event));
    
    # 7. (Optional) Log locally
    $self->local_logger->info("Forwarding event $event->{type} from $event->{context}");
}
```

### Forwarding Logic (Direct Mode)

```perl
sub on_event_received_direct {
    my ($self, $event_json) = @_;
    
    my $event = from_json($event_json);
    
    # 1. Set the parent ID to THIS daemon's context ID
    $event->{parent} = $self->context_id;
    
    # 2. No hop chain needed for direct mode
    $event->{transport} = "direct";
    $event->{hop_chain} = [];
    $event->{hop_count} = 0;
    
    # 3. Forward to the Monitor (OEC) via direct socket
    $self->direct_socket->send(to_json($event));
}
```

---

## 7. Implementation in the Original Execution Context (OEC)

The OEC (Engineer's UI) is a "Consumer". It receives the stream and updates the UI.

### Event Loop (Relay Mode)

```perl
sub run_ui_loop_relay {
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
        } elsif ($event->{type} eq 'TOPO_CHANGE') {
            # Update routing table silently
            $self->routing_table->update($event->{payload}{new_addr});
        } elsif ($event->{type} eq 'STATE_CHANGE' || $event->{alert}) {
            # Alert: Highlight the node
            $self->tree->highlight($event->{context}, 'alert', $event->{type});
            $self->ui->show_notification($event->{type}, $event->{payload});
            
            # If it's a security alert, we might auto-kill the build
            if ($event->{type} eq 'SECURITY_ALERT') {
                $self->kill_build();
                $self->ui->show_modal("SECURITY ALERT: Build terminated by user.");
            }
        }
    }
}
```

### Transport Negotiation

The UI negotiates transport mode on `LINK_UP` events:

```perl
sub handle_link_up {
    my ($self, $event) = @_;
    
    my $context_name = $event->{payload}{name};
    my $direct_addr  = $event->{payload}{direct};
    my $relay_chain  = $event->{payload}{relay_chain};
    
    # Try direct connection first
    my $direct_sock = IO::Socket::INET->new(
        PeerAddr => $direct_addr,
        Timeout  => 5,
    );
    
    if ($direct_sock) {
        # Success: use direct mode
        $self->sockets{$context_name} = {
            transport => "direct",
            sock      => $direct_sock,
        };
        return;
    }
    
    # Fall back to relay mode
    $self->sockets{$context_name} = {
        transport => "relay",
        sock      => $self->relay_sock,  # Shared relay socket
        chain     => $relay_chain,
    };
}
```

---

## 8. Connection Events (Nested Environments)

When a nested environment starts (e.g., `wsl start` or `parallels start`), it must send a `LINK_UP` event.

### The Link Handshake

1. **Nested Env starts.**
2. **Nested Env sends `LINK_UP`** to the OEC (via its parent).
3. **OEC receives `LINK_UP`**, updates the Context Tree with the nested env's address.
4. **OEC can now connect directly** to the nested env for debugging (e.g., `shbuild debug --target wsl-host`).

### Address Propagation

The `LINK_UP` payload includes the nested env's *reachable* address:
```json
{
  "type": "LINK_UP",
  "payload": {
    "name": "wsl-host",
    "direct": "tcp:192.168.1.50:9002",
    "relay": "tcp:localhost:9000",
    "transport": "auto",
    "relay_chain": ["host-mac"]
  }
}
```
The OEC uses this address to forward connections from the engineer.

---

## 9. Integration with Syscall Interceptor

The syscall interceptor is the source of these alerts.

### `FILE_PERM_CHANGE`

When `chmod()` or `chown()` is intercepted:
1.  Check if the file is a build artifact.
2.  Check if the permission change is expected (e.g., compiler setting execute bit).
3.  If unexpected, generate `FILE_PERM_CHANGE` alert and send to monitor.

### `UNEXPECTED_FILE`

When `open()` or `create()` is intercepted:
1.  Check if the file is in a "clean" directory (e.g., `/outputs`).
2.  Check if the file matches the action's declared outputs.
3.  If it doesn't match, generate `UNEXPECTED_FILE` alert and send to monitor.

### `SECURITY_ALERT`

When a command is intercepted:
1.  Run command through security filter (e.g., regex match for `rm -rf` on `/`).
2.  If matched, generate `SECURITY_ALERT` and send to monitor.
3.  Optionally terminate the process immediately.

### `TOPO_CHANGE`

When a nested environment detects an IP/port change:
1.  Generate `TOPO_CHANGE` event.
2.  Send to monitor via current transport mode (direct or relay).
3.  OEC updates routing table automatically.

---

## 10. Summary

The **Event Notification** system adds a "phone home" layer to the build system:
- **Lightweight JSON:** Sent over local sockets, negligible overhead.
- **Recursive Propagation:** Every context forwards up, allowing deep nesting (Mac → WSL → Parallels) to report to the Engineer.
- **Context Tree:** The OEC builds a real-time visual tree of all active contexts.
- **Transition Events:** Every action (compile, link) reports start/end to the UI.
- **Immediate Alerts:** Anomalies (permission changes, unexpected files, security risks) trigger immediate UI feedback (Red nodes, toasts).
- **Dual-Mode Transport:** Auto-negotiates between DIRECT (single-hop) and RELAY (multi-hop proxy) modes based on network reachability.
- **Service Consistency:** The OEC always speaks the same protocol over the same local socket, regardless of underlying topology.
- **Connection Events:** Nested environments report `LINK_UP` so the UI knows where they are.
- **Topology Awareness:** `TOPO_CHANGE` events allow seamless network migration without build interruption.

This gives the engineer a **live, secure, and deeply connected view** of the entire nested build stack, with transport that adapts to whatever network topology is in place.
