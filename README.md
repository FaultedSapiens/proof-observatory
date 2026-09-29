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
| `comparator` | Runs `lake exe comparator <config>` for each JSON file in `ComparatorChallenges/` | Results from each recorded Comparator invocation |

Examples:

```powershell
proof-observatory verify <artifact> --mode metadata --report metadata.json
proof-observatory verify <artifact> --mode structural --report structure.json
proof-observatory verify <artifact> --mode module --target NavierStokes.ComparatorSolution
proof-observatory verify <artifact> --mode file --target NavierStokes/ComparatorSolution.lean
proof-observatory verify <artifact> --mode full --report full-build.json
proof-observatory verify <artifact> --mode comparator --report comparator.json
```

Reports capture the input commit and dirty state, toolchain pin, environment,
commands, stdout/stderr, duration, exit status, and generated report path. A
successful Lean invocation means Lean accepted the checked inputs in that
environment; it is not independent mathematical validation. Comparator success
is evidence from that Comparator run and configuration.

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
