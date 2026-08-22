# Design: Advanced Delusion Shell & Reactive Build Graph

*2025-01-23 | Status: Active Design*
*Purpose: The "one-command" is an editable file on the Virtual Execution Path (VXP), not a REPL history entry. The build engineer works across many levels of the environment. Variable changes are traced through the entire process stack — from shell variables in the third recursive invocation of Make down to the compiler — and the integrated total build graph reflects ramifications visually in real-time (nodes turn red as changes propagate).*

---

## 1. The Virtual Execution Path (VXP) — The "Command" File

The failing command is **not** a volatile REPL history entry. It is a **real file** on the virtual execution path — a Makefile target (or equivalent build rule) that captures the exact invocation that failed.

### The VXP File
The file lives at a virtual execution path, e.g., `/shmoo/vxp/build-target.mk`:

```makefile
# VXP — Virtual Execution Path
# This file captures the exact build state at the moment of failure.

# --- Environment Overrides (Editable) ---
export CFLAGS  = -O2 -DDEBUG -g
export LDFLAGS = -L/mnt/vol/libs

# --- Build Rule (Editable) ---
build-app: $(OBJDIR)/app.bin
	$(CC) $(CFLAGS) $(LDFLAGS) -o app app.c
```

The engineer:
- Opens the file in their editor (via the delshell)
- Edits variables, rules, includes, shell functions
- Saves the file to the **overlay filesystem**

The file persists, can be versioned, diffed, and is the engineer's primary interface to the build state — not a REPL prompt.

---

## 2. Reactive Build Graph — Visual Propagation of Changes

The build graph is **reactive**. It is not a static DAG; it is a live, monitored view of the entire build state.

### Real-Time Tracing
When the engineer edits a variable in the VXP file, Shmoo's Perl wrappers (e.g., `Shmoo::Tool::Make`, `Shmoo::Tool::Gcc`) parse the file, determine which downstream actions depend on the changed variable, and update the **Build Graph UI** in real-time.

### The Visual Ripple — "Turns Red"
- **Yellow nodes:** Actions that will be re-evaluated because they depend on the changed variable
- **Red nodes:** Actions that are now **broken** (new variable value causes a compilation error, a missing dependency, etc.)
- **Green nodes:** Unaffected actions

**Visual Example:**
```
[ Root: all ] (Green)
    |
    v
[ compile:base.c ] (Yellow - depends on CFLAGS)
    |
    v
[ link:base.o ] (Yellow - depends on compile:base.c)
    |
    v
[ compile:app.c ] (Red - BROKEN: new -O3 conflicts with -DDEBUG)
```

The engineer sees the ramifications of their change *before* re-running the build. They tweak the variable, save the file, and watch the graph update node-by-node.

---

## 3. Deep Recursion Tracing — The Stack of Processes

This is the core capability: **tracing a variable change across the entire process stack**, including multiple recursive invocations of Make, shell subshells, and Perl wrappers.

### Multi-Level Tracing

When the engineer changes a variable in the **third recursive invocation of Make**, the tracer follows the execution stack:

1. **Shell Level:** Traces `$PATH`, `$HOME`, shell variables set by the delshell
2. **Make Level (1st recursion):** Traces `$(CFLAGS)`, `export` statements in the root Makefile
3. **Make Level (2nd recursion):** Traces deeper variable scopes in sub-Makefiles
4. **Make Level (3rd recursion):** Traces the *actual change point* — where the engineer modified the variable
5. **Perl Wrapper Level:** Traces variables inside `Shmoo::Tool::Make`, `Shmoo::Tool::Gcc`, etc.
6. **Compiler Level:** Traces internal compiler macros and flag processing

At each level, the tracer checks:
- Is this variable read by a downstream action?
- Does the new value cause a downstream action to fail?
- Does the new value produce a binary that fails the purity check?

### Upward and Downward Propagation
Changes propagate **both ways**:
- **Downward (red ripple):** The variable change affects all child nodes that depend on it. If a child breaks, it turns red.
- **Upward (back-propagation):** If a child node breaks, the parent node also turns red (because the parent cannot complete successfully).

### Example: Third Recursion of Make

