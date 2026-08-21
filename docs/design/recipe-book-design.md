# Design: Shmoo::Transitions — Recipe Book

*2025-01-23 | Status: Concept Design*
*Purpose: Recipe-based transition system with auto-upgrade, community contribution, and dependency resolution.*

---

## Vision

A builder receives a request: "I need to transition from a Mac host to a Mac guest running
in Parallels, which is running an experimental version of WINE. I am running a rigorous
test suite, and my goal is to improve the capabilities of WINE."

The builder should:
1. Detect that the current environment (Mac host) does not match the target (WINE guest).
2. Check its recipe book for a transition recipe matching this source→target pair.
3. If no recipe exists, attempt to auto-upgrade the recipe book from CPAN.
4. If the recipe still doesn't exist, fall back to asking the user or using a generic path.
5. Once a recipe is found (or installed), download the appropriate CPAN modules for
   transitioning.
6. Load the recipe, execute the transition.
7. After the transition, the builder is running in the target environment (e.g., WINE on
   Parallels), with all necessary tools installed and configured.

This should work for any transition: Cygwin→Strawberry, Linux→AWS, Mac→WINE, bare metal→VM.

---

## Core Design

### Module: Shmoo::Transitions::RecipeBook

The recipe book is the central registry. It:
- Maps environment pairs (source→target) to recipe module names.
- Auto-upgrades by checking CPAN for newer recipes.
- Loads recipes on demand.
- Validates recipes before execution.

```
Recipe Book:
├── Environment Registry
│   ├── cygwin → { 'wine-strawberry': 'Shmoo::Transitions::Recipe::CygwinToWineStrawberry' }
│   ├── linux → { 'aws': 'Shmoo::Transitions::Recipe::LinuxToAWS' }
│   └── mac → { 'parallels-wine': 'Shmoo::Transitions::Recipe::MacToParallelsWine' }
├── Auto-Upgrader
│   ├── Checks CPAN for newer recipe versions.
│   ├── Installs missing recipes.
│   └── Reloads recipe registry after upgrade.
└── Recipe Loader
    ├── Loads a recipe module.
    ├── Validates its metadata.
    └── Returns a recipe object.
```

### Recipe Format

Each recipe is a Perl module with a standard interface:

```perl
package Shmoo::Transitions::Recipe::CygwinToWineStrawberry;

# Required metadata
use Shmoo::Transitions::Recipe::Base;

our $ENV_FROM = 'cygwin';
our $ENV_TO   = 'wine-strawberry';
our $VERSION  = '0.01';

# Recipe phases (all optional, but should be implemented)
sub preflight { ... }    # Validate prerequisites.
sub prepare   { ... }    # Install/ensure dependencies.
sub migrate   { ... }    # Execute the transition.
sub validate  { ... }    # Verify transition succeeded.
sub cleanup   { ... }    # Clean up intermediate state.

# Phase implementations:
#   preflight(): Check that target exists, is reachable, has required capabilities.
#     Returns: (ok|fail, message).
#
#   prepare(): Install missing tools (Perl modules, binaries, config files).
#     Downloads and installs CPAN modules as needed.
#     Returns: (ok|fail, message).
#
#   migrate(): Actually perform the transition (copy files, run scripts, etc.).
#     Returns: (ok|fail, message).
#
#   validate(): Run test suite or other validation to confirm transition.
#     Returns: (ok|fail, message).
#
#   cleanup(): Remove temporary artifacts.
#     Returns: (ok|fail, message).
```

### Auto-Upgrader

```perl
sub upgrade_recipe_book {
    my $recipe = shift;
    
    # Check if recipe module is installed.
    unless (module_installed($recipe)) {
        # Attempt to install from CPAN.
        system("cpan install $recipe");
        
        # If CPAN doesn't have it, try GitHub mirror.
        unless (module_installed($recipe)) {
            system("cpan install Shmoo::Transitions::$recipe");
        }
        
        # Still not found? Log error and return false.
        return module_installed($recipe) ? 1 : 0;
    }
    
    return 1;
}
```

