# Proof Observatory

Proof Observatory records provenance, inspects Lean source structure, runs scoped
Lean checks, and produces reproducible numerical experiments. It treats source
analysis, compiler acceptance, Comparator output, and numerical results as
different kinds of evidence.

## Build

This is a C++20 CMake project with `yaml-cpp` supplied by vcpkg. With the Visual
Studio generator already configured:

```powershell
cmake --build build --config Debug --parallel 1
ctest --test-dir build -C Debug --output-on-failure
```

## CLI

```text
proof-observatory inspect <artifact-path>
proof-observatory index <artifact-path> [snapshot.json]
proof-observatory verify <artifact-path> [--mode MODE] [--target TARGET] [--report run.json]
proof-observatory simulate [output-dir] [simulation options]
```

`verify` defaults to `structural`; full project builds are opt-in.

| Mode | Action | Evidence produced |
|---|---|---|
| `metadata` | Reads repository/toolchain provenance and tracked-file SHA-256 hashes | Provenance only; no Lean compilation |
| `structural` | Indexes source files, declarations, imports, alignment metadata, and lexical tokens | Heuristic structural evidence; no Lean compilation |
| `file` | Runs `lake env lean <file>` | Lean acceptance for the requested source file and its imports |
| `module` | Maps a dotted Lean module name to its source file, then runs `lake env lean <file>` | Lean acceptance for the requested module source and its imports |
| `full` | Runs `lake build` | Full Lake build evidence |
| `comparator` | Runs `lake exe comparator <config>` for each JSON file in `ComparatorChallenges/` | Independent Comparator verification results |

Examples:

```powershell
proof-observatory verify <artifact> --mode metadata --report metadata.json
proof-observatory verify <artifact> --mode structural --report structure.json
proof-observatory verify <artifact> --mode module --target NavierStokes.ComparatorSolution
proof-observatory verify <artifact> --mode file --target NavierStokes/ComparatorSolution.lean
proof-observatory verify <artifact> --mode full --report full-build.json
proof-observatory verify <artifact> --mode comparator-preflight --report comparator-preflight.json
proof-observatory verify <artifact> --mode comparator --report comparator.json
```

Reports capture the input commit and dirty state, toolchain pin, environment,
commands, stdout/stderr, duration, exit status, and generated report path. They
also identify the verification class (`metadata_provenance`,
`structural_source_analysis`, `lean_kernel_compilation`, or
`comparator_independent_verification`). Comparator reports include
`comparator_status: "verified"` or `"failed"`. A successful Lean invocation
means Lean accepted the checked inputs in that environment; it is not
independent mathematical validation. Comparator success is evidence from that
Comparator run and configuration. `comparator_export_cache_status` records
whether the selected pinned Comparator supports export reuse. Comparator
failures are additionally classified as tool environment, project build,
export, configuration, or Comparator rejection/error so setup failures are not
reported as failed proofs.

## Comparator and export cost

For the pinned Comparator used by the Navier–Stokes and Euler artifact,
`lake exe comparator <config>` is the supported invocation. The `lake exe`
wrapper matters: Comparator forwards its process `LEAN_PATH` to `lean4export`,
so invoking the Comparator binary directly can lose the artifact's Lake package
paths and fail with `unknown module prefix 'ComparatorChallenges'`.

For WSL artifacts, Observatory resolves `landrun` and `lean4export` inside the
artifact's WSL login shell. Explicit `COMPARATOR_LANDRUN` and
`COMPARATOR_LEAN4EXPORT` settings take precedence; otherwise Observatory
resolves them from that shell's `PATH` and the conventional Go install/bin
locations for the pinned tools. It prepends the resolved executable
directories to `PATH`, which is the environment Comparator preserves inside
Landrun, and records the resolved paths in command output. No machine-specific
binary paths are embedded in Observatory.

Run `--mode comparator-preflight` before a costly Comparator run to check that
both helper executables resolve and that Lake's `LEAN_PATH` includes the
artifact's `.lake/build/lib/lean`. This check does not build the project or run
Comparator.

Comparator's pinned implementation incrementally builds each configured module
with Lake, exports only its configured theorem/definition targets plus the
required primitive declarations, then parses both exports and compares/replays
them in the same invocation. Existing `.olean` outputs let Lake replay cached
build jobs instead of recompiling the project. However, this Comparator version
does not persist or accept NDJSON exports, expose a separate compare-from-export
stage, or provide an export cache. It regenerates challenge and solution exports
on every Comparator invocation. Therefore there is no safe pre-generated export
cache that this pinned binary can consume, and the first (and every later)
Comparator run can spend substantial time exporting this large project. A
completed Comparator report records an actual run; structural, metadata, and
numerical reports are not substitutes for it.

To run the pinned artifact with its Lake-managed environment:

```powershell
proof-observatory verify \\wsl.localhost\Ubuntu\home\yash\projects\NavierStokesAndEuler --mode comparator --report comparator.json
```

The command may replay cached Lake outputs, but it still performs both exports.
Do not use a direct Comparator binary invocation as a shortcut. The project
report's `comparator_status` only says whether the Comparator process exited
successfully; inspect its command output for the detailed run evidence.

Structural dependency candidates are marked `lexical_heuristic`. They are not
Lean elaborator dependencies. Lexical `sorry` and `axiom` token counts include
comments and strings and must be read as candidate counts. File fingerprints in
the structural snapshot use non-cryptographic FNV-1a-64; verification metadata
records SHA-256 hashes for Git-tracked files.

For a WSL-hosted artifact, pass its Windows WSL UNC path, for example
`\\wsl.localhost\Ubuntu\home\user\projects\Artifact`. Inspection obtains
Lean paths with one artifact-side listing. Indexing stages a compressed archive
of Lean files and relevant project metadata through one process boundary instead
of traversing source files individually over UNC. Targeted Lean and Lake
commands run inside the artifact's WSL environment.

## Numerical experiments

`simulate` runs a deterministic 2D periodic incompressible Navier–Stokes solver
in vorticity–streamfunction form. It writes `run.json`, `diagnostics.csv`, VTK
fields, and SVG vorticity frames. These are numerical experiments, not proofs;
large vorticity or numerical instability does not establish mathematical
singularity or blow-up.
