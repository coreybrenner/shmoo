# Docker and Hosted VM Transitions

**Study guide for:** Linux-to-Linux container transitions, hosted environment transitions, network-based execution  
**Related Shmoo feature:** Layer 1 Launcher, Layer 3 Stream Bridge, environment transition tracking  
**Date:** 2026-08-19

---

## Table of Contents

1. [The Problem: Transitions Between Linux Environments](#1-the-problem-transitions-between-linux-environments)
2. [Docker: The Primary Container Transition](#2-docker-the-primary-container-transition)
3. [Docker API — Programmatic Detection](#3-docker-api--programmatic-detection)
4. [Docker API — Programmatic Start](#4-docker-api--programmatic-start)
5. [Docker API — Executing Commands in Running Containers](#5-docker-api--executing-commands-in-running-containers)
6. [Docker: C-Language Integration](#6-docker-c-language-integration)
7. [SSH: The Network Transition](#7-ssh-the-network-transition)
8. [Virtual Machines: Beyond Docker](#8-virtual-machines-beyond-docker)
9. [Comparison: Docker vs SSH vs QEMU](#9-comparison-docker-vs-ssh-vs-qemu)
10. [Implications for Shmoo](#10-implications-for-shmoo)

---

## 1. The Problem: Transitions Between Linux Environments

So far we've discussed transitions from Linux → QEMU → Windows and Linux → Wine → Windows Perl. Both cross **operating system boundaries** (Linux host to guest OS).

But a major class of transitions is **between Linux environments on the same or different hosts**:

1. **Same-host transitions**: Host Linux → Docker container (same kernel, different filesystem/user namespace)
2. **Cross-host transitions**: Host Linux → SSH to remote Linux server (different kernel, different hardware)
3. **VM transitions**: Host Linux → KVM/VMware VM running Linux (different virtual hardware, possibly different kernel version)

Each transition type requires different mechanisms for:
- Detecting whether the target environment exists/is running
- Starting the target environment
- Executing commands inside it
- Capturing output and exit codes
- Preserving the configuration chain across the transition

The configuration chain problem is especially important here: the same kinds of information that cross the Linux→Windows boundary (interpreter flags, library paths, environment variables) also cross the host→container boundary (Docker environment variables, mount points, user/group mappings).

---

## 2. Docker: The Primary Container Transition

Docker is the most widely-used container runtime on Linux. It provides:
- A client API (CLI: `docker`)
- A daemon API (HTTP on UNIX socket or TCP): `/var/run/docker.sock` or `unix:///var/run/docker.sock`
- Language bindings for Python, Go, Java, C, etc.

**The Docker daemon** (`dockerd`) is the actual process management layer. All container operations go through the daemon API, regardless of whether you invoke them via CLI, SDK, or direct HTTP calls.

**Key distinction from QEMU:**
- Docker containers share the host kernel. There's no separate VM monitor socket, no guest agent.
- The daemon provides everything: detect, start, exec, log capture.
- The "guest agent" equivalent is `docker exec` — it injects commands into a running container's PID namespace.

---

## 3. Docker API — Programmatic Detection

### 3.1 Python Docker SDK (Recommended for Shmoo's Perl Integration)

Install: `pip install docker`

```python
import docker

client = docker.from_env()  # Connects via /var/run/docker.sock by default

# List all containers (running and stopped)
containers = client.containers.list(all=True)

# Find a specific container by name
def find_container(name):
    for c in containers:
        if name in c.names:
            return c
    return None

def is_running(name):
    c = find_container(name)
    return c is not None and c.status == 'running'

# Get detailed container info
def container_info(name):
    c = find_container(name)
    if c is None:
        return None
    # Reload to get full attributes
    c.reload()
    return {
        'name': c.name,
        'id': c.short_id,
        'status': c.status,      # 'running', 'exited', 'created'
        'image': c.image.tags[0] if c.image.tags else c.image.id,
        'created': c.attrs['Created'],
        'started_at': c.attrs['State'].get('StartedAt'),
        'finished_at': c.attrs['State'].get('FinishedAt'),
        'restart_count': c.attrs['RestartCount'],
        'host_config': {
            'network_mode': c.attrs['HostConfig']['NetworkMode'],
            'privileged': c.attrs['HostConfig']['Privileged'],
            'memory_limit': c.attrs['HostConfig']['Memory'],
        },
    }
```

### 3.2 Direct HTTP API Calls

The Docker daemon exposes a REST API over the UNIX socket. No Python SDK needed:

```python
import urllib.request
import json

def docker_call(method, path, data=None, timeout=30):
    """Make an API call to the Docker daemon."""
    socket_path = "/var/run/docker.sock"
    req = urllib.request.Request(
        url=f"http://localhost{path}",
        method=method,
    )
    if data:
        req.data = json.dumps(data).encode()
        req.add_header("Content-Type", "application/json")

    # Docker UNIX socket — need a custom handler
    class DockerHandler(urllib.request.BaseHandler):
        def http_open(self, req):
            sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            sock.connect(socket_path)
            return self.do_open(lambda *a, **k: sock, req)

    opener = urllib.request.build_opener(DockerHandler)
    resp = opener.open(req, timeout=timeout)
    return json.loads(resp.read()) if resp.read() else {}

# Check if container exists and get status
info = docker_call("GET", f"/v1.45/containers/{name}/json")
# Response includes 'State': {'Status': 'running'|'exited'|'created', ...}
```

**API version negotiation:** Docker API has versioned endpoints (`/v1.45/containers/...`). Use `/v1.45/version/` to get the daemon's API version and use that for all calls.

### 3.3 Detecting by Pattern

When you don't know the exact container name:

```python
def find_containers_by_label(label_key, label_value=None):
    """Find containers by label — useful for tagged builds."""
    containers = client.containers.list(all=True)
    results = []
    for c in containers:
        labels = c.attrs.get('Config', {}).get('Labels', {})
        if label_value is not None:
            if labels.get(label_key) == label_value:
                results.append(c)
        else:
            if label_key in labels:
                results.append(c)
    return results

# Example: find all containers with label "build-target=ubuntu-22"
targets = find_containers_by_label("build-target", "ubuntu-22")
```

This is more reliable than name-based detection for automated build systems.

---

## 4. Docker API — Programmatic Start

### 4.1 Starting an Existing Stopped Container

```python
def start_container(name):
    c = find_container(name)
    if c is None:
        raise ValueError(f"Container '{name}' not found")
    if c.status == 'running':
        return True  # Already running
    
    print(f"Starting container '{name}'...")
    c.start()
    return True
```

### 4.2 Creating and Starting from an Image

If no container exists yet, create one from an image:

```python
def create_and_start(name, image, command=None, volumes=None, environment=None):
    """Create a new container from an image and start it."""
    kwargs = {
        'name': name,
        'image': image,
        'detach': True,
        'auto_remove': False,  # Don't auto-remove — we need to exec into it
    }
    if command:
        kwargs['command'] = command
    if volumes:
        kwargs['volumes'] = volumes
    if environment:
        kwargs['environment'] = environment
    
    try:
        container = client.containers.run(**kwargs)
        return container
    except docker.errors.APIError as e:
        if "already in use" in str(e).lower():
            # Container exists, just start it
            return start_container(name)
        raise
```

### 4.3 Starting an Existing Container with New Configuration

You can modify an existing container's configuration and restart:

```python
def restart_with_config(name, new_config):
    """Stop, update config, start container."""
    c = find_container(name)
    if c is None:
        raise ValueError(f"Container '{name}' not found")
    
    # Stop the container
    c.stop(timeout=30)
    
    # The Docker API doesn't support updating running container config directly.
    # You must recreate the container with the new config.
    # This means: get the image, recreate with new options, remove old.
    
    image = c.image
    new_container = client.containers.create(
        image=image.id,
        name=name,
        **new_config,
    )
    new_container.start()
    c.remove()  # Remove old container
    return new_container
```

**Important:** Docker containers are immutable by design. You can't change a container's configuration after creation without recreating it. This is different from QEMU VMs, where you can modify domain XML and restart without losing state.

---

## 5. Docker API — Executing Commands in Running Containers

### 5.1 `docker exec` — The Primary Mechanism

This is the Docker equivalent of QEMU's `guest-exec`. A container must be running.

```python
def exec_in_container(name, command, user=None, workdir=None):
    """Execute a command inside a running Docker container.
    
    Returns: (exit_code, stdout, stderr)
    """
    c = find_container(name)
    if c is None:
        raise ValueError(f"Container '{name}' not found")
    
    if c.status != 'running':
        start_container(name)
        c.reload()
    
    # Execute the command
    result = c.exec_run(
        cmd=command,
        demux=True,       # Separate stdout/stderr
        stdout=True,      # Capture stdout
        stderr=True,      # Capture stderr
        workdir=workdir,  # Working directory inside container
        user=user,        # User to run as (e.g., "root" or "app")
    )
    
    exit_code = result.exit_code
    stdout = result.output[0] if result.output[0] else b""
    stderr = result.output[1] if result.output[1] else b""
    
    # Decode output
    if isinstance(stdout, bytes):
        stdout = stdout.decode('utf-8', errors='replace')
    if isinstance(stderr, bytes):
        stderr = stderr.decode('utf-8', errors='replace')
    
    return {
        'exit_code': exit_code,
        'stdout': stdout,
        'stderr': stderr,
        'running': result.exit_code is not None,
    }
```

### 5.2 Docker exec Flags and Options

| Option | Description | Default |
|--------|-------------|---------|
| `detach` | Run in background (async) | `False` (blocks until complete) |
| `demux` | Separate stdout/stderr | `False` (combined) |
| `stdin` | Attach stdin | `False` |
| `tty` | Allocate pseudo-TTY | `False` |
| `user` | Run as specific user | Container default |
| `workdir` | Working directory | Container default |
| `privileged` | Run with full privileges | `False` |
| `environment` | Set environment variables | Container default |

### 5.3 Background Execution — `docker exec` with `detach=True`

For long-running processes:

```python
def exec_background(name, command):
    """Start a long-running process inside a container.
    
    Returns: exec_id that can be used to check status.
    """
    c = find_container(name)
    result = c.exec_run(cmd=command, detach=True)
    return result.output.decode().strip()  # exec_id

def check_background_status(exec_id):
    """Check if a background exec is still running."""
    info = client.exec_inspect(exec_id)
    return {
        'running': info.get('Running', False),
        'exit_code': info.get('ExitCode'),
        'pid': info.get('Pid'),
    }
```

### 5.4 Executing a Command with Input

```python
def exec_with_input(name, command, input_data, user=None):
    """Execute with stdin input."""
    c = find_container(name)
    
    result = c.exec_run(
        cmd=command,
        stdin=True,           # Accept stdin
        demux=True,
        stdout=True,
        stderr=True,
        user=user,
    )
    
    # Send data via result.container.exec_start()
    # This is more complex — see Docker SDK docs for full example
    
    # Alternative: use 'docker exec -i' pattern
    # In the SDK, exec_run with stdin=True handles this automatically:
    exit_code, output = result.exit_code, result.output
    
    stdout = output[0] if len(output) > 0 else b""
    stderr = output[1] if len(output) > 1 else b""
    
    return {
        'exit_code': exit_code,
        'stdout': stdout.decode('utf-8', errors='replace'),
        'stderr': stderr.decode('utf-8', errors='replace'),
    }
```

### 5.5 Stream Output in Real-Time

For long-running commands, stream output:

```python
def exec_streaming(name, command):
    """Stream output from a long-running exec."""
    c = find_container(name)
    
    # Use exec_run with demux and iterate
    exit_code, stream = c.exec_run(cmd=command, demux=True)
    
    for line in stream:
        yield line
    
    return exit_code
```

---

## 6. Docker: C-Language Integration

### 6.1 libdocker — C Library for Docker API

libdocker provides a C binding for the Docker Engine API. Installation:

```bash
apt install libdocker-dev  # Available on Ubuntu/Debian
```

```c
#include <docker.h>

// Connect to Docker daemon
docker_context_t *ctx = docker_context_new();
if (!ctx) {
    // Failed to connect
}

// List containers
docker_container_list_options_t opts = { 0 };
docker_container_list_result_t *result;
docker_api_container_list(ctx, &opts, &result, NULL);

for (int i = 0; i < result->nb_containers; i++) {
    docker_container_t *c = result->containers[i];
    printf("%s: %s (%s)\n",
           c->name,
           c->state,         // "running", "exited", "created"
           c->image_name);
}

// Inspect a container
docker_container_inspect_options_t inspect_opts = { 0 };
docker_container_inspect_result_t *inspect;
docker_api_container_inspect(ctx, "container-name", &inspect_opts, &inspect, NULL);

// Check if running
int is_running = (strcmp(inspect->state->status, "running") == 0);

// Exec into a container
docker_container_exec_options_t exec_opts = { 0 };
docker_container_exec_result_t *exec_result;
const char *cmd[] = { "/bin/bash", "-c", "echo hello", NULL };
exec_opts.arguments = (char **)cmd;
exec_opts.attach_stdin = 0;
exec_opts.attach_stdout = 1;
exec_opts.attach_stderr = 1;

docker_api_container_exec(ctx, "container-name", &exec_opts, &exec_result, NULL);

// Cleanup
docker_api_container_inspect_result_free(inspect);
docker_api_container_list_result_free(result);
docker_context_free(ctx);
```

### 6.2 Direct HTTP API Calls in C

If libdocker is not available, call the Docker daemon's REST API directly:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define DOCKER_SOCKET "/var/run/docker.sock"

// Write callback for curl — captures response body
size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    char **buffer = (char **)userdata;
    *buffer = realloc(*buffer, (*buffer ? strlen(*buffer) : 0) + total + 1);
    if (*buffer) {
        strcat(*buffer, ptr);
    }
    return total;
}

// Make a Docker API call
int docker_call(const char *method, const char *path, char **response) {
    CURL *curl = curl_easy_init();
    if (!curl) return -1;
    
    char url[256];
    snprintf(url, sizeof(url), "http://localhost%s", path);
    
    // Create a UNIX socket transport
    curl_easy_setopt(curl, CURLOPT_UNIX_SOCKET_PATH, DOCKER_SOCKET);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);
    
    *response = strdup("");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    
    CURLcode res = curl_easy_perform(curl);
    long http_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    
    int rc = (res == CURLE_OK && http_code >= 200 && http_code < 300) ? 0 : -1;
    
    curl_easy_cleanup(curl);
    return rc;
}

// Check if container exists and is running
int is_container_running(const char *name) {
    char *response = NULL;
    char path[256];
    snprintf(path, sizeof(path), "/v1.45/containers/%s/json", name);
    
    int rc = docker_call("GET", path, &response);
    if (rc != 0 || !response) {
        free(response);
        return 0;  // Not found or error
    }
    
    // Parse "Status" from JSON response
    // Use a simple string search — in production, use a JSON parser
    int running = (strstr(response, "\"Status\":\"running\"") != NULL);
    
    free(response);
    return running;
}

// Example: execute a command via Docker exec
int docker_exec(const char *container, const char *command, char **stdout_out, char **stderr_out) {
    // Step 1: Create exec instance
    char *create_body = NULL;
    asprintf(&create_body,
        "{\"AttachStdout\":true,\"AttachStderr\":true,\"Cmd\":[\"/bin/sh\",\"-c\",\"%s\"]}",
        command);
    
    char *create_resp = NULL;
    char create_path[256];
    snprintf(create_path, sizeof(create_path), "/v1.45/containers/%s/exec", container);
    
    // For POST, we need to add a body — omitted for brevity
    // In production: curl_easy_setopt(curl, CURLOPT_POSTFIELDS, create_body);
    
    // Step 2: Start the exec
    // GET /v1.45/exec/{id}/start
    
    free(create_body);
    return 0;  // Simplified — see Docker SDK docs for full implementation
}
```

### 6.3 Compiling with libcurl

```bash
gcc -o docker-exec docker-exec.c -lcurl $(pkg-config --cflags --libs libcurl)
```

---

## 7. SSH: The Network Transition

Docker containers are **same-host** transitions. SSH is the primary mechanism for **cross-host** transitions:

```python
import paramiko

def ssh_exec(host, user, key_path, command, timeout=30):
    """Execute a command on a remote Linux host via SSH."""
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(host, username=user, key_filename=key_path, timeout=10)
    
    stdin, stdout, stderr = client.exec_command(command, timeout=timeout)
    exit_code = stdout.channel.recv_exit_status()
    
    return {
        'exit_code': exit_code,
        'stdout': stdout.read().decode(),
        'stderr': stderr.read().decode(),
    }
    
    client.close()
```

**Install:** `pip install paramiko`

### 7.1 SSH vs Docker for Transitions

| Aspect | Docker | SSH |
|--------|--------|-----|
| Scope | Same-host only | Any networked host |
| Overhead | Low (container shares kernel) | Low (just a shell session) |
| Isolation | Full (filesystem, network, PID namespace) | None (shared host) |
| Detecting target | Docker API, socket inspection | TCP port check, SSH banner grab |
| Starting target | `docker start` | N/A (target is always running) |
| Exec command | `docker exec` | SSH command execution |
| Config chain | Pass via `--env`, `--volume`, labels | Pass via `ssh user@host command` or environment variables |

For Shmoo's cross-host transitions, SSH is the primary mechanism. The configuration chain passes through SSH's environment variable handling and command-line argument forwarding.

---

## 8. Virtual Machines: Beyond Docker

### 8.1 KVM/libvirt — Linux VM Transitions

The same libvirt API used for QEMU Windows VMs can also manage Linux VMs. The workflow is identical:

```python
# Same libvirt API as QEMU Windows transition
dom = conn.lookupByName("linux-build-vm")
state, _ = dom.state()
if state != libvirt.VIR_DOMAIN_RUNNING:
    dom.create()

# Wait for guest agent
wait_for_guest_agent(dom, 60)

# Execute command
result = exec_in_guest(dom, path="/usr/bin/make", args=["-j4"], timeout=600)
```

### 8.2 VMware and Hyper-V

Both provide programmatic APIs for remote VM management:
- **VMware**: vSphere API (SOAP-based, Python SDK available via `pyvmomi`)
- **Hyper-V**: WMI/CIM interface, PowerShell `Get-VM`, `Invoke-VMScript`

These are beyond the scope of the current study but follow the same detect → start → exec pattern.

---

## 9. Comparison: Docker vs SSH vs QEMU

| Feature | Docker | SSH | QEMU VM |
|---------|--------|-----|---------|
| **Detection** | Docker API (`containers list`) | TCP port check, SSH banner | libvirt `domstate`, QMP `query-status` |
| **Start** | `docker start` | N/A (always running) | `virDomainCreate()`, `qemu-system` |
| **Exec** | `docker exec` | `ssh host command` | `guest-exec` via guest agent |
| **Output capture** | `exec_run().output` | `ssh stdout/stderr` | Base64 in `guest-exec-status` |
| **Background exec** | `exec_run(detach=True)` | `ssh host 'command &'` | `guest-exec` + poll `guest-exec-status` |
| **Environment vars** | `--env`, `--env-file` | SSH env vars, `SendEnv` | Guest agent `guest-set-timezone` etc. |
| **File transfer** | `docker cp`, mounted volumes | SCP, rsync | virtio-fs, NFS, shared storage |
| **Isolation level** | Full (PID, mount, net namespaces) | None | Full (virtual hardware, kernel) |
| **Overhead** | Low (no VM startup) | Low | High (VM startup 10-60s) |
| **Cross-host** | No (same host kernel) | Yes (network) | No (same host, unless remote libvirt) |
| **Same host** | Yes | N/A | Yes |

---

## 10. Implications for Shmoo

### 10.1 Unified Transition Framework

The three transition types (Docker, SSH, QEMU) share a common interface:

```python
class TransitionRunner:
    """Unified interface for all execution transitions."""
    
    def __init__(self, target, kind):
        # kind: 'docker', 'ssh', 'qemu', 'wine'
        self.target = target
        self.kind = kind
    
    def is_running(self):
        if self.kind == 'docker':
            return self._docker_is_running()
        elif self.kind == 'ssh':
            return self._ssh_is_reachable()
        elif self.kind == 'qemu':
            return self._qemu_is_running()
        raise NotImplementedError(f"Transition kind {self.kind}")
    
    def start(self):
        if self.kind == 'docker':
            return self._docker_start()
        elif self.kind == 'ssh':
            pass  # SSH targets are always running
        elif self.kind == 'qemu':
            return self._qemu_start()
    
    def exec(self, command, **kwargs):
        if self.kind == 'docker':
            return self._docker_exec(command, **kwargs)
        elif self.kind == 'ssh':
            return self._ssh_exec(command, **kwargs)
        elif self.kind == 'qemu':
            return self._qemu_exec(command, **kwargs)
    
    def exec_with_input(self, command, input_data, **kwargs):
        # Same pattern — delegate to kind-specific implementation
        ...
```

### 10.2 Configuration Chain Across Transition Types

The configuration chain needs to record:

**Docker transitions:**
- Docker image name and tag
- Environment variables passed via `--env`
- Volume mounts (`--volume`)
- Docker network configuration
- Docker labels (for build target identification)

**SSH transitions:**
- SSH host, port, user
- Authentication method (key/password/agent)
- Environment variables set by `SendEnv` or shell profiles
- SSH options (`-o StrictHostKeyChecking=no`, etc.)

**QEMU transitions:**
- VM name/UUID
- Domain XML configuration
- Guest agent status
- QEMU monitor commands issued

### 10.3 Daemon Monitoring for Containers

Docker containers can run long-lived processes. The monitoring challenge is similar to QEMU:
- Start a process in the container
- Stream its output
- Handle signals (SIGTERM for graceful shutdown)
- Detect process exit

Docker's solution is the `docker cp` and `docker exec` model combined with container restart policies (`--restart=always`). For long-running daemon processes:

```python
def run_daemon_in_container(name, command, signal=True):
    """Run a long-running process in a container and manage it."""
    c = find_container(name)
    
    # Execute with detach
    exec_id = c.exec_run(command, detach=True).output.decode().strip()
    
    # To send a signal (e.g., SIGTERM for graceful shutdown):
    if signal:
        c.kill(signal='SIGTERM')
    
    # To wait for completion:
    status = c.wait()  # Blocks until container stops or exec exits
    return status
```

### 10.4 Dockerfile Generation for Build Targets

For build-specific transitions, generate Dockerfiles dynamically:

```python
def generate_dockerfile(base_image, build_steps, name="shmoo-build"):
    """Generate a Dockerfile for a specific build target."""
    dockerfile = f"FROM {base_image}\n"
    for step in build_steps:
        if isinstance(step, str):
            # RUN command
            dockerfile += f"RUN {step}\n"
        elif isinstance(step, dict) and 'COPY' in step:
            # COPY file
            dockerfile += f"COPY {step['COPY']}\n"
    
    return dockerfile
```

### 10.5 Transition Detection Priority

For Shmoo's self-orienting behavior, detect available transition targets in order:

```python
def detect_transitions():
    """Detect what transition targets are available."""
    results = {}
    
    # Check Docker
    try:
        client = docker.from_env()
        results['docker'] = {
            'available': True,
            'containers': [c.name for c in client.containers.list(all=True)],
        }
    except:
        results['docker'] = {'available': False}
    
    # Check SSH (requires config)
    # Would check known hosts configured in shmoo-build.env
    
    # Check QEMU (same as QEMU study guide)
    results['qemu'] = detect_qemu_vms()
    
    # Check Wine (same as Wine study guide)
    results['wine'] = detect_wine()
    
    return results
```

---

## 11. Summary

### Docker: The Same-Host Container Transition

Docker is the primary mechanism for transitioning between Linux environments on the same host. It provides:

1. **Detection**: Docker SDK API (`client.containers.list()`) or daemon REST API
2. **Start**: `docker start` (stopped container) or `docker run` (new from image)
3. **Exec**: `container.exec_run()` — equivalent to QEMU's `guest-exec`
4. **Output**: `exec_run().output` — separate stdout/stderr, already decoded

The Docker SDK for Python provides the most complete API. For C integration, libdocker or direct HTTP calls to the daemon's REST API.

### Key Differences from QEMU

- **No guest agent needed** — `docker exec` is the daemon's native execution mechanism
- **No start/wait cycle** — containers are either running or not; no "guest agent not ready" polling
- **Shared kernel** — containers share the host kernel, not a separate one
- **Immutable by design** — containers can't be modified after creation; must be recreated
- **No filesystem mapping** — volumes are explicit mounts, not automatic path translation

### Transition Type Selection

| Need | Best Tool |
|------|-----------|
| Isolated Linux build on same host | Docker |
| Cross-host Linux build | SSH |
| Windows build from Linux host | QEMU VM |
| Windows Perl under Linux | Wine |

The configuration chain captures the choice, the reason, and the specific parameters of each transition. This is the data that enables the audit-grade traceability that Shmoo is designed to provide.