### Dependency Resolution

Recipes can depend on other recipes (e.g., "Install Strawberry Perl" is a dependency
of "Cygwin to Wine-Strawberry"). The recipe book resolves dependencies recursively:

```
Recipe: CygwinToWineStrawberry
  DependsOn:
    - InstallStrawberryPerl
    - ConfigureWINE
    - CopyBuildArtifacts
  Each depends on other recipes in the registry.
```

---

## Usage Scenario

### Scenario 1: "I need to go from Cygwin to WINE-hosted Strawberry"

```perl
use Shmoo::Transitions::RecipeBook;

my $rb = Shmoo::Transitions::RecipeBook->new;

# Request transition: cygwin → wine-strawberry
my $recipe = $rb->find_recipe('cygwin', 'wine-strawberry');

# No recipe found? Auto-upgrade!
unless ($recipe) {
    my $success = $rb->upgrader->upgrade('cygwin', 'wine-strawberry');
    next unless $success;
    $recipe = $rb->find_recipe('cygwin', 'wine-strawberry');
}

# Execute the recipe
$recipe->preflight() or die "Preflight failed";
$recipe->prepare()   or die "Prepare failed";
$recipe->migrate()   or die "Migrate failed";
$recipe->validate()  or die "Validation failed";
$recipe->cleanup();

# Now running in wine-strawberry environment
```

### Scenario 2: "New transition: Mac host → Parallels guest with WINE"

1. Dev says: "I need to transition from Mac host to a Mac guest in Parallels with WINE."
2. Builder's recipe book has no such recipe.
3. Upgrader checks CPAN — no recipe exists.
4. Builder reports: "No recipe available for Mac→Parallels-WINE. Please contribute one."
5. Subject matter expert creates a recipe: `Shmoo::Transitions::Recipe::MacToParallelsWine`.
6. Recipe is published to CPAN (or GitHub repo that feeds CPAN).
7. Next time, builder's upgrader finds it, installs it, and the transition works.

### Scenario 3: "Auto-upgrade the recipe book"

```perl
use Shmoo::Transitions::RecipeBook;

my $rb = Shmoo::Transitions::RecipeBook->new;

# Check for newer recipe versions on CPAN.
my $upgrades = $rb->upgrader->check_updates;

if (@$upgrades) {
    print "Available upgrades:\n";
    for my $u (@$upgrades) {
        print "  $u->{module} v$u->{current} → v$u->{latest}\n";
    }
    
    $rb->upgrader->apply_updates;
    $rb->reload_registry();  # Refresh the recipe cache.
}
```

---

## Directory Layout

```
Shmoo/Transitions/
├── RecipeBook.pm          # Core registry, upgrader, loader
├── Recipe/
│   ├── Base.pm            # Base class for all recipes
│   ├── CygwinToWineStrawberry.pm  # Example recipe
│   ├── LinuxToAWS.pm
│   ├── MacToParallelsWine.pm  # Community-contributed recipe
│   └── ...
├── Upgrader.pm            # Auto-upgrade from CPAN/GitHub
├── Validator.pm           # Recipe validation (schema, tests)
└── Registry.pm            # Serialized recipe registry
```

---

## External Dependencies & Overrides

A recipe is useless if the external tools it depends on (e.g., a compiler, a specific version of Perl) are broken or leaky. The recipe book must account for "host impurities" where the target environment's default toolchain fails a purity check.

### The Problem with "System" Externals
The default installation mechanism of an OS is not always reliable.
* **Example:** The `wine-strawberry` transition recipe needs a working Perl interpreter.
* **The Failure Mode:** If the recipe blindly relies on the host's default `apt install perl` or a registry-installed ActiveState Perl, it might inherit a build that leaks registry paths or Windows system clock timestamps into the binary.
* **The Solution:** The recipe must have the capability to override the default installation. In this case, the recipe should forcibly install a known-working distribution (e.g., Strawberry Perl, which handles Windows registry paths and timestamps correctly) into a sandboxed or private path (e.g., `/mnt/tools/perl`), effectively bypassing the broken host default.