```bash
# Engineer changes a variable in the 3rd recursion of Make:
# File: /shmoo/vxp/subdir/Makefile (3rd recursion of Make)
# Line: export LDFLAGS = -L/new/path  (was -L/old/path)

# The tracer follows:
# 1. 3rd recursion → 2nd recursion → 1st recursion → root Make
# 2. Downward: all actions that read LDFLAGS are updated
# 3. Downward: broken nodes turn RED across the entire project graph
# 4. Upward: all parent nodes that depend on broken children also turn RED
```

The engineer sees **every** place where their change causes a problem — not just in the local directory, but across the entire build tree, at every level of the process stack.

---

## 4. Integrated Total Build Graph

The "integrated total build graph" is a single, global DAG that contains **all** build actions across the entire project — not just the local scope.

### Global State Awareness
The build engine maintains a global registry of all actions and their dependencies. When the engineer modifies the VXP file:
1. The engine parses the file and extracts all variable changes
2. For each variable change, the engine traces all downstream and upstream nodes
3. The engine highlights affected nodes in the UI (yellow → red)
4. If the engineer changes a variable that breaks a node elsewhere in the tree, that node turns red too

This allows the engineer to see if their fix for one failure introduces a new failure elsewhere in the project.

### Scope Refunding
Once the engineer is happy with the fix (and the graph turns green for all affected nodes), they can **Refund the Scope**:
1. **Archive:** The old, broken artifacts are archived
2. **Delete:** The current overlay is cleared
3. **Restart:** The build restarts from the last successful checkpoint, with the engineer's changes applied

---

## 5. Implementation: The VXP Editor Workflow

The delshell provides a seamless editing experience:

```bash
# Enter delshell after build failure:
$ shbuild dev --action compile:app.c

# Inside the Delusion Shell:
$ ls -la /shmoo/vxp/
build-target.mk  (VXP file)

# Open the VXP in the engineer's preferred editor:
$ edit /shmoo/vxp/build-target.mk

# Engineer makes changes:
#   export CFLAGS = -O3 -DDEBUG -g  (Modified)
#   $(CC) $(CFLAGS) -o app app.c    (Modified)

# Engineer saves and exits the editor.
# The overlay filesystem updates.
# The Build Graph UI updates (Yellow/Red ripple across the whole project).

# Engineer is happy with the result (Green ripple).
$ shbuild refund --action compile:app.c
# Restarts build with changes applied.
```

---

## 5. Interactive Graph UI — Visual Parameter Tracing

The build graph is not just a passive visualization; it is an **active interface** for exploring the build environment.

### The "Glass Box" Concept
Instead of just seeing a static map, the engineer can **interact with the graph itself**:
- **Grab a Node:** Click and expand a node (e.g., a specific compiler invocation or a Make rule).
- **Open Up Parameters:** View the internal variables, flags, and dependencies of that node.
- **Mess Around:** Change a variable directly in the UI (e.g., tweak `CFLAGS` from `-O2` to `-O3`).
- **Watch the Ripple:** The graph immediately highlights all affected nodes **downstream** (in the subtree) based on the variable change.

### Instructional Mode
This is "incredibly instructive" because it visualizes the **causal chain** of the build:
- **Highlighting:** Affected nodes light up (yellow/red) as the engineer changes the parameter.
- **Explanation:** The UI annotates *why* a change propagated ("Node X changed because `CFLAGS` depends on Variable Y").
- **Safe Exploration:** The engineer can "mess around" in a sandbox mode without committing the changes.

### Visualizing the Stack
Because the graph represents the **entire stack** (shell → Make → Perl → compiler), changing a variable at any level instantly shows the blast radius across all levels.

This turns the build graph into a **dependency simulator**, making the invisible structure of the build visible and interactive.

---

## 6. Summary

This advanced Delusion Shell is a **reactive, visual build IDE**:
- **VXP:** An editable file on the virtual execution path that captures the failure context
- **Visual Graph:** A live, color-coded map of the entire build state
- **Ripple Effect:** Real-time propagation of variable changes through the entire process stack (shell → Make → Perl wrappers → compiler)
- **Deep Recursion:** Traces changes across multiple recursive invocations of build tools, down the entire stack
- **Global Awareness:** The system knows how a local change affects every node in the total build graph
- **Scope Refunding:** A clean way to commit changes and re-run the build

This makes Shmoo a true **build environment** that the engineer can explore, modify, and understand visually — not just a black box that fails and succeeds.

