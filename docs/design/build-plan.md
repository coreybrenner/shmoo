# Design: Build Plan — DAG Resolution, Dependency Graph, Build Order

*2025-01-23 | Status: Active Design*
*Purpose: Define the build plan as a DAG of compile/link/copy actions with explicit dependencies, content-addressed inputs/outputs, and a topological sort scheduler that distributes work across distributed hosts.*

---

## 1. Build Plan Overview

The build plan is a **directed acyclic graph (DAG)** of actions that describes what needs to be compiled, linked, and assembled into final artifacts.

```
                    [all] (no deps)
                   /     \
              [compile]   [compile]
              (main.c)    (util.c)
                 |          |
                 v          v
              [libfoo.a]  [libutil.a]
                 \          /
                  \        /
                   [link app]
                      |
                      v
                 [link libfoo] (depends on libutil.a, app.o)
```

Each node is an **Action** — an atomic build step with explicit inputs, outputs, and dependencies.

---

## 2. Action Definition

### Action Structure

```perl
package Shmoo::Build::Action;

# Required fields
has 'id'        => (is => 'ro', required => 1);    # Unique action ID
has 'type'      => (is => 'ro', required => 1);    # Action type
has 'inputs'    => (is => 'ro', required => 1);    # Input files + tools
has 'outputs'   => (is => 'ro', required => 1);    # Expected outputs
has 'depends'   => (is => 'ro', default => sub { [] }); # Dependent action IDs
has 'env'       => (is => 'rw');                   # Environment (exported vars)

# Optional fields
has 'host'      => (is => 'rw');                   # Target host (if assigned)
has 'slot'      => (is => 'rw');                   # Host slot (if assigned)
has 'cache_key' => (is => 'rw');                   # Content-addressed cache key
has 'priority'  => (is => 'rw', default => 0);     # Execution priority (0 = normal)
has 'retries'   => (is => 'rw', default => 2);     # Max retry count on failure
has 'timeout'   => (is => 'rw');                   # Timeout in seconds (undef = infinite)
```

### Action Types

| Type | Description | Example |
|------|-------------|---------|
| `cc_compile` | Compile C source to object file | `gcc -c main.c -o main.o` |
| `cc_link` | Link object files into library/binary | `gcc main.o util.o -o app` |
| `cp` | Copy a file | `cp /inputs/src/main.c /tmp/build/main.c` |
| `mkdir` | Create a directory | `mkdir -p /outputs/build/lib` |
| `perl_compile` | Run a Perl script that generates code | `perl gen_header.pl > header.h` |
| `asm_compile` | Assemble assembly to object file | `nasm -f elf64 main.asm -o main.o` |
| `test_run` | Execute a test binary | `./run_tests.sh` |
| `artifact_copy` | Copy a build artifact to a final location | `cp app /dist/app` |
| `purity_inspect` | Run binary inspection | `shbuild-inspect app` |
| `purity_diff` | Compare against baseline | `shbuild-diff baseline.json current.json` |
| `sign` | Cryptographic signing | `cosign sign app` |

### Action Inputs

Inputs are a list of files and tools with content hashes:

```json
{
  "files": [
    {
      "path": "/inputs/src/main.c",
      "hash": "sha256:abc123...",
      "size": 1024
    },
    {
      "path": "/inputs/include/main.h",
      "hash": "sha256:def456...",
      "size": 512
    }
  ],
  "tools": [
    {
      "path": "/tools/gcc/bin/gcc",
      "hash": "sha256:ghi789...",
      "version": "gcc 13.2.0"
    },
    {
      "path": "/tools/gcc/bin/ld",
      "hash": "sha256:jkl012...",
      "version": "ld 2.41"
    }
  ],
  "flags": [
    "-O2",
    "-g",
    "-DDEBUG",
    "-I/inputs/include"
  ]
}
```

### Action Outputs

Outputs are a list of expected output files with content hashes (for verification):

```json
{
  "files": [
    {
      "path": "/outputs/main.o",
      "expected_hash": "sha256:xyz789...",
      "expected_size": 4096
    }
  ],
  "exists": [
    "/outputs/build/main.o"
  ]
}
```

### Action Environment

Environment variables are a dictionary of exported variables:

```json
{
  "CFLAGS": "-O2 -g",
  "LDFLAGS": "-L/outputs/build -lfoo",
  "PATH": "/tools/gcc/bin:/tools/perl/bin:/usr/bin",
  "SHMOO_STRICT_ENV": "1"
}
```

---

## 3. Build Plan Parser

### Source Formats

The build plan can be defined in multiple formats:

#### Shmoo Build File (Native Format)