### Accounting for External Development
The build system's "pure" internal logic (the compiler driver, the inspector) is one thing. The "bridge" logic—the code that ensures the external host tools are actually working as expected—is another.
* **Effort:** Significant effort will be required to audit and "certify" common external dependencies.
* **Recipe Book Extension:** The recipe book needs a concept of a "Certified External."
    * `Recipe::External::StrawberryPerl` — A known-good version of Strawberry Perl, pre-verified by Shmoo.
    * `Recipe::External::SystemPerl` — A placeholder for the system default (unverified).

### Implementation Requirement
The recipe's `prepare()` phase must support a **Force Override** pattern:
```perl
sub prepare {
    my ($self, $ctx) = @_;
    
    # Check if the system perl works.
    if ($ctx->host->is_broken('perl')) {
        # Override: Install our known-good version.
        $self->install_known_good('Shmoo::External::StrawberryPerl');
    }
}
```

This is where the Shmoo business model lives. We provide the *Certified Externals* (the "overriding" toolchains). Anyone can try the system defaults for free, but they'll hit impurities. The paying customer gets the Shmoo-certified external that overrides the system's broken defaults.

---

## Key Design Decisions

### 1. Recipes Are CPAN Modules

**Rationale:**
- Versioned and installable with standard tooling.
- Anyone can contribute by submitting to CPAN.
- Auto-upgrade is trivial: `cpan install Shmoo::Transitions::Recipe::X`.
- Recipes are just Perl code — flexible, no schema overhead.

### 2. Recipe Book Is the Central Registry

**Rationale:**
- Maps transitions to recipe modules by name.
- Auto-upgrades by checking CPAN for newer recipes.
- Reloadable without restarting the builder.

### 3. Recipes Have Standard Phases

**Rationale:**
- Consistent interface across all recipes.
- Phases are optional (a recipe can override only what it needs).
- Phases are: preflight, prepare, migrate, validate, cleanup.

### 4. Dependency Resolution Is Recursive

**Rationale:**
- Complex transitions have dependencies (e.g., "Install Strawberry" is needed by "Cygwin→Wine").
- Recipe Book resolves dependencies by looking up each dependency in the registry.
- Circular dependencies are detected and rejected.

### 5. Recipes Can Be Contributed by the Community

**Rationale:**
- CPAN is an open, public repository.
- Anyone can submit a recipe via CPAN.
- Recipes are just Perl modules — familiar to Perl developers.
- Subject matter experts can contribute their knowledge directly.

### 6. Builder Is Stateful

**Rationale:**
- Builder maintains state across phases (e.g., what was installed, what was copied).
- State is stored in a JSON file between runs.
- On resume, builder checks state and skips completed phases.

---

## Example: Cygwin → WINE-Strawberry Recipe

```perl
package Shmoo::Transitions::Recipe::CygwinToWineStrawberry;

use Moo;
extends 'Shmoo::Transitions::Recipe::Base';

sub _env_from { 'cygwin' }
sub _env_to   { 'wine-strawberry' }

sub preflight {
    my ($self) = @_;
    
    # Check that WINE is installed on target.
    my $wine_ver = system('wine --version');
    if ($wine_ver != 0) {
        return (fail, "WINE not installed on target");
    }
    
    # Check that Strawberry Perl is accessible.
    my $perl_ver = system('perl --version');
    if ($perl_ver != 0) {
        return (fail, "Strawberry Perl not accessible on target");
    }
    
    return (ok, "Target ready for transition");
}

sub prepare {
    my ($self) = @_;
    
    # Ensure Strawberry Perl CPAN modules are installed.
    system("cpanm Test::More IO::All");
    
    # Ensure WINE tools are installed.
    system("winecfg");
    
    return (ok, "Dependencies installed");
}

sub migrate {
    my ($self) = @_;
    
    # Copy build artifacts from Cygwin to WINE host.
    system('rsync -av /mnt/vol/ /mnt/tools/');
    
    # Configure build environment.
    system('echo "export PATH=/c/Strawberry/perl/bin:$PATH" > /mnt/tools/.bashrc');
    
    return (ok, "Migration complete");
}

sub validate {
    my ($self) = @_;
    
    # Run the test suite to confirm transition.
    my $result = system('perl t/test-suite.t');
    if ($result == 0) {
        return (ok, "Validation passed");
    } else {
        return (fail, "Validation failed (exit code: $result)");
    }
}

sub cleanup {
    my ($self) = @_;
    
    # Remove temporary build artifacts.
    system('rm -rf /mnt/tools/build-temp/');
    
    return (ok, "Cleanup complete");
}

1;
```

