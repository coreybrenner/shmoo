# Dev Discussion: Build Hermeticity, Binary Purity & External Overrides

*2025-01-23 | Status: Concept Design*
*Purpose: Discuss and capture the requirements for reproducible builds, binary auditing, and the management of unreliable external dependencies.*

---

## Vision

The goal is to make Shmoo the first tool someone uses when running builds on or for a different platform. The builds must be **exactly repeatable**. In "release" mode, compiler drivers enforce consistent timestamps, and a thorough inspection suite dissects generated binaries to compare them with subsequent or shadow builds, ensuring purity.

This purity and repeatability become a core selling point. Shmoo can offer a consultancy business maintaining open-source toolchains as hermetic build environments. Anyone can use the open-source config, but Shmoo will offer **sealed and certified tarballs** for those who want us to tend their toolchains for cash.

---

## 1. Exact Repeatability & Deterministic Builds

### The Problem
Standard compilers leak non-deterministic data into binaries, making even an identical source tree produce different artifacts on different build runs or machines:
- **System timestamps:** `__DATE__`, `__TIME__`, and filesystem metadata.
- **Compiler randomness:** GCC/Clang use randomized seed values for instruction scheduling.
- **Host paths:** Absolute paths to headers or libraries embedded in debug sections.
- **Environment:** Locale, CPU features detected at compile time, or environment variables.

### The Solution: `shbuild` Driver Wrappers

The build system must provide wrapper scripts that enforce hermeticity before passing commands to the actual compiler (`gcc`, `clang`, `ld`, etc.).

**Implementation (Conceptual):**
```bash
# shbuild-cc wrapper logic
export SOURCE_DATE_EPOCH="0"          # Strips filesystem mtime from debug info.
export TZ="UTC"                       # Eliminates timezone bleed.
export LC_ALL="C"                     # Normalizes locale and sorting.
export CC_FLAGS="${CC_FLAGS} -frandom-seed=hash_of_source_files" # Neutralizes compiler RNG.
export NO_DEBUG_ALT_LINE="1"          # Prevents host path leakage in debug sections.
export __DATE__="\"Jan 1 2024\""      # Hardcoded date strings.
export __TIME__="\"00:00:00\""        # Hardcoded time strings.

# Pass strictly controlled flags through to the actual compiler.
exec gcc "$@"
```

**Metadata Output:**
Each build invocation produces a `build-metadata.json`:
- Compiler version and exact flag list.
- Sysroot hash (SHA-256 of the entire toolchain environment).
- Host CPU ID and OS version.
- Source tree hash (SHA-256 of all input files).

---

## 2. The Binary Purity Inspection Suite

### The Problem
Just because the build *flags* are hermetic doesn't mean the resulting binary is free of impurities. A developer needs a tool that can dissect a generated binary, extract facts about it, and compare those facts against a known-good baseline.

**Are there existing tools?**
- **Bazel / Buck2 / Pants:** Enforce hermeticity via sandboxes and remote caching, but lack post-build binary dissection.
- **Nix:** Normalizes paths and strips timestamps, but has no CLI to dissect a binary and flag impurities.
- **diffoscope:** Excellent for comparing two binaries to see what changed, but is a standalone diffing tool, not a build-system hook.
- **SLSA / sigstore:** Solve the provenance problem (attesting build steps), but don't analyze the binary's internal entropy, timestamps, or library linkage.
- **Binary Ninja / Ghidra:** Powerful reverse-engineering tools with scripting APIs, but not build-system hooks.

**The Gap:** No build system currently bundles deep binary auditing with compiler driver enforcement. This is where Shmoo provides unique value.

### The Implementation: `shbuild-inspect` & `shbuild-diff`

**`shbuild-inspect`: Post-build dissection pipeline.**
Runs a series of static analysis tools and outputs a structured fingerprint:

```json
{
  "binary_sha256": "...",
  "sections": ["text", "data", "bss", "debug", "rodata"],
  "debug_paths_extracted": [],
  "timestamp_strings": [],
  "sysroot_hash": "sha256:abc123...",
  "linked_libraries": ["libc.so.6", "libm.so.6", "libgcc_s.so.1"],
  "library_hashes": {
    "libc.so.6": "sha256:def456...",
    "libm.so.6": "sha256:ghi789..."
  },
  "symbol_order": ["start", "main", "_init", ...],
  "entropy": { "text": 5.82, "data": 2.11, "debug": 0.00 }
}
```

**`shbuild-diff`: Compares a new build's fingerprint against a baseline.**
```bash
shbuild-diff baseline.json new-build.json --strict
```
**Flags impurities:**
- `DIFF_COMPILER_VERSION` (e.g., GCC 13.2.0 → 13.3.0)
- `DIFF_TIMESTAMPS` (leaked `__DATE__` or debug path)
- `DIFF_SYMBOL_ORDER` (compiler reordering)
- `DIFF_LIBRARY_VERSION` (linked against different libc)
- `DIFF_ENTROPY_DELTA` (unexpected randomness in debug/sections)
- `DIFF_DEBUG_PATHS` (absolute host paths leaked into DWARF)