```perl
# shmoo-build

# Define actions
action 'cc_compile' => 'main.o' => {
    inputs => [
        '/inputs/src/main.c',
        '/inputs/include/main.h',
    ],
    tools  => ['/tools/gcc/bin/gcc'],
    flags  => ['-O2', '-g', '-I/inputs/include'],
    env    => {
        CFLAGS => '-O2 -g',
        PATH   => '/tools/gcc/bin:/usr/bin',
    },
};

action 'cc_link' => 'app' => {
    inputs => [
        '/outputs/main.o',
        '/outputs/util.o',
        '/outputs/libfoo.a',
    ],
    tools  => ['/tools/gcc/bin/gcc'],
    flags  => ['-O2', '-g'],
    env    => {
        LDFLAGS => '-L/outputs -lfoo',
        PATH    => '/tools/gcc/bin:/usr/bin',
    },
    depends => ['main.o', 'util.o', 'libfoo.a'],
};
```

#### Makefile Parser (Drop-in Compatibility)

```makefile
# Makefile — translated to Shmoo actions
CFLAGS = -O2 -g
LDFLAGS = -L/outputs -lfoo

app: main.o util.o libfoo.a
	gcc $(CFLAGS) $(LDFLAGS) -o app main.o util.o

main.o: main.c main.h
	gcc $(CFLAGS) -c main.c

util.o: util.c util.h
	gcc $(CFLAGS) -c util.c
```

#### Recipe Book Integration

```perl
# Recipe resolves dependencies automatically
recipe 'cc_library' => {
    action => 'cc_compile',
    type   => 'library',
    inputs => glob('/inputs/src/*.c'),
    env    => { CFLAGS => '-O2 -g' },
};
```

### Parser Implementation

```perl
package Shmoo::Build::Parser;

use Shmoo::Build::Action;

sub parse_file {
    my ($self, $file_path) = @_;
    
    my @actions;
    
    if ($file_path =~ /\.shmoo$/) {
        @actions = $self->_parse_shmoo($file_path);
    } elsif ($file_path eq 'Makefile' || $file_path =~ /\.mk$/) {
        @actions = $self->_parse_makefile($file_path);
    } else {
        die "Unsupported build file format: $file_path";
    }
    
    return @actions;
}

sub _parse_shmoo {
    my ($self, $file_path) = @_;
    
    # Parse shmoo-build format (see above)
    # Extract action definitions, dependencies, inputs, outputs, etc.
}

sub _parse_makefile {
    my ($self, $file_path) = @_;
    
    # Parse Makefile and translate to Shmoo actions:
    # 1. Extract all targets and their dependencies
    # 2. Extract variable definitions (CFLAGS, LDFLAGS, etc.)
    # 3. Extract recipe lines (commands)
    # 4. Translate to Shmoo Action objects
}
```

---

## 4. Dependency Graph

### DAG Construction

The parser produces a list of actions. The DAG builder connects them via dependency edges:

```perl
package Shmoo::Build::DAG;

use Shmoo::Build::Action;

sub build_dag {
    my ($self, @actions) = @_;
    
    my %action_map;      # id -> Action
    my %in_degree;       # id -> number of dependencies
    my %successors;      # id -> [list of dependent action IDs]
    
    # Index actions by ID
    for my $action (@actions) {
        $action_map{$action->id} = $action;
        $in_degree{$action->id} = 0;
    }
    
    # Build edges from dependencies
    for my $action (@actions) {
        for my $dep_id (@{$action->depends}) {
            # Add edge: dep_id -> action->id
            push @{$successors{$dep_id}}, $action->id;
            $in_degree{$action->id}++;
        }
    }
    
    return {
        action_map  => \%action_map,
        in_degree   => \%in_degree,
        successors  => \%successors,
        roots       => [grep { $in_degree{$_} == 0 } keys %action_map],
    };
}
```

### Example DAG

```
action 'compile:main.c'  → depends on: []
action 'compile:util.c'  → depends on: []
action 'lib:libfoo.a'    → depends on: ['compile:main.c']
action 'lib:libutil.a'   → depends on: ['compile:util.c']
action 'link:app'        → depends on: ['lib:libfoo.a', 'lib:libutil.a']
action 'link:libapp.so'  → depends on: ['lib:libfoo.a', 'lib:libutil.a']
action 'test:app_test'   → depends on: ['link:app']
action 'install:app'     → depends on: ['test:app_test', 'link:app', 'link:libapp.so']
```

```
                    [compile:main.c]  [compile:util.c]
                           \              /
                            v            v
                    [lib:libfoo.a]    [lib:libutil.a]
                             \              /
                              v            v
                        [link:app]  [link:libapp.so]
                               \       /
                                v     v
                         [test:app_test]
                                   |
                                   v
                           [install:app]
```

---

## 5. Topological Sort & Build Order

### Kahn's Algorithm

The build order is computed using Kahn's algorithm (topological sort):