---

## CPAN Module: Shmoo::Transitions::RecipeBook

This is the main public interface:

```perl
use Shmoo::Transitions::RecipeBook;

# Create a new recipe book (loads current registry).
my $rb = Shmoo::Transitions::RecipeBook->new;

# Find a recipe for a transition.
my $recipe = $rb->find_recipe('cygwin', 'wine-strawberry');

# Upgrade the recipe book (auto-installs from CPAN).
$rb->upgrader->upgrade();

# Reload the registry (re-read from disk).
$rb->reload_registry();

# List all available recipes.
my @recipes = $rb->list_recipes();

# Get recipe dependencies.
my @deps = $recipe->dependencies();

# Execute a recipe.
$recipe->run();
```

---

## Future Enhancements

1. **Recipe Signing:** Recipes can be signed with a GPG key to prevent tampering.
2. **Recipe Versioning:** Recipes are versioned; the recipe book can pin specific versions.
3. **Recipe Sandboxing:** Recipes run in a sandbox to prevent malicious code.
4. **Recipe Testing:** CI tests each recipe against a real environment before publishing.
5. **Recipe Marketplace:** A website where users can browse and install recipes.
6. **Recipe Templates:** A CLI tool to scaffold new recipes.
7. **Recipe Fallback:** If a recipe fails, try an older version or a generic fallback.
8. **Recipe Hooks:** Callbacks before/after each phase for custom behavior.

---

## 8. Build Hermeticity & Deterministic Builds

The goal is to make Shmoo the first tool someone uses when running builds on or for a different platform. The builds must be **exactly repeatable**.

### 8.1 Compiler Driver Enforcement
In "release" mode, compiler drivers must ensure that date and time strings are set consistently for each build to avoid differences in binary artifacts:
- Force `__DATE__` and `__TIME__` to a constant value.
- Set `SOURCE_DATE_EPOCH=0` to strip filesystem timestamps from debug info.
- Normalize `TZ`, `LC_ALL`, and other environment variables.
- Ensure compiler internal RNG uses a fixed seed.

### 8.2 Binary Inspection & Purity Suite
A very thorough inspection suite is required that can dissect generated binaries and note facts about them. These facts can then be compared with subsequent or shadow builds to ensure purity:
- Disassemble binary sections (text, data, bss, debug, rodata).
- Extract embedded timestamps, paths, and entropy.
- Hash system libraries and sysroot components.
- Verify symbol ordering and dependency linkage.
- Generate a "Purity Report" that can be compared against a baseline.

### 8.3 Do Other Build Systems Offer This?
Currently, **no build system bundles deep binary auditing with compiler driver enforcement**. Existing tools handle this problem in silos:
- **Bazel / Buck2 / Pants:** Enforce hermeticity via sandboxes and remote caching, but lack post-build binary dissection tools.
- **Nix:** Excellent for deterministic builds and reproducible tarballs, but has no built-in CLI to dissect a resulting binary and flag impurities.
- **diffoscope:** The gold standard for *comparing* two binaries/archives, but it's a standalone diffing tool, not a build-system hook.
- **SLSA / `in-toto` / `sigstore`:** Solve the *provenance* problem (attesting build steps, signing artifacts), but they don't analyze the binary's internal entropy, timestamp artifacts, or library linkage.
- **Binary Ninja / Ghidra / radare2:** Have powerful scripting APIs for dissection, but they're reverse-engineering tools, not build-system hooks.