Generates a **Purity Report** (`purity.json`) that can be attached to a release artifact.

---

## 3. Provenance & Certification

- Wraps the Purity Report in an `in-toto` layout.
- Signs it with `cosign` / `sigstore` for SLSA Level 3+ compliance.
- Produces a **Certified Tarball**: the binary + metadata + signature + recipe hash.

---

## 4. Integration with `Shmoo::Transitions::RecipeBook`

The compiler driver, inspector, comparator, and signer are all packaged as `Shmoo::Transitions::Recipe::Build::*` modules:

```
RecipeBook:
├── Recipe::Build::HermeticGcc          # Open source compiler wrapper.
├── Recipe::Build::ClangCertified       # Licensed wrapper (maintained by Shmoo LLC).
├── Recipe::Inspect::Inspector          # Open source dissection pipeline.
├── Recipe::Compare::Comparator         # Open source diffing/purity logic.
└── Recipe::Provenance::Signer          # Open source signing/layout.
```

A dev requests a build: `"compile libfoo.a for x86_64-linux-hermetic"`.
1. RecipeBook resolves `HermeticGcc`, `Inspector`, `Comparator`, `Signer`.
2. `shbuild-cc` intercepts the build, enforces flags, strips timestamps.
3. `shbuild-inspect` runs post-build, generates fingerprint.
4. `shbuild-diff` compares against baseline, generates Purity Report.
5. `cosign` signs the report and binary.
6. Certified tarball shipped.

---

## 5. The Business Model: Open Source vs. Sealed & Certified

| | **Open Source (Community)** | **Sealed & Certified (Shmoo LLC)** |
|---|---|---|
| **What's included** | Recipe modules (`HermeticGcc`, `Inspector`, `Comparator`), open compiler wrappers, community-maintained toolchain recipes. | Pre-validated, SLA-backed toolchains, signed baseline fingerprints, dedicated purity audits, hosted cert repository, priority recipe dev. |
| **Distribution** | GitHub / CPAN / self-hosted recipe registry. | Shmoo-certified tarballs, cryptographically signed, version-locked. |
| **Maintenance** | Community-driven. Updates depend on contributors. | Shmoo maintains the toolchain, patches compiler drivers, updates inspector as new leak vectors are discovered. |
| **Guarantee** | "Works best effort." No SLA. | "100% hermetic purity guaranteed." Signed attestation per build. Auditable supply chain. |
| **Target** | Hobbyists, researchers, teams who want to self-host and verify. | Enterprises, regulated industries, clients who need audited, certified builds for compliance. |

A client pays Shmoo to:
1. Lock their toolchain versions (compiler, stdlib, linker).
2. Provide certified baseline fingerprints.
3. Run Shmoo's sealed `Inspector` / `Comparator` as part of their CI.
4. Receive signed Purity Reports for every release build.
5. Get priority patching if a new compiler version leaks timestamps or changes debug sections.

---

## 6. External Dependencies & Overrides

A recipe is useless if the external tools it depends on (e.g., a compiler, a specific version of Perl) are broken or leaky. The recipe book must account for "host impurities" where the target environment's default toolchain fails a purity check.

### The Problem with "System" Externals
The default installation mechanism of an OS is not always reliable.
* **Example:** The `wine-strawberry` transition recipe needs a working Perl interpreter.
* **The Failure Mode:** If the recipe blindly relies on the host's default `apt install perl` or a registry-installed ActiveState Perl, it might inherit a build that leaks registry paths or Windows system clock timestamps into the binary.
    * **Historical Context:** ActiveState Perl famously had problems accessing the Windows registry and managing environment variables, which is exactly how I ended up finding and preferring Strawberry Perl. Strawberry Perl works perfectly because it doesn't fight the Windows environment; it embraces it, while still remaining clean and predictable.
* **The Solution:** The recipe must have the capability to override the default installation. In this case, the recipe should forcibly install a known-working distribution (e.g., Strawberry Perl) into a sandboxed or private path (e.g., `/mnt/tools/perl`), effectively bypassing the broken host default.

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

## Future Extensions

1. **Recipe Signing:** Recipes can be signed with a GPG key to prevent tampering.
2. **Recipe Versioning:** Recipes are versioned; the recipe book can pin specific versions.
3. **Recipe Sandboxing:** Recipes run in a sandbox to prevent malicious code.
4. **Recipe Testing:** CI tests each recipe against a real environment before publishing.
5. **Recipe Marketplace:** A website where users can browse and install recipes.
6. **Recipe Templates:** A CLI tool to scaffold new recipes.
7. **Recipe Fallback:** If a recipe fails, try an older version or a generic fallback.
8. **Recipe Hooks:** Callbacks before/after each phase for custom behavior.
9. **Entropy Analysis:** Deeper statistical analysis of binary sections to detect random seeds or non-deterministic algorithmic outputs.
10. **Linker Script Patching:** Automating the patching of linker scripts to force deterministic symbol ordering and section layouts.

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