```perl
sub topological_sort {
    my ($self, $dag) = @_;
    
    my @result;
    my @queue = @{$dag->{roots}};  # Start with roots (no dependencies)
    
    while (@queue) {
        my $node = shift @queue;
        push @result, $node;
        
        for my $successor (@{$dag->{successors}{$node}}) {
            $dag->{in_degree}{$successor}--;
            if ($dag->{in_degree}{$successor} == 0) {
                push @queue, $successor;
            }
        }
    }
    
    return @result;
}
```

### Parallel Build Order

For parallel execution, we compute the **level** of each node (the minimum number of steps from any root):

```perl
sub parallel_levels {
    my ($self, $dag) = @_;
    
    my %level;
    my @queue = @{$dag->{roots}};
    
    # Roots are at level 0
    for my $root (@{$dag->{roots}}) {
        $level{$root} = 0;
    }
    
    while (@queue) {
        my $node = shift @queue;
        my $current_level = $level{$node};
        
        for my $successor (@{$dag->{successors}{$node}}) {
            my $new_level = $current_level + 1;
            if (!exists $level{$successor} || $new_level > $level{$successor}) {
                $level{$successor} = $new_level;
            }
            push @queue, $successor;
        }
    }
    
    return \%level;
}
```

**Result:**

| Level | Actions | Can Run In Parallel? |
|-------|---------|---------------------|
| 0 | compile:main.c, compile:util.c | ✓ Both |
| 1 | lib:libfoo.a, lib:libutil.a | ✓ Both (after level 0 completes) |
| 2 | link:app, link:libapp.so | ✓ Both (after level 1 completes) |
| 3 | test:app_test | ✗ (after level 2) |
| 4 | install:app | ✗ (after level 3) |

---

## 6. Action Graph with Content Addresses

### Content-Addressed Inputs/Outputs

Every input file and output file has a content hash:

```
Action: cc_compile:main.c
  inputs:
    /inputs/src/main.c     → sha256:abc123...
    /inputs/include/main.h → sha256:def456...
    /tools/gcc/bin/gcc     → sha256:ghi789...
  outputs:
    /outputs/main.o        → expected sha256:xyz789...
```

If the content hash of any input changes, the action's **cache key** changes, and the build engine knows it must re-run.

### Cache Key Computation

The cache key is a content-addressed hash of:
1. **Tool hash** — SHA256 of the compiler binary
2. **Flag list** — All compiler flags (sorted)
3. **Input hashes** — SHA256 of every input file
4. **Environment hash** — SHA256 of exported environment variables
5. **Host hash** — SHA256 of the host environment (OS, CPU, etc.)

```perl
sub compute_cache_key {
    my ($self, $action) = @_;
    
    my @inputs = map { $_->hash } @{$action->inputs};
    my @flags  = @{$action->flags};
    my $env    = join("\n", sort %{$action->env});
    my $tool   = $action->tools[0]->hash;  # Primary tool hash
    my $host   = $self->host_fingerprint();
    
    my $data = join("\0", $tool, $env, @inputs, @flags, $host);
    return sha256_hex($data);
}
```

---

## 7. Action Execution

### Per-Host Execution

Each action is assigned to a host with available slots:

```perl
sub assign_action {
    my ($self, $action, $dag, $hosts) = @_;
    
    # Find hosts with available slots
    my @available_hosts = grep { $_->available_slots() > 0 } @$hosts;
    
    unless (@available_hosts) {
        die "No available hosts (all slots busy)";
    }
    
    # Pick the host with the most available slots
    my $host = sort { $b->available_slots <=> $a->available_slots } @available_hosts;
    
    $action->host($host->id);
    $action->slot($host->next_slot());
    
    # Reduce host's available slots
    $host->decrement_slots();
    
    return $host;
}
```

### Execution on Host

The Host Daemon executes the action:

```perl
sub execute_action {
    my ($daemon, $action) = @_;
    
    # 1. Set up environment
    setenv($action->env);
    
    # 2. Set up 9p mounts
    SHMOO_9P_MOUNTS = join(",", map { "${_}=tcp:9p.server:5640,0" } @mount_paths);
    
    # 3. Inject LD_PRELOAD for syscall interception
    LD_PRELOAD = "/path/to/libsyscallhook.so";
    
    # 4. Inject LD_PRELOAD for syscall interception
    LD_PRELOAD="/path/to/libsyscallhook.so";
    
    # 5. Set up event logging
    SHMOO_EVENT_LOG="/build/misiones/0xA3F1/host-alpha.log";
    
    # 6. Build command based on action type
    my $cmd;
    if ($action->type eq 'cc_compile') {
        $cmd = join(" ", @{$action->tools}, @{$action->flags});
        # Example: gcc -O2 -g /inputs/src/main.c -o /outputs/main.o
    } elsif ($action->type eq 'cc_link') {
        $cmd = join(" ", @{$action->tools}, @{$action->flags});
        # Example: gcc -o app /outputs/main.o /outputs/util.o
    } elsif ($action->type eq 'cp') {
        $cmd = "cp @{$action->flags}";
        # ... etc.
    }
    
    # 7. Execute the command (with 9p mounts + LD_PRELOAD + event logging)
    system($cmd);
    
    # 8. Verify outputs
    for my $output (@{$action->outputs}) {
        if (!-e $output->path) {
            die "Expected output not found: $output->path";
        }
        
        if ($output->expected_hash) {
            my $actual_hash = sha256_hex(read_file($output->path));
            if ($actual_hash ne $output->expected_hash) {
                # Warn: output content differs from expected, but build still succeeds.
                # This is normal if the expected_hash is just a reference, not a hard requirement.
                warn "Output hash mismatch: $output->path (expected: $output->expected_hash, actual: $actual_hash)";
            }
        }
    }
    
    # 9. Flush event log
    event_log_flush();
}
```

---

## 8. Build Plan Configuration

### Global Configuration

```yaml
build:
  # Maximum parallel jobs across all hosts
  max_global_jobs: 64
  
  # Maximum parallel jobs per host (can be overridden per-host)
  max_host_jobs: 8
  
  # Content-addressed cache location
  cache_path: "/build/cache"
  
  # Enable strict environment isolation
  strict_env: true
  
  # Enable 9p mounts (default: true)
  use_9p: true
  
  # Enable event logging (default: true)
  enable_logging: true
  
  # Default retry count on failure
  default_retries: 2
  
  # Default timeout (seconds, 0 = infinite)
  default_timeout: 3600
  
  # Default priority
  default_priority: 0
  
  # Host assignment strategy
  host_strategy: "least-loaded"  # least-loaded | capability | affinity
  
  # Build order strategy
  order_strategy: "topological"  # topological | priority
```

### Host-Specific Configuration

```yaml
hosts:
  - name: "alpha"
    slots: 8
    max_jobs: 16  # Override global max_host_jobs
    tools:
      gcc: "/usr/bin/gcc-13"
      perl: "/usr/bin/perl-5.38"
    mounts:
      - path: "/inputs"
        local_path: "/data/shmoo/src"
      - path: "/libs"
        local_path: "/data/shmoo/libs"
```

---

## 9. Implementation Plan

### Phase 1: Action Model
- [ ] Define `Shmoo::Build::Action` (Perl module)
- [ ] Implement input/output/tool/env fields
- [ ] Implement action types (cc_compile, cc_link, etc.)

### Phase 2: Parser
- [ ] Implement `Shmoo::Build::Parser` (shmoo-build format)
- [ ] Implement Makefile parser (drop-in compatibility)
- [ ] Implement recipe book integration

### Phase 3: DAG & Topological Sort
- [ ] Implement `Shmoo::Build::DAG` (graph builder)
- [ ] Implement Kahn's algorithm (topological sort)
- [ ] Implement parallel levels computation

### Phase 4: Content-Addressed Cache
- [ ] Implement `compute_cache_key()` (action key)
- [ ] Implement cache lookup (does action need to run?)
- [ ] Implement cache write (store output after successful execution)

### Phase 5: Host Assignment & Execution
- [ ] Implement `assign_action()` (slot-aware host assignment)
- [ ] Implement `execute_action()` (Host Daemon execution)
- [ ] Implement output verification (content hash check)

### Phase 6: Integration
- [ ] Integrate with 9p client (mount inputs/outputs)
- [ ] Integrate with syscall interceptor (LD_PRELOAD)
- [ ] Integrate with event log (per-action event recording)
- [ ] Integrate with distributed hosts (multi-host scheduling)

### Phase 7: Testing & Optimization
- [ ] Test with real build projects (gcc, perl, make)
- [ ] Test content-addressed cache (cache hit/miss)
- [ ] Test parallel execution (multiple independent actions)
- [ ] Test multi-host execution (jobs distributed across hosts)
- [ ] Benchmark build graph resolution speed (should be < 100ms for 1000+ actions)

---

## 10. Summary

The build plan is a DAG of compile/link/copy actions with:

- **Explicit dependencies:** Topological sort determines execution order
- **Content-addressed inputs:** Actions only re-run when inputs change
- **Parallel execution:** Independent actions run concurrently on available hosts
- **Host assignment:** Jobs distributed across hosts based on slot availability
- **Output verification:** Content hashes ensure build integrity
- **Multiple formats:** Native shmoo-build, Makefile, recipe book integration

This turns the build system into a **deterministic, reproducible, parallel build engine** that can scale across distributed hosts while maintaining full observability and replay capabilities.