**Shmoo's Advantage:** A build system that (1) enforces deterministic compiler flags at the driver level, (2) inspects the generated binary for purity artifacts, (3) compares it against a baseline, and (4) signs/certifies the result — all in a composable, recipe-driven package. This is a gap in the current landscape.

### 8.4 Business Model: Open Source vs. Sealed & Certified
- **Open Source:** Anyone can play along at home with the open-sourced configuration and build environments.
- **Sealed & Certified:** Shmoo will offer sealed and certified tarballs for folks who want us to tend their toolchains for cash.
- The business model revolves around maintaining the *Certified Externals* — the known-good, purity-verified toolchains and dependencies that override the default (and often broken) system installations.

---

## 9. External Dependencies & Overrides

A recipe is useless if the external tools it depends on (e.g., a compiler, a specific version of Perl) are broken or leaky. The recipe book must account for "host impurities" where the target environment's default toolchain fails a purity check.

### 9.1 The Problem with "System" Externals
The default installation mechanism of an OS is not always reliable.
- **Example:** The `wine-strawberry` transition recipe needs a working Perl interpreter.
- **The Failure Mode:** If the recipe blindly relies on the host's default `apt install perl` or a registry-installed ActiveState Perl, it might inherit a build that leaks registry paths or Windows system clock timestamps into the binary.
  - **Historical Context:** ActiveState Perl had significant problems with accessing the Windows registry. That is exactly how I found Strawberry Perl in the first place, and it worked perfectly.
- **The Solution:** The recipe must have the capability to override the default installation. In this case, the recipe should forcibly install a known-working distribution (e.g., Strawberry Perl) into a sandboxed or private path (e.g., `/mnt/tools/perl`), effectively bypassing the broken host default.

### 9.2 Accounting for External Development Effort
The build system's "pure" internal logic (the compiler driver, the inspector) is one thing. The "bridge" logic—the code that ensures the external host tools are actually working as expected—is another.
- **Effort:** Significant development effort will be required to audit and "certify" common external dependencies.
- **Recipe Book Extension:** The recipe book needs a concept of a **Certified External**:
  - `Recipe::External::StrawberryPerl` — A known-good version of Strawberry Perl, pre-verified by Shmoo.
  - `Recipe::External::SystemPerl` — A placeholder for the system default (unverified).

### 9.3 Implementation Requirement
The recipe's `prepare()` phase must support a **Force Override** pattern:
```perl
sub prepare {
    my ($self, $ctx) = @_;
    
    # Check if the system perl works.
    if ($ctx->host->is_broken('perl')) {
        # Override: Install our known-good version.
        $self->install_known_good('Shmoo::External::StrawberryPerl');
    }
}
```

This is where the Shmoo business model lives. We provide the *Certified Externals* (the "overriding" toolchains). Anyone can try the system defaults for free, but they'll hit impurities. The paying customer gets the Shmoo-certified external that overrides the system's broken defaults.

---

## Summary

A recipe book as a CPAN module is an elegant solution for a transition system:
- Recipes are just Perl modules — no special format, no schema.
- Anyone can contribute by publishing to CPAN.
- Auto-upgrade is trivial: `cpan install Shmoo::Transitions::Recipe::X`.
- Dependency resolution is recursive and natural (Perl `use` statements).
- The builder is a thin wrapper around the recipe book.

The recipe book makes the builder smart; the CPAN ecosystem makes it extensible.
Adding deterministic compiler drivers and deep binary auditing makes Shmoo a true **build integrity platform**, capable of guaranteeing that every build artifact is pure, repeatable, and verifiably hermetic.
