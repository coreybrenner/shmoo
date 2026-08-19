# Demand-Starting Windows VMs Under QEMU on Linux

**Study guide for:** Remote execution infrastructure, runtime environment switching, self-orienting build scripts  
**Related Shmoo feature:** Dynamic execution target selection  
**Date:** 2026-08-19

---

## Table of Contents

1. [Overview](#1-overview)
2. [The Three Phases: Detect → Start → Execute](#2-the-three-phases-detect--start--execute)
3. [Tool Landscape: virsh, libvirt, QEMU Monitor, QEMU Guest Agent](#3-tool-landscape-virsh-libvirt-qemu-monitor-qemu-guest-agent)
4. [Phase 1: Detect Whether VM Is Running](#4-phase-1-detect-whether-vm-is-running)
5. [Phase 2: Start the VM on Demand](#5-phase-2-start-the-vm-on-demand)
6. [Phase 3: Execute Commands Inside the Guest](#6-phase-3-execute-commands-inside-the-guest)
7. [C-Language Integration](#7-c-language-integration)
8. [Complete Control Script](#8-complete-control-script)
9. [Integration with Shmoo's Build System](#9-integration-with-shmoo-build-system)
10. [Caveats and Edge Cases](#10-caveats-and-edge-cases)

---

## 1. Overview

The goal is a programmatic method to:
1. **Detect** whether a target Windows VM is already running
2. If stopped, **start** it
3. If running, **execute a command** inside it (no human interaction)
4. Return control to the host after execution

This is a control-plane problem. The host orchestrates three phases. Each phase has multiple technical approaches with different tradeoffs, different toolchains, and different C-language APIs.

The architecture is layered:

```
Host Process (C / Python / Perl)
    │
    ▼
┌─────────────┐
│ libvirt     │  ← High-level domain management
│ (C API)     │     virsh, virDomainCreate*, virDomainQemuAgentCommand
└──────┬──────┘
       │ socket (UNIX / TCP)
       ▼
┌─────────────┐
│ QEMU        │  ← Hypervisor
│ Monitor     │     QMP (query-status, guest-exec, guest-exec-status)
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Guest OS    │  ← Windows 10
│ (Agent)     │     qemu-ga service, PowerShell, compiled executables
└─────────────┘
```

Two execution paths:
- **libvirt path**: Host → libvirt C API → libvirtd daemon → QEMU monitor → guest
- **Direct QMP path**: Host → QEMU monitor socket → guest (bypasses libvirtd entirely)

---

## 2. The Three Phases: Detect → Start → Execute

### High-Level Flow

```
1. Identify the target VM by name or UUID
2. Get its current state
   └── If "running": skip to step 5
   └── If "shut off": start it
   └── If "paused": resume it
3. Wait for guest agent to become ready
4. Execute command inside guest via guest-agent
5. Parse exit code and output
6. (Optional) Shutdown domain
7. Return results
```

---

## 3. Tool Landscape: virsh, libvirt, QEMU Monitor, QEMU Guest Agent

### 3.1 `virsh` — The CLI Wrapper

`virsh` is the command-line interface to libvirt. It's a Perl script that wraps `libvirt.so`.

**What it does:**
- Domain lifecycle: `start`, `shutdown`, `destroy`, `reboot`, `suspend`, `resume`
- State inspection: `domstate`, `dominfo`, `list`, `domblklist`
- Agent communication: `qemu-agent-command` (wraps `virDomainQemuAgentCommand()`)
- XML manipulation: `dumpxml`, `edit`, `define`, `undefine`
- Interface/network management: `domif-setlink`, `net-list`

**Compilation:** Built from `libvirt/src/util/virsh` in the libvirt source tree. Links against `libvirt.so`.

**C API equivalent:** Every `virsh` command calls a libvirt C function. For example, `virsh start FOO` calls `virDomainCreate()` on the domain handle.

**When to use virsh vs C:**
- `virsh` for interactive admin and quick scripting
- C API for tight integration (no shell overhead, structured error handling, no JSON parsing)

### 3.2 libvirt C API

Installed package: `libvirt-dev` (headers + library)  
Library: `-lvirt`  
Version: Check with `pkg-config --modversion libvirt`

**Key functions for this workflow:**

```c
// Open connection to hypervisor
virConnectPtr virConnectOpen(const char *uri);          // "qemu:///system"
virConnectPtr virConnectOpenReadOnly(const char *uri);  // read-only

// Domain lookup
virDomainPtr virConnectLookupByName(virConnectPtr conn, const char *name);
virDomainPtr virConnectLookupDomainByUuidString(virConnectPtr conn, const char *uuid);
int virConnectListAllDomains(virConnectPtr conn, virDomainPtr **domains, unsigned int flags);

// Domain state
int virDomainGetState(virDomainPtr dom, int *state, int *reason, unsigned long long *memory);
// state values:
//   VIR_DOMAIN_RUNNING     = 1
//   VIR_DOMAIN_BLOCKED     = 2
//   VIR_DOMAIN_PAUSED      = 3
//   VIR_DOMAIN_SHUTDOWN    = 4
//   VIR_DOMAIN_SHUTOFF     = 5
//   VIR_DOMAIN_CRASHED     = 6
//   VIR_DOMAIN_PMSUSPENDED = 7

// Domain lifecycle
int virDomainCreate(virDomainPtr dom);           // Start a shut-off domain (non-blocking)
int virDomainDestroy(virDomainPtr dom);          // Force kill
int virDomainShutdown(virDomainPtr dom);         // Graceful shutdown (ACPI)
int virDomainReboot(virDomainPtr dom);           // Graceful reboot (ACPI)
int virDomainSuspend(virDomainPtr dom);          // Suspend to RAM
int virDomainResume(virDomainPtr dom);           // Resume from suspend

// Guest agent commands
char *virDomainQemuAgentCommand(virDomainPtr dom, const char *cmd, unsigned int flags);
// flags: VIR_DOMAIN_QEMU_AGENT_COMMAND_BLOCK (synchronous)

// Guest agent version/info
int virDomainQemuAgentGetVersion(virDomainPtr dom, int *version);

// Memory info
unsigned long long virDomainGetMaxMemory(virDomainPtr dom);
unsigned long long virDomainGetMemory(virDomainPtr dom, unsigned long long *memory);

// Domain XML
char *virDomainGetXMLDesc(virDomainPtr dom, unsigned int flags);

// Domain configuration (persistent vs. transient)
int virDomainCreateWithFlags(virDomainPtr dom, unsigned int flags);
// flags: VIR_DOMAIN_START_PAUSED, VIR_DOMAIN_START_FORCE_XML

// Error handling
virErrorPtr virGetLastError(void);
void virFreeError(virErrorPtr err);

// Cleanup
int virDomainFree(virDomainPtr dom);
int virConnectClose(virConnectPtr conn);
```

**Error handling pattern:**

```c
virErrorPtr err = virGetLastError();
if (err) {
    fprintf(stderr, "libvirt error %d: %s\n", err->code, err->message);
    virFreeError(err);
}
```

### 3.3 QEMU Monitor Protocol (QMP) — Direct Socket Access

QEMU exposes QMP over UNIX socket or TCP. This is the protocol that libvirtd uses under the hood.

**QMP socket location:**
- With libvirt: `/var/lib/libvirt/qemu/domain-<name>-monitor.sock`
- Raw QEMU: Whatever socket you specified with `-qmp`

**QMP is JSON-based.** All communication is JSON-RPC-style:

**Sending a command:**
```
{"execute":"<command>","arguments":{<args>}}\n
```

**Receiving a response:**
```
{"return":{<result>}}
{"error":{"class":<class>,"desc":<description>}}
```

**Receiving events:**
```
{"event":<event-name>,"data":{<data>}}
```

**Key QMP commands for this workflow:**

| Command | Purpose | Response |
|---------|---------|----------|
| `query-status` | Check if VM is running | `{"status":"running"|"paused"|"shutdown"}` |
| `cont` | Continue after `stop` | `{"return":{}}` |
| `guest-info` | Check if guest agent is present | List of supported commands |
| `guest-exec` | Execute command in guest | `{"pid":<pid>}` (async) |
| `guest-exec-status` | Get output/exit code for a PID | `{"exitcode":N,"stdout":"<base64>","stderr":"<base64>"}` |
| `guest-ping` | Check agent responsiveness | `{"return":{}}` |
| `query-vm-generation-id` | Get VM generation ID | `{"guid":"..."}` |
| `query-kvm` | Check KVM status | `{"enabled":true|false}` |
| `query-machines` | List supported machine types | Array of machine objects |

**Step-by-step: Manual QMP interaction (for understanding)**

```bash
# 1. Start a QEMU VM with QMP socket
qemu-system-x86_64 \
  -name "windows-test" \
  -m 4096 \
  -drive file=/var/lib/libvirt/images/windows.qcow2,format=qcow2 \
  -chardev socket,id=guest_agent,host=localhost,port=9003,server,nowait \
  -device isa-serial,chardev=guest_agent \
  -qmp unix:/tmp/win-test-qmp,server,nowait \
  -vnc :10 &

# 2. Check if running
echo '{"execute":"query-status"}' | socat - UNIX-CONNECT:/tmp/win-test-qmp
# Response: {"return":{"status":"running","singlestep":false,"running":true}}

# 3. Check guest agent
echo '{"execute":"guest-info"}' | socat - UNIX-CONNECT:/tmp/win-test-qmp
# If agent is running: {"return":{"name":"guest-agent","version":"1.2.0"}}
# If agent is not running: {"error":{"class":"DeviceNotActive","desc":"..."}}

# 4. Execute a command
echo '{"execute":"guest-exec","arguments":{"path":"/bin/ls","args":["-la"]}}' | socat - UNIX-CONNECT:/tmp/win-test-qmp
# Response: {"return":{"pid":12345}}

# 5. Get status (poll)
echo '{"execute":"guest-exec-status","arguments":{"pid":12345}}' | socat - UNIX-CONNECT:/tmp/win-test-qmp
# Response: {"return":{"exitcode":0,"stdout":"<base64>","stderr":""}}
```

**Pros of QMP:** No libvirt dependency, direct control  
**Cons of QMP:** Must know socket path, must handle JSON parsing manually, must understand QMP protocol details

### 3.4 QEMU Guest Agent (`qemu-ga`)

The guest agent runs **inside** the guest OS as a service. On Windows, it's `qemu-ga.exe` installed as a Windows service.

**Installation on Windows:**

1. Download the QEMU guest agent MSI from the QEMU release: https://fedorapeople.org/groups/virt/virtio-win/direct-downloads/latest-virtio/
2. Install: `msiexec /i qemu-ga.msi /qn /norestart`
3. Service name: `qemu-ga`
4. By default, it listens on a virtio-serial channel. To also expose a TCP port (for debugging or alternative access):

```
sc.exe config qemu-ga binpath="\"C:\Program Files\QEMU\qemu-ga.exe\" --method=virtio-serial --path=org.qemu.guest_agent.0" start=auto
```

**Installation on Linux guest:**
```bash
apt install qemu-guest-agent
systemctl enable --now qemu-guest-agent
```

**Configuration file on Windows (default location):**  
`C:\Program Files\QEMU\qemu-ga.conf`

```ini
[global]
method=virtio-serial
path=org.qemu.guest_agent.0
loglevel=info
logfile=C:\Program Files\QEMU\qemu-ga.log
```

**The agent must be running before any guest-exec commands will work.**

**Guest agent commands (visible via `guest-info`):**

| Command | Purpose |
|---------|---------|
| `guest-ping` | Check if agent is alive |
| `guest-info` | List supported commands |
| `guest-set-timezone` | Set guest timezone |
| `guest-set-user-password` | Change user password |
| `guest-get-time` | Get guest time |
| `guest-set-time` | Set guest time |
| `guest-suspend-disk` | Suspend to disk |
| `guest-suspend-ram` | Suspend to RAM |
| `guest-suspend-hybrid` | Hybrid suspend |
| `guest-fsfreeze-thaw` | Thaw filesystem freeze |
| `guest-fsfreeze-freeze` | Freeze filesystem |
| `guest-fsfreeze-status` | Check freeze status |
| `guest-exec` | Execute a command (this is our primary tool) |
| `guest-exec-status` | Get result of a guest-exec |
| `guest-get-vcpus` | Get virtual CPU config |
| `guest-set-vcpus` | Set virtual CPU count |
| `guest-get-memory-blocks` | Get memory block info |
| `guest-get-memory-blocks` | Get memory block info |
| `guest-get-fsinfo` | Get filesystem info |
| `guest-set-volatile-info` | Set volatile info |
| `guest-get-volatile-info` | Get volatile info |
| `guest-network-get-interfaces` | Get network interfaces |
| `guest-get-host-name` | Get hostname |
| `guest-get-users` | Get logged-in users |
| `guest-get-osinfo` | Get OS info |
| `guest-get-timezone` | Get timezone |

### 3.5 Comparison: libvirt C API vs QMP Direct

| Aspect | libvirt C API | Direct QMP |
|--------|--------------|------------|
| Installation | `libvirt-dev` package | Nothing extra — QEMU provides the socket |
| Connection URI | `qemu:///system` or `qemu:///session` | Direct UNIX socket path or TCP |
| Error handling | Structured `virErrorPtr` | JSON parsing + manual error check |
| Domain management | Full lifecycle API | Limited (must know socket path) |
| Guest agent | `virDomainQemuAgentCommand()` wraps it | Send raw JSON to QMP socket |
| Multi-hypervisor | Yes (KVM, Xen, LXC, etc.) | QEMU only |
| Overhead | One extra daemon (libvirtd) | No extra process |
| C call example | `virDomainQemuAgentCommand(dom, json, 0)` | Write JSON to socket, read response |

**For Shmoo:** The libvirt C API is the recommended path because it gives structured error handling, domain management, and guest agent commands all in one library. Direct QMP is the fallback if libvirtd is not running or unavailable.

---

## 4. Phase 1: Detect Whether VM Is Running

### 4.1 libvirtd Connection

First step is connecting to the hypervisor.

**C code:**

```c
#include <stdio.h>
#include <libvirt/libvirt.h>
#include <libvirt/qemu.h>

int main() {
    // Connect to QEMU system hypervisor
    virConnectPtr conn = virConnectOpen("qemu:///system");
    if (!conn) {
        fprintf(stderr, "Failed to connect to libvirt\n");
        return 1;
    }
    printf("Connected to libvirt\n");
    
    // ... work ...
    
    virConnectClose(conn);
    return 0;
}
```

**Compilation:**
```bash
gcc -o detect detect.c $(pkg-config --cflags --libs libvirt)
```

### 4.2 Lookup a Domain by Name

```c
virDomainPtr dom = virConnectLookupByName(conn, "windows-build");
if (!dom) {
    fprintf(stderr, "Domain not found\n");
    virConnectClose(conn);
    return 1;
}
```

### 4.3 Get Domain State

```c
int state, reason;
unsigned long long mem;
int rc = virDomainGetState(dom, &state, &reason, &mem);
if (rc < 0) {
    fprintf(stderr, "Failed to get domain state\n");
}

switch (state) {
    case VIR_DOMAIN_RUNNING:
        printf("Running\n");
        break;
    case VIR_DOMAIN_SHUTOFF:
        printf("Shut off\n");
        break;
    case VIR_DOMAIN_PAUSED:
        printf("Paused\n");
        break;
    case VIR_DOMAIN_SHUTDOWN:
        printf("Shutting down\n");
        break;
    case VIR_DOMAIN_CRASHED:
        printf("Crashed\n");
        break;
}

virDomainFree(dom);
```

### 4.4 List All Domains (For Discovery)

```c
virDomainPtr **domains;
int count = virConnectListAllDomains(conn, &domains, 0);
if (count < 0) {
    fprintf(stderr, "Failed to list domains\n");
}

for (int i = 0; i < count; i++) {
    const char *name = virDomainGetName(domains[i]);
    int state, reason;
    virDomainGetState(domains[i], &state, &reason, NULL);
    printf("%s: %d\n", name, state);
}

// Free the array
for (int i = 0; i < count; i++) {
    virDomainFree(domains[i]);
}
free(domains);
```

### 4.5 QMP Alternative: Direct Socket Inspection

If libvirtd is not available, connect directly to the QMP socket:

```c
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

// Connect to QMP socket and send a command
// Returns 0 on success, -1 on failure
// Response is stored in *response, caller must free it
int qmp_send_command(const char *socket_path, const char *command, char **response) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    
    // Send command
    send(sock, command, strlen(command), 0);
    
    // Read response (simple blocking read — for production, use event loop)
    char buf[65536];
    ssize_t n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        close(sock);
        return -1;
    }
    buf[n] = '\0';
    
    *response = strndup(buf, n);
    close(sock);
    return 0;
}

// Usage:
char *response = NULL;
qmp_send_command(
    "/var/lib/libvirt/qemu/domain-windows-build-monitor.sock",
    "{\"execute\":\"query-status\"}\n",
    &response
);
// response contains: {"return":{"status":"running",...}}
// Parse the JSON to check status
free(response);
```

**Finding the socket path without libvirt:**
```bash
# The socket path follows a convention:
# /var/lib/libvirt/qemu/domain-<name>-monitor.sock
# If libvirtd is down, you can also find it in /proc:
pgrep -a qemu | grep domain-$(virsh domblklist windows-build 2>/dev/null | head -1 | awk '{print $2}')
```

This approach is fragile — the socket path may vary by QEMU version and libvirt configuration.

---

## 5. Phase 2: Start the VM on Demand

### 5.1 libvirt: Start a Shut-Off Domain

```c
#include <libvirt/libvirt.h>
#include <libvirt/qemu.h>

virDomainPtr dom = virConnectLookupByName(conn, "windows-build");
if (!dom) { ... }

// Create (start) the domain. Non-blocking.
int rc = virDomainCreate(dom);
if (rc < 0) {
    virErrorPtr err = virGetLastError();
    fprintf(stderr, "Failed to start: %s\n", err->message);
    virFreeError(err);
}
// rc == 0 means the QEMU process has been spawned
// The domain is NOT immediately in RUNNING state yet
// Wait for guest agent to confirm readiness (see Phase 3)

virDomainFree(dom);
```

### 5.2 libvirt: Resume a Paused Domain

```c
int rc = virDomainResume(dom);
if (rc < 0) {
    virErrorPtr err = virGetLastError();
    fprintf(stderr, "Failed to resume: %s\n", err->message);
    virFreeError(err);
}
```

### 5.3 Start a Transient Domain (No Persistent XML)

If you want to start a domain from an XML file without making it persistent:

```c
#include <libvirt/libvirt.h>

const char *xml = "<domain type='kvm'>\n"
    "<name>windows-build</name>\n"
    "<memory unit='MiB'>4096</memory>\n"
    "<vcpu placement='static'>2</vcpu>\n"
    "<os>\n"
    "  <type arch='x86_64' machine='pc-q35-8.2'>hvm</type>\n"
    "  <boot dev='hd'/>\n"
    "</os>\n"
    "<devices>\n"
    "  <emulator>/usr/bin/qemu-system-x86_64</emulator>\n"
    "  <disk type='file' device='disk'>\n"
    "    <driver name='qemu' type='qcow2'/>\n"
    "    <source file='/var/lib/libvirt/images/windows.qcow2'/>\n"
    "  </disk>\n"
    "  <channel type='unix'>\n"
    "    <source mode='bind'/>\n"
    "    <target type='virtio' name='org.qemu.guest_agent.0'/>\n"
    "  </channel>\n"
    "</devices>\n"
    "<on_poweroff>destroy</on_poweroff>\n"
    "<on_reboot>restart</on_reboot>\n"
    "<on_crash>restart</on_crash>\n"
    "</domain>\n";

virDomainPtr dom = virConnectDefineXML(conn, xml, 0);
if (!dom) { ... error handling ... }

// Start the transient domain
int rc = virDomainCreate(dom);

// When done, free the domain handle
virDomainFree(dom);
// Transient domains are automatically removed when shut down
```

### 5.4 QMP-Only Start: Raw QEMU Process

If libvirt is unavailable entirely:

```bash
qemu-system-x86_64 \
  -name "windows-build" \
  -m 4096 \
  -drive file=/var/lib/libvirt/images/windows.qcow2,format=qcow2 \
  -chardev socket,id=guest_agent,host=localhost,port=9003,server,nowait \
  -device isa-serial,chardev=guest_agent \
  -qmp unix:/tmp/win-build-qmp,server,nowait \
  -vnc :10 \
  -no-acpi \
  -display none \
  2>/tmp/qemu.log &
```

Then send `cont` via QMP:

```c
// After connecting to the QMP socket:
const char *cmd = "{\"execute\":\"cont\"}\n";
send(sock, cmd, strlen(cmd), 0);
// This resumes the VM if it was stopped, or is a no-op if already running
```

### 5.5 Wait for Guest Agent to Become Ready

After starting, the guest agent takes time to initialize. Poll `guest-info`:

```c
#include <libvirt/libvirt.h>
#include <libvirt/qemu.h>
#include <string.h>
#include <time.h>

int wait_for_guest_agent(virDomainPtr dom, int timeout_sec) {
    char *response;
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - start.tv_sec > timeout_sec) {
            fprintf(stderr, "Timeout waiting for guest agent (%ds)\n", timeout_sec);
            return -1;
        }
        
        response = virDomainQemuAgentCommand(dom,
            "{\"execute\":\"guest-info\"}\n", 0);
        
        if (response != NULL) {
            // Agent is present — check if it's actually responding
            // guest-info returns the agent's supported commands list
            // A successful return means the channel is working
            printf("Guest agent ready: %s\n", response);
            free(response);
            return 0;
        }
        
        // Error means agent is not ready yet — wait and retry
        usleep(2000000); // 2 second interval
    }
}
```

**Important:** The return value of `virDomainQemuAgentCommand()` is the key:
- `NULL` = agent not responding (or error)
- Non-NULL (a JSON string) = agent responded successfully

This is the standard pattern for waiting.

---

## 6. Phase 3: Execute Commands Inside the Guest

### 6.1 QEMU Guest Agent `guest-exec` — The Primary Method

The guest agent's `guest-exec` command executes a binary in the guest and returns a process ID. The caller then polls `guest-exec-status` to get output and exit code.

#### Step-by-Step: C Implementation

**Step 1: Build the guest-exec command JSON**

```c
// Build the guest-exec command as a JSON string
// This executes C:\Windows\System32\cmd.exe with /c echo hello
const char *exec_cmd =
    "{"
    "  \"execute\":\"guest-exec\","
    "  \"arguments\":{"
    "    \"path\":\"C:\\\\Windows\\\\System32\\\\cmd.exe\","
    "    \"args\":[\"/c\",\"echo hello\"],"
    "    \"capture_output\":true"
    "  }"
    "}";
```

Note the double-escaping of backslashes in C strings (Windows paths use `\`).

**Step 2: Send to guest agent**

```c
char *response = virDomainQemuAgentCommand(dom, exec_cmd, 0);
if (!response) {
    fprintf(stderr, "guest-exec failed\n");
    // Agent not responding, channel broken, or command syntax error
}

// Parse the PID from the response
// Response format: {"pid":12345}
// Simple JSON parsing — for production, use a real JSON parser
unsigned int pid = 0;
if (sscanf(response, "{\"pid\":%u}", &pid) != 1) {
    fprintf(stderr, "Failed to parse PID from: %s\n", response);
}
free(response);
```

**Step 3: Poll for completion**

```c
#include <libvirt/libvirt.h>
#include <libvirt/qemu.h>
#include <string.h>
#include <time.h>
#include <strings.h>  // for strcasestr
#include <ctype.h>

typedef struct {
    int exit_code;
    char *stdout_data;
    char *stderr_data;
    int got_output;
} exec_result_t;

// Decode base64 (simple implementation — use libbase64 or openssl for production)
// This is a simplified version for illustration
char *decode_base64_simple(const char *input) {
    // Production code should use: openssl/base64.h
    // or libb64
    // For now, return input if it's not base64 (pass-through for testing)
    return strdup(input);
}

exec_result_t poll_guest_exec(virDomainPtr dom, unsigned int pid, int timeout_sec) {
    exec_result_t result = { .exit_code = -1, .stdout_data = NULL, .stderr_data = NULL, .got_output = 0 };
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - start.tv_sec > timeout_sec) {
            fprintf(stderr, "Timeout waiting for guest-exec PID %u\n", pid);
            break;
        }
        
        char *status_cmd = NULL;
        asprintf(&status_cmd,
            "{\"execute\":\"guest-exec-status\",\"arguments\":{\"pid\":%u}}", pid);
        
        char *response = virDomainQemuAgentCommand(dom, status_cmd, 0);
        free(status_cmd);
        
        if (response) {
            // Parse the response
            // Expected format:
            // {"exitcode":0,"stdout":"<base64>","stderr":""}
            // OR (not finished yet):
            // {"running":true}
            
            if (strstr(response, "\"running\":true")) {
                // Still running, wait and poll again
                free(response);
                usleep(500000); // 500ms
                continue;
            }
            
            // Parse exit code
            int exit_code = 0;
            char *stdout_b64 = NULL;
            char *stderr_b64 = NULL;
            
            // Extract exitcode (simplified — use a JSON parser in production)
            if (sscanf(response, "{\"exitcode\":%d", &exit_code) == 1) {
                result.exit_code = exit_code;
            }
            
            // Extract stdout (base64)
            char *pos = strstr(response, "\"stdout\":\"");
            if (pos) {
                pos += 10; // skip '"stdout":"'
                char *end = strstr(pos, "\"");
                if (end) {
                    *end = '\0';
                    stdout_b64 = decode_base64_simple(pos);
                    *end = '"'; // restore
                }
            }
            
            // Extract stderr (base64)
            pos = strstr(response, "\"stderr\":\"");
            if (pos) {
                pos += 11; // skip '"stderr":"'
                char *end = strstr(pos, "\"");
                if (end) {
                    *end = '\0';
                    stderr_b64 = decode_base64_simple(pos);
                    *end = '"'; // restore
                }
            }
            
            result.stdout_data = stdout_b64;
            result.stderr_data = stderr_b64;
            result.got_output = 1;
            
            free(response);
            break; // Done — command has exited
        }
        
        // Response was NULL — agent not responding
        // Could mean the guest froze or the channel dropped
        usleep(500000); // 500ms
    }
    
    return result;
}
```

**Step 4: Clean up**

```c
exec_result_t result = poll_guest_exec(dom, pid, 60);
printf("Exit code: %d\n", result.exit_code);
if (result.got_output) {
    printf("Output:\n%s\n", result.stdout_data ? result.stdout_data : "(none)");
    printf("Stderr:\n%s\n", result.stderr_data ? result.stderr_data : "(none)");
    free(result.stdout_data);
    free(result.stderr_data);
}
```

### 6.2 Base64 Handling

QEMU guest agent encodes captured stdout/stderr as base64. Use proper base64 decoding:

```c
#include <openssl/bio.h>
#include <openssl/buffer.h>

// Proper base64 decode using OpenSSL
char *base64_decode(const char *input, int *output_length) {
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *bmem = BIO_new_mem_buf(input, -1);
    bmem = BIO_push(b64, bmem);
    
    char *buf = malloc(strlen(input) + 1);
    *output_length = BIO_read(bmem, buf, strlen(input));
    buf[*output_length] = '\0';
    
    BIO_free_all(bmem);
    return buf;
}
```

**Compilation with OpenSSL:**
```bash
gcc -o shmoo-vm-run vm-run.c -lvirt -lcrypto $(pkg-config --cflags --libs libvirt)
```

### 6.3 Complete C Example: `shmoo-vm-run`

```c
/*
 * shmoo-vm-run.c — Demand-start a QEMU VM and execute a command inside it
 * 
 * Compiles:
 *   gcc -o shmoo-vm-run shmoo-vm-run.c -lvirt -lcrypto
 * 
 * Requires:
 *   - libvirt-dev (for virDomainQemuAgentCommand)
 *   - openssl (for base64 decoding)
 *   - User in 'libvirt' group or root access
 *   - Windows 10 VM named "windows-build" with qemu-ga installed
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <libvirt/libvirt.h>
#include <libvirt/qemu.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

typedef struct {
    int exit_code;
    char *stdout_data;
    char *stderr_data;
    int got_output;
    int timed_out;
} exec_result_t;

// Base64 decode
char *base64_decode(const char *input, int *out_len) {
    BIO *b64 = BIO_new(BIO_f_base64());
    BIO *bmem = BIO_new_mem_buf(input, -1);
    bmem = BIO_push(b64, bmem);
    
    // OpenSSL's base64 decoder adds newlines every 76 chars internally
    // Read in chunks
    char buf[4096];
    int total = 0;
    char *result = malloc(4096);
    *out_len = 0;
    
    while (1) {
        int n = BIO_read(bmem, buf, sizeof(buf));
        if (n <= 0) break;
        if (total + n > 4096) {
            result = realloc(result, total + 4096);
        }
        memcpy(result + total, buf, n);
        total += n;
    }
    *out_len = total;
    result[total] = '\0';
    
    BIO_free_all(bmem);
    return result;
}

// Wait for guest agent to become available
int wait_for_agent(virDomainPtr dom, int timeout_sec) {
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - start.tv_sec > timeout_sec) {
            return -1;
        }
        
        char *resp = virDomainQemuAgentCommand(dom,
            "{\"execute\":\"guest-info\"}\n", 0);
        if (resp) {
            free(resp);
            return 0; // Agent ready
        }
        usleep(2000000); // 2s
    }
}

// Execute a command via guest-agent
exec_result_t exec_in_guest(virDomainPtr dom,
                           const char *path,
                           char **args,
                           int arg_count,
                           int timeout_sec) {
    exec_result_t result = { .exit_code = -2, .stdout_data = NULL, .stderr_data = NULL,
                             .got_output = 0, .timed_out = 0 };
    
    // Build guest-exec command
    char *cmd = NULL;
    asprintf(&cmd, "{\"execute\":\"guest-exec\",\"arguments\":{\"path\":\"%s\",\"capture_output\":true}",
             path);
    
    if (arg_count > 0) {
        strcat(cmd, ",\"args\":[");
        for (int i = 0; i < arg_count; i++) {
            if (i > 0) strcat(cmd, ",");
            // Escape backslashes and quotes for JSON
            const char *a = args[i];
            strcat(cmd, "\"");
            while (*a) {
                if (*a == '\\') strcat(cmd, "\\\\");
                else if (*a == '"') strcat(cmd, "\\\"");
                else strcat(cmd, a);
                a++;
            }
            strcat(cmd, "\"");
        }
        strcat(cmd, "]");
    }
    
    strcat(cmd, "}}");
    
    // Send the command
    char *exec_resp = virDomainQemuAgentCommand(dom, cmd, 0);
    free(cmd);
    
    if (!exec_resp) {
        fprintf(stderr, "guest-exec failed to spawn\n");
        return result;
    }
    
    // Parse PID
    unsigned int pid = 0;
    if (sscanf(exec_resp, "{\"pid\":%u}", &pid) != 1) {
        fprintf(stderr, "Failed to parse PID from: %s\n", exec_resp);
        free(exec_resp);
        return result;
    }
    free(exec_resp);
    
    fprintf(stderr, "guest-exec PID: %u\n", pid);
    
    // Poll for completion
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - start.tv_sec > timeout_sec) {
            result.timed_out = 1;
            fprintf(stderr, "Timeout after %ds\n", timeout_sec);
            break;
        }
        
        char *status_cmd = NULL;
        asprintf(&status_cmd,
            "{\"execute\":\"guest-exec-status\",\"arguments\":{\"pid\":%u}}", pid);
        
        char *status_resp = virDomainQemuAgentCommand(dom, status_cmd, 0);
        free(status_cmd);
        
        if (status_resp) {
            // Check if still running
            if (strstr(status_resp, "\"running\":true")) {
                free(status_resp);
                usleep(500000); // 500ms
                continue;
            }
            
            // Parse result
            // Check for exitcode
            if (strstr(status_resp, "\"exitcode\"")) {
                int ec = 0;
                if (sscanf(status_resp, "{\"exitcode\":%d", &ec) == 1) {
                    result.exit_code = ec;
                }
                
                // Parse stdout
                char *pos = strstr(status_resp, "\"stdout\":\"");
                if (pos) {
                    pos += 10;
                    char *end = strstr(pos, "\"");
                    if (end) {
                        *end = '\0';
                        int out_len = 0;
                        result.stdout_data = base64_decode(pos, &out_len);
                        *end = '"';
                        if (out_len > 0) {
                            // Strip trailing newlines from base64'd output
                            while (out_len > 0 && (result.stdout_data[out_len-1] == '\n' || result.stdout_data[out_len-1] == '\r')) {
                                result.stdout_data[--out_len] = '\0';
                            }
                        }
                    }
                }
                
                // Parse stderr
                pos = strstr(status_resp, "\"stderr\":\"");
                if (pos) {
                    pos += 11;
                    char *end = strstr(pos, "\"");
                    if (end) {
                        *end = '\0';
                        int err_len = 0;
                        result.stderr_data = base64_decode(pos, &err_len);
                        *end = '"';
                        if (err_len > 0) {
                            while (err_len > 0 && (result.stderr_data[err_len-1] == '\n' || result.stderr_data[err_len-1] == '\r')) {
                                result.stderr_data[--err_len] = '\0';
                            }
                        }
                    }
                }
                
                result.got_output = 1;
            }
            
            free(status_resp);
            break; // Done
        }
        
        // No response — agent may have died
        usleep(500000);
    }
    
    return result;
}

// Print usage
void usage(const char *prog) {
    fprintf(stderr, "Usage: %s <command> [args...]\n", prog);
    fprintf(stderr, "  Executes a command inside the 'windows-build' QEMU VM\n");
    fprintf(stderr, "  Requires libvirt and qemu-ga running in the guest\n");
    fprintf(stderr, "\nExample:\n");
    fprintf(stderr, "  %s C:\\\\Windows\\\\System32\\\\cmd.exe /c ver\n", prog);
    exit(1);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        usage(argv[0]);
    }
    
    // Connect to libvirt
    virConnectPtr conn = virConnectOpen("qemu:///system");
    if (!conn) {
        fprintf(stderr, "Failed to connect to libvirt\n");
        return 1;
    }
    
    // Lookup domain
    virDomainPtr dom = virConnectLookupByName(conn, "windows-build");
    if (!dom) {
        fprintf(stderr, "Domain 'windows-build' not found\n");
        virConnectClose(conn);
        return 1;
    }
    
    // Check if running
    int state, reason;
    if (virDomainGetState(dom, &state, &reason, NULL) < 0) {
        fprintf(stderr, "Failed to get domain state\n");
        virDomainFree(dom);
        virConnectClose(conn);
        return 1;
    }
    
    if (state == VIR_DOMAIN_SHUTOFF) {
        fprintf(stderr, "Domain is shut off, starting...\n");
        if (virDomainCreate(dom) < 0) {
            virErrorPtr err = virGetLastError();
            fprintf(stderr, "Failed to start: %s\n", err->message);
            virFreeError(err);
            virDomainFree(dom);
            virConnectClose(conn);
            return 1;
        }
        
        // Wait for agent
        fprintf(stderr, "Waiting for guest agent...\n");
        if (wait_for_agent(dom, 60) < 0) {
            fprintf(stderr, "Timeout waiting for guest agent\n");
            virDomainFree(dom);
            virConnectClose(conn);
            return 1;
        }
        fprintf(stderr, "Guest agent ready\n");
    } else if (state != VIR_DOMAIN_RUNNING) {
        fprintf(stderr, "Domain is in unexpected state %d\n", state);
        virDomainFree(dom);
        virConnectClose(conn);
        return 1;
    } else {
        fprintf(stderr, "Domain is already running\n");
    }
    
    // Execute the command
    char **args = (char **)&argv[1];
    int arg_count = argc - 1;
    
    fprintf(stderr, "Executing: %s", argv[1]);
    for (int i = 2; i < argc; i++) {
        fprintf(stderr, " %s", argv[i]);
    }
    fprintf(stderr, "\n");
    
    exec_result_t result = exec_in_guest(dom, argv[1], args, arg_count, 120);
    
    printf("EXIT_CODE=%d\n", result.exit_code);
    if (result.got_output) {
        if (result.stdout_data) {
            printf("STDOUT:\n%s\n", result.stdout_data);
            free(result.stdout_data);
        }
        if (result.stderr_data) {
            printf("STDERR:\n%s\n", result.stderr_data);
            free(result.stderr_data);
        }
    }
    if (result.timed_out) {
        printf("TIMED_OUT=1\n");
    }
    
    virDomainFree(dom);
    virConnectClose(conn);
    return result.timed_out ? 2 : result.exit_code;
}
```

**Compile:**
```bash
gcc -o /usr/local/bin/shmoo-vm-run /path/to/shmoo-vm-run.c -lvirt -lcrypto
```

**Test:**
```bash
# Install in guest first:
# Windows: msiexec /i qemu-ga.msi /qn
# Then on host:
shmoo-vm-run C:\Windows\System32\cmd.exe /c ver
# Expected output:
# EXIT_CODE=0
# STDOUT:
# Microsoft Windows [Version 10.0.19045.4894]
```

### 6.4 Alternative: Direct QMP Without libvirt

If libvirtd is not running but QEMU is:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>

// Connect to QMP socket
int connect_qmp(const char *socket_path) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    
    // Send handshake
    const char *handshake = "{\"execute\":\"qmp_capabilities\"}\n";
    send(sock, handshake, strlen(handshake), 0);
    
    // Read response
    char buf[4096];
    recv(sock, buf, sizeof(buf), 0);
    // Response: {"return":{}}
    
    return sock;
}

// Send a command and get response
char *qmp_command(int sock, const char *cmd) {
    char buf[65536];
    memset(buf, 0, sizeof(buf));
    
    // Send command (append newline for line-delimited JSON)
    char *full_cmd = malloc(strlen(cmd) + 2);
    strcpy(full_cmd, cmd);
    strcat(full_cmd, "\n");
    send(sock, full_cmd, strlen(full_cmd), 0);
    free(full_cmd);
    
    // Read response
    int n = recv(sock, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return NULL;
    buf[n] = '\0';
    
    // Handle events — skip them, find the response
    // QMP is line-delimited, responses are one line each
    // For simplicity, return the last line
    char *last_line = NULL;
    char *line = strtok(buf, "\n");
    while (line) {
        last_line = strdup(line);
        line = strtok(NULL, "\n");
    }
    
    return last_line;
}

// Example usage:
// int sock = connect_qmp("/var/lib/libvirt/qemu/domain-windows-build-socket.sock");
// char *resp = qmp_command(sock, "{\"execute\":\"query-status\"}");
// Check resp for {"status":"running"}
// char *exec = qmp_command(sock, "{\"execute\":\"guest-exec\",\"arguments\":{\"path\":\"...\"}}");
// Parse PID from exec
// ... poll guest-exec-status ...
```

### 6.5 Comparison: libvirt C API vs Direct QMP

| Feature | libvirt C API | Direct QMP |
|---------|--------------|------------|
| Installation | `libvirt-dev` | Nothing extra |
| Connect | `virConnectOpen("qemu:///system")` | `socket()` + connect to UNIX socket |
| Domain lookup | `virConnectLookupByName(conn, name)` | Must know socket path |
| Check state | `virDomainGetState(dom, &state, &reason, &mem)` | Send `query-status` JSON |
| Start domain | `virDomainCreate(dom)` | Must have started QEMU; send `cont` |
| Guest agent check | `virDomainQemuAgentCommand(dom, guest-info, 0)` | Send `guest-info` JSON |
| Execute command | `virDomainQemuAgentCommand(dom, guest-exec JSON, 0)` | Send `guest-exec` JSON |
| Get output | `virDomainQemuAgentCommand(dom, guest-exec-status JSON, 0)` | Send `guest-exec-status` JSON |
| Error handling | Structured `virErrorPtr` | Manual JSON parsing |
| Domain management | Full API (suspend, resume, shutdown, destroy, define, undefine, dumpxml) | Limited (QMP commands only) |
| Multi-hypervisor | Yes (KVM, Xen, LXC, VMWare, etc.) | QEMU/KVM only |

**For Shmoo:** Use libvirt C API. It's the more complete, more robust, and more portable option. The only reason to use direct QMP is if libvirtd is unavailable or if you need bare-metal control for low-level operations (live migration parameters, CPU pinning, NUMA topology, etc.).

---

## 7. C-Language Integration with Shmoo

### 7.1 XS Binding Pattern

The C library can be bound to Perl via XS:

```c
// Shmoo/C/VmRun.xs

MODULE = Shmoo::C::VmRun    PACKAGE = Shmoo::C::VmRun

void
run_command(vm_name, command_path, ...args)
    char *vm_name
    char *command_path
    PREINIT:
        virConnectPtr conn;
        virDomainPtr dom;
        int state, reason;
        exec_result_t result;
    CODE:
        conn = virConnectOpen("qemu:///system");
        if (!conn) {
            XSRETURN_UNDEF;
        }
        
        dom = virConnectLookupByName(conn, vm_name);
        if (!dom) {
            virConnectClose(conn);
            XSRETURN_UNDEF;
        }
        
        // Check state, start if needed, wait for agent...
        // (same logic as above)
        
        // Execute command
        result = exec_in_guest(dom, command_path, args, n_args, timeout);
        
        // Return: { exit_code, stdout, stderr }
        AV *ret = newAV();
        sv_setiv(newAV(), result.exit_code);
        sv_setpv(newAV(), result.stdout_data);
        sv_setpv(newAV(), result.stderr_data);
        
        virDomainFree(dom);
        virConnectClose(conn);
        
        PUSHs(sv_2mortal(newRV((SV*)ret)));
```

This gives Shmoo's Perl code direct access to the libvirt C API without shell overhead. The C library handles all the JSON encoding/decoding, base64 decoding, and error handling.

### 7.2 Inline::C Alternative

If XS is too much setup, Inline::C works too:

```perl
use Inline C => <<'END_C';
#include <libvirt/libvirt.h>

SV* run_command(const char* vm_name, const char* command_path) {
    virConnectPtr conn = virConnectOpen("qemu:///system");
    if (!conn) {
        return &PL_sv_undef;
    }
    virDomainPtr dom = virConnectLookupByName(conn, vm_name);
    if (!dom) {
        virConnectClose(conn);
        return &PL_sv_undef;
    }
    
    // ... execute logic ...
    
    virDomainFree(dom);
    virConnectClose(conn);
    
    return newSViv(exit_code);  // or return a hash reference
}
END_C
```

### 7.3 Shmoo Target Definition

In Shmoo's build target syntax:

```perl
target "compile-windows" {
    # Check if target environment is Windows VM
    my $target_env = get_target_environment();
    
    if ($target_env ne "windows-vm") {
        # Demand-start the VM
        my $vm = Shmoo::VM->new("windows-build");
        $vm->start_if_stopped();
        
        # Wait for guest agent
        $vm->wait_for_agent(timeout => 120);
        
        # Execute build command
        my $result = $vm->exec(
            path    => "C:\\build\\compile.exe",
            args    => ["--release", "--config", "Release"],
            timeout => 600,
        );
        
        # Record origin: this output came from guest execution
        record_origin($result, "vm-execution", "windows-build");
        
        if ($result->{exit_code} != 0) {
            die "Build failed in Windows VM: $result->{stderr}";
        }
        
        $self->{target_env} = $target_env;  # Cache for subsequent targets
    }
    
    # ... continue build ...
}
```

The key insight: Shmoo's origin provenance system records that a build step's output was produced via `guest-exec` on a specific VM. Every value carries the origin chain, and the guest execution context becomes part of that chain.

---

## 8. Complete Control Script (Perl)

For Shmoo's Perl-based build system, here's a ready-to-use module:

```perl
package Shmoo::VM::Runner;
use strict;
use warnings;
use JSON::PP;
use Socket;

=head1 NAME

Shmoo::VM::Runner — Demand-start a QEMU Windows VM and execute commands inside it.

=head1 SYNOPSIS

    use Shmoo::VM::Runner;
    
    my $vm = Shmoo::VM::Runner->new(domain => "windows-build");
    
    # Start if not running, wait for agent
    $vm->start();
    
    # Execute a command
    my $result = $vm->exec(
        path    => "C:\\Windows\\System32\\cmd.exe",
        args    => ["/c", "echo hello"],
        timeout => 120,
    );
    
    print "Exit: $result->{exit_code}\n";
    print "Output: $result->{stdout}\n";

=head1 METHODS

=cut

sub new {
    my ($class, %args) = @_;
    my $self = {
        domain   => $args{domain} || "windows-build",
        timeout  => $args{timeout} // 60,
        virsh    => "/usr/bin/virsh",
    };
    bless $self, $class;
    return $self;
}

=head2 is_running

Check if the domain is running.

=cut

sub is_running {
    my ($self) = @_;
    
    # Use virsh domstate — it's reliable and fast
    my $output = `$self->{virsh} domstate $self->{domain} 2>/dev/null`;
    chomp $output;
    
    return ($output eq "running");
}

=head2 start

Start the domain if not running. Returns 1 on success, 0 on failure.

=cut

sub start {
    my ($self) = @_;
    
    if ($self->is_running()) {
        return 1;  # Already running, nothing to do
    }
    
    # Start the domain
    my $rc = system("$self->{virsh} start $self->{domain} >/dev/null 2>&1");
    if ($rc != 0) {
        warn "Failed to start domain: $self->{domain}";
        return 0;
    }
    
    # Wait for guest agent
    return $self->_wait_for_agent();
}

=head2 _wait_for_agent

Poll the guest agent until it responds or timeout.

=cut

sub _wait_for_agent {
    my ($self) = @_;
    my $timeout = $self->{timeout};
    my $elapsed = 0;
    
    while ($elapsed < $timeout) {
        # Use virsh qemu-agent-command with guest-info
        my $cmd = "echo '{\"execute\":\"guest-info\"}' | $self->{virsh} qemu-agent-command "
                  . "$self->{domain} 2>/dev/null";
        my $output = `$cmd`;
        
        if ($? == 0 && $output && $output !~ /DeviceNotActive/) {
            return 1;  # Agent ready
        }
        
        sleep(2);
        $elapsed += 2;
    }
    
    warn "Guest agent did not become ready within ${timeout}s";
    return 0;
}

=head2 exec

Execute a command inside the guest via guest-agent. Returns a hashref:
    { exit_code => N, stdout => "...", stderr => "...", timed_out => bool }

=cut

sub exec {
    my ($self, %args) = @_;
    my $path    = $args{path}    or die "path required";
    my $args    = $args{args}    // [];
    my $timeout = $args{timeout} // 120;
    
    my $pid = $self->_guest_exec($path, $args);
    if (!defined $pid) {
        return { exit_code => -1, stdout => "", stderr => "guest-exec failed", timed_out => 0 };
    }
    
    # Poll for completion
    my $result = $self->_guest_exec_status($pid, $timeout);
    return $result // { exit_code => -1, stdout => "", stderr => "timeout", timed_out => 1 };
}

=head2 _guest_exec

Send a guest-exec command and return the PID.

=cut

sub _guest_exec {
    my ($self, $path, $args) = @_;
    
    # Build JSON command
    my $json = {
        execute    => "guest-exec",
        arguments  => {
            path           => $path,
            capture_output => 1,
        },
    };
    
    if ( @$args ) {
        $json->{arguments}{args} = $args;
    }
    
    my $cmd_json = encode_json($json);
    
    # Send via virsh qemu-agent-command
    my $virsh_cmd = join(" ", $self->{virsh}, "qemu-agent-command",
                          $self->{domain},
                          "'" . $cmd_json . "'");
    
    my $output = `$virsh_cmd 2>/dev/null`;
    if ($? != 0 || !$output || $output =~ /DeviceNotActive/) {
        return undef;  # Agent not ready
    }
    
    # Parse PID from: {"pid":12345}
    if ($output =~ /"pid":\s*(\d+)/) {
        return int($1);
    }
    
    return undef;
}

=head2 _guest_exec_status

Poll guest-exec-status until the command completes or timeout.

=cut

sub _guest_exec_status {
    my ($self, $pid, $timeout) = @_;
    my $elapsed = 0;
    
    while ($elapsed < $timeout) {
        my $cmd_json = encode_json({
            execute   => "guest-exec-status",
            arguments => { pid => $pid },
        });
        
        my $virsh_cmd = join(" ", $self->{virsh}, "qemu-agent-command",
                              $self->{domain},
                              "'" . $cmd_json . "'");
        
        my $output = `$virsh_cmd 2>/dev/null`;
        if ($? == 0 && $output) {
            # Parse response
            my $result = decode_json($output);
            
            if (exists $result->{running} && $result->{running}) {
                sleep(1);
                $elapsed += 1;
                next;
            }
            
            # Command completed — return the result
            return {
                exit_code => $result->{exitcode} // -1,
                stdout    => $self->_decode_base64($result->{stdout} // ""),
                stderr    => $self->_decode_base64($result->{stderr} // ""),
                timed_out => 0,
            };
        }
        
        sleep(1);
        $elapsed += 1;
    }
    
    return { exit_code => -1, stdout => "", stderr => "timeout", timed_out => 1 };
}

=head2 _decode_base64

Decode base64 output from guest-agent.

=cut

sub _decode_base64 {
    my ($self, $b64) = @_;
    return "" unless $b64;
    
    # Use Perl's MIME::Base64 (core module)
    require MIME::Base64;
    MIME::Base64->import();
    return decode_base64($b64);
}

1;
```

**Usage:**
```perl
use Shmoo::VM::Runner;

my $vm = Shmoo::VM::Runner->new(domain => "windows-build", timeout => 60);

$vm->start();

my $result = $vm->exec(
    path    => "C:\\Windows\\System32\\cmd.exe",
    args    => ["/c", "echo hello"],
    timeout => 120,
);

print "Exit: $result->{exit_code}\n";
print "Output: $result->{stdout}\n";
```

---

## 9. Integration with Shmoo's Build System

### 9.1 How This Fits into Shmoo's Architecture

The VM runner module becomes a Shmoo subsystem. Every command executed in the guest carries an origin record:

```perl
package Shmoo::VM::Runner;
use Shmoo::Origin;

sub exec {
    my ($self, %args) = @_;
    
    # Record origin: the command invocation itself
    my $cmd_origin = Shmoo::Origin::Command->new(
        type    => "vm-execution",
        vm_name => $self->{domain},
        command => $args{path},
        args    => $args{args},
        timestamp => time,
    );
    
    # Execute via guest-agent
    my $result = $self->_do_exec(%args);
    
    # Attach origin to the result
    $result->{origin} = $cmd_origin;
    $result->{stdout_origin} = Shmoo::Origin::Command->new(
        type     => "vm-exec-output",
        source   => "guest-agent-output",
        parent   => $cmd_origin,
        pid      => $result->{pid},
        exitcode => $result->{exit_code},
    );
    
    return $result;
}
```

This makes every build output traceable back through the guest execution path.

### 9.2 Build Target Example

```perl
# In a Shmoo build file:
target "compile-windows" {
    my $runner = Shmoo::VM::Runner->new(domain => "windows-build");
    
    # Auto-start the VM if not running
    $runner->start();
    
    # Run the compiler
    my $build_result = $runner->exec(
        path    => "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.41.34120\\bin\\Hostx64\\x64\\cl.exe",
        args    => ["/O2", "/c", "main.c", "/Fo", "main.obj"],
        timeout => 300,
    );
    
    record_origin($build_result, "compile", "windows-build");
    
    if ($build_result->{exit_code} != 0) {
        die "Compilation failed: $build_result->{stderr}";
    }
    
    # Collect output
    my $obj_path = "/build/windows/main.obj";
    # ... download the .obj from the guest ...
}
```

### 9.3 Why This Matters

The guest agent approach is the correct choice for Shmoo because:

1. **No network dependency** — the virtio channel is internal, not network-based
2. **No authentication** — the host already owns the VM
3. **Provenance-traceable** — every command's origin is recorded (path, args, PID, exit code, stdout/stderr)
4. **Atomic execution** — `guest-exec` + `guest-exec-status` gives a clean start/finish boundary
5. **Zero user session** — runs as SYSTEM, not in a desktop session

---

## 10. Caveats and Edge Cases

### 10.1 Guest Agent Not Ready

The guest agent takes 30-60 seconds to start after boot. The `_wait_for_agent()` loop handles this by polling `guest-info`.

**Detection:** If `guest-info` returns an error, the agent is not ready. The `DeviceNotActive` error class is the specific indicator.

### 10.2 VM Crashed or Frozen

If the guest OS has frozen, the agent won't respond. There's no way to recover from a frozen guest without rebooting:

```perl
# Handle frozen guest
eval {
    my $result = $runner->exec(...);
};
if ($@ =~ /timeout|DeviceNotActive/) {
    # Guest is frozen or agent crashed — reboot the VM
    system("$self->{virsh} reboot $self->{domain}");
    $runner->start();  # Re-wait for agent
    retry_exec();      # Retry the command
}
```

### 10.3 Multiple Windows Versions

If you have multiple Windows VMs (e.g., Win10, Win11, Win2022), use separate domain names and optionally separate VM runners:

```perl
my $win10_runner  = Shmoo::VM::Runner->new(domain => "windows-10");
my $win11_runner  = Shmoo::VM::Runner->new(domain => "windows-11");
my $server_runner = Shmoo::VM::Runner->new(domain => "windows-server-2022");
```

### 10.4 Root Permissions

The libvirt system connection (`qemu:///system`) requires root or membership in the `libvirt` group:

```bash
# Add user to libvirt group
sudo usermod -a -G libvirt $USER

# Verify access
virsh list --all
```

### 10.5 Base64 Overhead

The guest agent encodes stdout/stderr as base64. This adds ~33% overhead for text output. For large outputs, consider:
- Limiting output capture to the first N kilobytes
- Using `guest-exec` without `capture_output` and writing to a file in the guest, then reading the file via `guest-fsfreeze` or SSH

### 10.6 Guest Agent Permissions

Guest-agent commands run as **SYSTEM** by default. If you need a specific user context, use one of:
- A scheduled task configured with the desired user
- The `guest-set-user-password` command followed by SSH/WinRM
- Run the build toolchain in a way that doesn't require user-level permissions

### 10.7 Shutdown After Use

To shut down the VM after all builds are done:

```bash
virsh shutdown windows-build  # Graceful (ACPI)
virsh destroy windows-build   # Force kill (like pulling the power cord)
```

In the Perl module, add a `shutdown()` method.

---

## 11. Summary

### The Three Phases, Simplified

| Phase | libvirt Method | Direct QMP Method |
|-------|---------------|-------------------|
| Detect | `virDomainGetState()` | `query-status` QMP command |
| Start | `virDomainCreate()` | `cont` QMP command (or spawn QEMU) |
| Execute | `virDomainQemuAgentCommand()` wrapping `guest-exec` | Raw JSON to QMP socket |

### Tool Summary

| Tool | What It Is | When to Use |
|------|-----------|-------------|
| `virsh` | CLI wrapper around libvirt C API | Quick admin, shell scripts |
| `libvirt.so` | C library for domain management | Shmoo's XS bindings, custom C tools |
| QMP socket | Raw JSON protocol over UNIX socket | When libvirtd is unavailable |
| `qemu-ga` | Guest agent running inside VM | Any guest execution is needed |

### C API Summary

The libvirt C API provides everything needed:

```
virConnectOpen()          → connect to hypervisor
virConnectLookupByName()  → get domain handle
virDomainGetState()       → check if running
virDomainCreate()         → start domain
virDomainQemuAgentCommand() → execute in guest via guest agent
virDomainFree()           → cleanup
virConnectClose()         → disconnect
```

All are available from XS, Inline::C, or standalone C programs. The guest agent command is the bridge between the host and the guest — it's what makes remote execution possible without any network or user authentication.