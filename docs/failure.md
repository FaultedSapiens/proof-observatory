\# Proof Observatory — Failure Notebook



A running record of failures and engineering decisions encountered while building

Proof Observatory.



Format:



\- Symptom

\- Cause

\- Fix / recovery

\- Engineering lesson / tradeoff



The goal is not to make every failure sound important. Most are ordinary

engineering problems. The useful part is recording why the architecture changed.



\---



\## 1. Formalization YAML alignment parser



\### Symptom



The inspector reported:



&#x20;   Alignments: 0



while the real `formalization.yaml` contained four paper ↔ Lean alignment entries.



\### Cause



The parser assumed a YAML structure that did not match the actual artifact schema.



\### Fix



Parsed the actual `formalization.yaml` structure and tested it against the real

repository.



\### Lesson



Do not design metadata parsers from an assumed schema.



For formalization metadata, the upstream artifact is authoritative. The C++ data

model should adapt to it, not the other way around.



\---



\## 2. FNV-1a implementation typo



\### Symptom



The indexer's FNV-1a implementation used:



&#x20;   std::uint64\_t hash = offset;



while the implementation actually operated on `hash\_`.



\### Cause



Simple variable-name/implementation error.



\### Fix



Corrected the hash state variable.



\### Lesson



Small infrastructure code still needs tests. Hashing is simple enough to be

easy to overlook and important enough that a tiny bug can invalidate indexing.



\---



\## 3. C++20 `starts\_with` tooling issue



\### Symptom



The indexer used:



&#x20;   line.starts\_with("import ")



The project was C++20, but the active tooling/IntelliSense environment did not

handle the API correctly.



\### Fix



Used:



&#x20;   line.rfind("import ", 0) == 0



\### Lesson



Theoretical language support is less important than compatibility with the

actual compiler/tooling environment.



The simpler prefix operation was sufficient, so there was no reason to fight

the tooling.



\---



\## 4. Windows → WSL UNC working-directory failure



\### Symptom



Running verification against:



&#x20;   \\\\wsl.localhost\\Ubuntu\\home\\yash\\projects\\NavierStokesAndEuler



failed before Lean started.



\### Cause



A normal Windows process cannot use a WSL UNC path as its ordinary working

directory in the way the original process runner expected.



\### Fix



Run Linux-side commands inside WSL using:



&#x20;   \~/projects/NavierStokesAndEuler



rather than trying to run `lake` from a Windows process whose current directory

is the UNC path.



\### Lesson



WSL paths are not ordinary Windows paths.



The process boundary needs to be explicit:



&#x20;   Windows Observatory

&#x20;       ↓

&#x20;   WSL command

&#x20;       ↓

&#x20;   Linux-native path



This is more reliable than pretending the two filesystems are the same.



\---



\## 5. Windows ↔ WSL per-file access was too slow



\### Problem



The original design accessed thousands of Lean files individually through the

Windows ↔ WSL boundary.



The repository contains roughly 2,659 Lean source files.



\### Cause



The filesystem boundary is expensive. Thousands of UNC operations multiply that

overhead.



\### Fix



Changed staging to a bulk WSL-side archive/transfer operation.



Instead of:



&#x20;   file

&#x20;   file

&#x20;   file

&#x20;   ...



use:



&#x20;   WSL

&#x20;     ↓

&#x20;   bulk archive

&#x20;     ↓

&#x20;   Windows

&#x20;     ↓

&#x20;   one indexing operation



\### Lesson



Cross-OS filesystem boundaries should be treated as expensive interfaces.



Batching is a better tradeoff than optimizing thousands of individual accesses.



\---



\## 6. Process runner relied on global current-directory state



\### Problem



The original runner used:



&#x20;   std::filesystem::current\_path()



to change the process-wide working directory.



\### Why this was bad



The state affects the entire process and makes independent command execution

harder to reason about.



\### Fix



Commands now have explicit working-directory semantics.



\### Lesson



External process execution should carry its own:



\- command

\- arguments

\- working directory

\- environment

\- stdout/stderr handling

\- exit status

\- timeout/error information



Avoid global mutable execution state.



\---



\## 7. Accidental line-ending corruption of the target repository



\### Symptom



A huge number of Lean files suddenly appeared modified.



Observed diff statistics were approximately:



&#x20;   +641,895

&#x20;   -641,895



The source itself had not intentionally been changed.



\### Cause



Line-ending conversion touched tracked Lean files.



\### Recovery



Restored the repository with Git:



&#x20;   git restore .



The repository returned to a clean state.



\### Lesson



Never run blind formatting/line-ending conversion over a large formalization

repository.



Before repository-wide operations, verify:



&#x20;   git status

&#x20;   git diff --stat



and keep source changes separate from generated/build state.



\---



\## 8. Accidental nested duplicate repository



\### Symptom



An untracked directory appeared:



&#x20;   NavierStokesAndEuler/NavierStokesAndEuler/



It contained another apparent repository including `.git` and `.lake`.



\### Cause



A duplicate copy/clone had been introduced during recovery work.



\### Fix



Removed the nested repository.



\### Lesson



Before destructive or repository-wide commands, verify:



&#x20;   git rev-parse --show-toplevel



There should be one obvious repository root.



\---



\# Lean / Lake / Cache Failures



\## 9. Missing `.olean` dependencies



\### Symptom



Targeted module verification failed because required project `.olean`

dependencies were missing.



Example:



&#x20;   NavierStokes.ComparatorR3Theorem.olean



\### Cause



The source existed, but the required compiled dependency graph had not been

built.



\### Lesson



Source availability and compiled-environment availability are different

things.



A targeted Lean check can fail because the environment is incomplete without

the theorem itself being wrong.



\---



\## 10. Invalid `.olean` headers



\### Symptom



`lake build` produced errors such as:



&#x20;   invalid header



while reading Mathlib `.olean` files.



Affected modules included several unrelated Mathlib modules.



\### Cause



Generated compiled artifacts were corrupted.



\### Lesson



This was a build/cache failure, not evidence of a mathematical failure.



The important distinction is:



&#x20;   source repository

&#x20;       !=

&#x20;   generated Lean environment



\---



\## 11. Zero-length / all-zero `.olean` files



\### Symptom



Investigation found generated `.olean` files that were empty or contained

invalid/all-zero data.



Approximately 239 zero-length `.olean` files were found during investigation.



\### Lesson



A file existing at the expected path does not mean it is a valid compiled

artifact.



A reproducibility system eventually needs artifact validation, not just

existence checks.



\---



\## 12. Corrupt Lake cache archives



\### Symptom



`lake exe cache get` did not immediately repair the environment.



Some local `.ltar` cache entries were themselves corrupt.



\### Cause



The recovery source was partially damaged too.



\### Fix



Identify/remove bad cache entries and retrieve the affected artifacts again.



\### Lesson



Caches can become part of the failure chain.



"Restore from cache" is only reliable if the cache itself is trusted.



\---



\## 13. Cache metadata could preserve bad state



\### Symptom



After replacing generated artifacts, Lake could still behave as if some

existing build state was valid.



\### Cause



Generated files and associated incremental/build metadata participate in Lake's

validity decisions.



\### Fix



For affected dependencies, forced retrieval was required:



&#x20;   lake exe cache get!



\### Lesson



Replacing one output file is not always enough.



Build systems maintain state about what has already been satisfied. Recovery has

to account for that state.



\---



\## 14. Corrupted `.ir` artifacts



\### Symptom



After getting past `.olean` failures, another generated artifact produced:



&#x20;   invalid header



Example:



&#x20;   Mathlib/Analysis/Normed/Group/Rat.ir



\### Lesson



The corruption was not limited to `.olean`.



Generated Lean state must be treated as a coherent build cache rather than a

collection of independent files.



\---



\## 15. Corrupted `.olean.server` artifacts



\### Symptom



Generated files such as:



&#x20;   Mathlib/Algebra/Group/Submonoid/Units.olean.server



also produced invalid-header errors.



\### Lesson



Recovery cannot assume that only `.olean` files matter.



\---



\## 16. More corrupted `.olean` files appeared later



\### Symptom



After repairing earlier failures, later parts of the dependency graph exposed

additional invalid `.olean` files.



Example:



&#x20;   Mathlib/Algebra/Polynomial/UnitTrinomial.olean



\### Lesson



Partial cache repair can simply expose the next bad artifact.



That is why repeated manual cache repair is not a good long-term reproducibility

strategy.



\---



\## 17. Corrupted `.trace` state



\### Symptom



Later builds produced warnings such as:



&#x20;   unexpected input

&#x20;   unexpected end of input



while processing generated trace state.



\### Lesson



Generated metadata can also be corrupted.



Again, this is separate from source correctness.



\---



\# Build / Host Failures



\## 18. Full Lean build was extremely expensive



\### Problem



The upstream project has a very large dependency graph.



Observed progress included:



&#x20;   7062/7118



and later:



&#x20;   8782/9580



targets.



\### Impact



A complete build can take a very long time on the development machine.



\### Engineering decision



Proof Observatory uses layered verification:



&#x20;   metadata

&#x20;   structural

&#x20;   targeted Lean

&#x20;   module

&#x20;   Comparator

&#x20;   full



\### Lesson / tradeoff



Do not make every inspection run a full formal build.



Full reproduction is valuable, but it is an explicit expensive operation.



\---



\## 19. Full build was interrupted



\### Symptom



A long `lake build` was manually stopped.



Observed exit status:



&#x20;   130



\### Correct interpretation



This means the execution was interrupted.



It does not mean:



&#x20;   theorem failed



or:



&#x20;   formalization failed



\### Lesson



The report needs at least:



&#x20;   success

&#x20;   failure

&#x20;   interrupted

&#x20;   timeout

&#x20;   unavailable

&#x20;   not-run



A stopped experiment is not a failed experiment.



\---



\## 20. Targeted build was also interrupted



\### Symptom



A targeted dependency build reached approximately:



&#x20;   8995/9038



before being stopped.



\### Lesson



Targeted does not automatically mean cheap.



Some apparently small targets pull in most of the dependency graph.



The Observatory therefore needs to report actual execution cost rather than

assuming a target is inexpensive.



\---



\## 21. WSL became unresponsive



\### Symptom



After prolonged Lean/Comparator activity, WSL stopped responding to normal

commands.



Windows reported:



&#x20;   Wsl/Service/0x8007274c



\### Recovery



WSL was restarted with:



&#x20;   wsl --shutdown



\### Lesson



The host execution environment is another possible failure layer.



The report should distinguish:



&#x20;   Observatory failure

&#x20;   Lean failure

&#x20;   WSL failure

&#x20;   host failure



\---



\## 22. Host crashes interrupted reproducibility work



\### Symptom



The development machine experienced Windows kernel crashes including:



&#x20;   MEMORY\_MANAGEMENT

&#x20;   UNEXPECTED\_STORE\_EXCEPTION

&#x20;   INACCESSIBLE\_BOOT\_DEVICE



\### Impact



Long-running builds and cache operations could be interrupted.



\### Lesson



When generated artifacts are corrupted after a crash, do not immediately blame

Lean.



Possible causes include:



\- compiler/build bug

\- interrupted write

\- cache corruption

\- WSL failure

\- OS failure

\- storage/memory instability



The host is part of the reproducibility environment.



\---



\## 23. Codex build environment was not equivalent to the development machine



\### Problem



At one point the agent environment lacked the required Windows build tooling or

PATH configuration.



\### Important distinction



The agent being unable to build does not establish that the project cannot

build.



\### Lesson



Every command in a reproducibility record needs execution-context information.



For example:



&#x20;   environment: Windows development machine



is different from:



&#x20;   environment: agent sandbox



\---



\# Observatory / CTest Failures



\## 24. CTest multi-config confusion



\### Symptom



CTest initially reported tests as not run/failing even though Debug binaries

existed.



\### Cause



The Visual Studio generator is multi-config.



CTest needed:



&#x20;   ctest --test-dir build -C Debug --output-on-failure



\### Lesson



Build configuration and test configuration are separate concepts for a

multi-config generator.



Use:



&#x20;   cmake --build build --config Debug



and:



&#x20;   ctest --test-dir build -C Debug --output-on-failure



\---



\## 25. Simulation executable passed but CTest failed



\### Symptom



Direct execution worked:



&#x20;   Simulation complete: 3 steps, t=0.003,

&#x20;   max |omega|=0.00472449



but the CTest invocation failed.



\### Investigation



The executable passed when run directly from both the project and build

directories.



The failure was therefore in the test harness/environment rather than the

simulation itself.



\### Lesson



When a harness reports failure:



&#x20;   run the underlying executable directly



before changing the executable.



\---



\## 26. Windows file-locking caused simulation test failure



\### Symptom



The simulation test tried to delete its temporary output directory while

`diagnostics.csv` was still open.



Windows prevented cleanup.



\### Fix



Close the output stream before deleting the directory.



\### Lesson



Resource lifetime matters in tests.



Especially on Windows:



&#x20;   file still open

&#x20;       =

&#x20;   cleanup may fail



The simulation itself was not the problem.



\---



\## 27. Checkpoint/restart consistency



\### Goal



Compare:



&#x20;   continuous simulation



against:



&#x20;   run → checkpoint → restart → continue



\### Requirement



For a deterministic configuration, both paths should agree within the defined

numerical tolerance.



\### Lesson



Checkpointing is not just a file-writing feature.



It is part of the reproducibility contract and therefore needs its own test.



\---



\# Comparator Failures



\## 28. Missing `landrun`



\### Symptom



Comparator initially failed with:



&#x20;   could not execute external process 'landrun'



\### Cause



The required sandbox executable was not installed/resolvable.



\### Fix



Installed the pinned `landrun` revision and added tool discovery.



\### Lesson



Comparator has dependencies beyond Lean itself.



Those dependencies need explicit provenance and resolution.



\---



\## 29. Elan `lake` permission problem inside sandbox



\### Symptom



The sandbox could resolve the Elan launcher but could not execute it correctly.



\### Cause



The launcher path/permissions interacted badly with the sandbox.



\### Fix



Used the actual pinned toolchain executable:



&#x20;   /home/yash/.elan/toolchains/leanprover--lean4---v4.34.0-rc2/bin/lake



instead of relying on the launcher.



\### Lesson



Inside a sandbox, "which executable" matters.



A wrapper executable that works normally may not behave identically under

restricted execution.



\---



\## 30. Landlock ABI was too old



\### Symptom



Pinned `landrun` could not operate correctly under the original WSL kernel.



\### Cause



The kernel did not provide the required Landlock ABI.



\### Fix



Updated WSL from kernel 6.6 to 6.18.



\### Lesson



Sandbox tooling has kernel-level prerequisites.



Tool versions alone do not completely define the execution environment.



\---



\## 31. Comparator PATH propagation failure



\### Problem



The pinned Comparator runs subprocesses inside its sandbox and only forwards

selected environment variables.



Tools that were visible outside the sandbox were not necessarily visible

inside it.



\### Fix



Proof Observatory resolves the pinned helper tools and makes their directories

available through the relevant PATH.



\### Lesson / tradeoff



Do not globally mutate the user's environment.



Resolve the exact tools needed by the verification operation and pass the

required environment explicitly.



\---



\## 32. Comparator export failed with unknown module prefix



\### Symptom



The challenge build completed, but export failed with:



&#x20;   unknown module prefix 'ComparatorChallenges'



\### Cause



The export sandbox did not initially have the project build library on its

Lean module search path.



\### Fix



Ensure the relevant project `.lake/build/lib/lean` path is present through

`LEAN\_PATH` in the export environment.



\### Lesson



Build success and export success are separate stages with separate environment

requirements.



\---



\## 33. Direct Comparator invocation was not equivalent to `lake exe`



\### Problem



Running the Comparator binary directly did not necessarily reproduce the

environment provided by:



&#x20;   lake exe comparator ...



\### Cause



Lake supplies project/environment information.



\### Lesson



For project-integrated tools, the supported project invocation is part of the

reproducibility procedure.



\---



\## 34. Comparator build/export/compare are separate failures



Comparator effectively performs:



&#x20;   build challenge

&#x20;       ↓

&#x20;   export challenge

&#x20;       ↓

&#x20;   build solution

&#x20;       ↓

&#x20;   export solution

&#x20;       ↓

&#x20;   compare



A failure at any earlier stage is not a Comparator rejection.



For example:



&#x20;   build failure

&#x20;   export failure

&#x20;   environment failure



must not become:



&#x20;   comparator rejected proof



\### Lesson



Failure classification must preserve the stage that actually failed.



\---



\## 35. Comparator execution is expensive



The pinned Navier–Stokes/Euler formalization requires substantial compilation

before Comparator can reach comparison.



An invocation entered a large:



&#x20;   lake build ComparatorChallenges.Euler



operation and consumed significant CPU for an extended period.



\### Lesson / tradeoff



Comparator is an expensive verification mode.



It should be explicit rather than silently triggered by normal inspection.



\---



\## 36. Comparator interruption / timeout



If Comparator is stopped before reaching comparison, the correct state is:



&#x20;   comparator\_interrupted



or:



&#x20;   comparator\_timeout



It is not:



&#x20;   comparator\_rejected



\### Lesson



Again, "did not finish" and "failed verification" are different outcomes.



\---



\## 37. No reusable Comparator export cache



The pinned Comparator implementation performs its own build/export workflow.



Proof Observatory cannot safely claim that a previous export is automatically

usable as a substitute for another Comparator run.



\### Lesson / tradeoff



Do not invent a cache layer that changes the semantics of the pinned upstream

tool.



Optimization can come later, but reproducibility should first follow the

actual pinned implementation.



\---



\# Structural Analysis Limitations



\## 38. Lexical `sorry` count is heuristic



The structural analyzer can count textual occurrences of `sorry`.



That can include comments or strings.



Therefore:



&#x20;   lexical sorry count



is not equivalent to:



&#x20;   Lean's actual declaration-level sorry usage.



\### Lesson



Keep the result explicitly labeled heuristic.



\---



\## 39. Lexical `axiom` count is heuristic



Same issue for textual `axiom` detection.



\### Lesson



A source-text count is useful for inspection but should not be presented as

kernel-level evidence.



\---



\## 40. Structural dependencies are heuristic



The source indexer initially derives dependency candidates from lexical/source

information.



That is useful for navigation but is not the same as Lean's elaborated

dependency graph.



\### Lesson / tradeoff



Lexical indexing is fast and cheap.



Compiler-derived dependency extraction is more authoritative but more expensive.



Keep the evidence levels separate rather than pretending the cheap method is

equivalent to the expensive one.



\---



\## 41. Claim alignment must come from authoritative metadata



The paper ↔ Lean mapping must come from the artifact's:



&#x20;   formalization.yaml



For the pinned artifact there are four concrete alignment entries.



Do not infer those relationships from similar names.



\### Lesson



Metadata supplied by the formalization is the source of truth for alignment.



\---



\# Evidence / Interpretation Failures



\## 42. Metadata verification is not proof verification



Metadata verification can establish:



\- commit

\- clean/dirty state

\- remote

\- tracked-file hashes

\- toolchain versions

\- host information



It cannot establish mathematical correctness.



\### Lesson



Metadata is provenance evidence, not proof evidence.



\---



\## 43. Structural success is not Lean verification



A successful structural scan means source analysis succeeded.



It does not mean Lean accepted the formalization.



\### Lesson



Keep:



&#x20;   structural\_success



separate from:



&#x20;   lean\_verified



\---



\## 44. Lean compilation is not the same as independent mathematical validation



A successful Lean compilation establishes that Lean accepted the checked terms

under that environment.



It does not by itself establish:



\- correctness of the paper's interpretation;

\- correctness of claims outside the formalized statement;

\- independence from every assumption;

\- absence of conceptual issues outside the formal system.



\### Lesson



Report exactly what was checked.



Do not expand the claim beyond the evidence.



\---



\## 45. Comparator evidence has a defined scope



A successful Comparator run is evidence produced by that Comparator workflow.



It should not automatically be described as:



&#x20;   "the entire mathematical result has been independently verified."



\### Lesson



Every verification result needs an explicit evidence type and provenance.



\---



\## 46. Numerical simulation is not mathematical proof



The numerical subsystem can show computational behavior such as:



\- vorticity evolution

\- numerical growth

\- instability

\- convergence/error behavior

\- finite-resolution trajectories



It cannot by itself establish a theorem about singularity or global regularity.



\### Lesson



Numerical experiments remain a separate subsystem from formal verification.



This separation is intentional.



\---



\# Reproducibility / Reporting Failures



\## 47. Provenance was initially too thin



Early reports captured basic information such as:



\- Git commit

\- clean/dirty state

\- file count

\- metadata



But reproducibility also needs:



\- exact command

\- environment

\- toolchain

\- dependency revisions

\- stdout/stderr

\- exit code

\- duration

\- relevant generated artifacts

\- hashes where appropriate



\### Lesson



A reproducibility system records the experiment, not just the repository.



\---



\## 48. Source state and generated state were initially too easy to conflate



A Git-clean repository can still have a broken `.lake` environment.



Conversely, generated files can be changed without source changes.



\### Lesson



Reports should distinguish:



&#x20;   source provenance



from:



&#x20;   generated build environment



\---



\## 49. Failure status was initially too coarse



A generic:



&#x20;   verification failed



is not enough.



Examples encountered:



\- command not run

\- command interrupted

\- WSL unavailable

\- cache corrupted

\- dependency missing

\- tool missing

\- sandbox unavailable

\- actual Lean compilation failure

\- Comparator export failure

\- Comparator rejection



\### Required status vocabulary



&#x20;   PASS

&#x20;   FAIL

&#x20;   ERROR

&#x20;   INTERRUPTED

&#x20;   TIMEOUT

&#x20;   NOT\_RUN

&#x20;   UNAVAILABLE

&#x20;   PARTIAL

&#x20;   SKIPPED

&#x20;   UNKNOWN



\### Lesson



"Not checked" must never be represented as "failed".



\---



\## 50. Criticism/evidence tracking is incomplete



Proof Observatory is intended to track:



&#x20;   claim

&#x20;     ↓

&#x20;   criticism/evidence

&#x20;     ↓

&#x20;   source

&#x20;     ↓

&#x20;   affected declaration/claim



That subsystem is not yet complete.



\### Lesson



The final reproducibility report must not imply comprehensive criticism tracking

until the graph actually exists.



\---



\## 51. Reproducibility report integration is incomplete



The intended final report combines:



\- artifact provenance

\- paper claims

\- formalization alignment

\- structural analysis

\- Lean verification

\- Comparator verification

\- criticism/evidence

\- numerical experiments

\- diagnostics

\- visualization



Those components currently exist at different levels of completeness.



\### Lesson



Individual successful components should remain individually labeled rather than

being presented as one completed end-to-end verification.



\---



\## 52. Visualization is separate from simulation



A simulation can succeed while visualization fails.



Possible separate failures include:



\- field export

\- frame generation

\- file format

\- animation construction

\- metadata



\### Lesson



Numerical execution and visualization need separate status/evidence.



\---



\# Engineering Principles From the Failures



\## 53. Do not fix source code just to make reproduction pass



When a build fails:



&#x20;   classify the failure

&#x20;       ↓

&#x20;   identify the layer

&#x20;       ↓

&#x20;   repair that layer

&#x20;       ↓

&#x20;   rerun

&#x20;       ↓

&#x20;   record evidence



Do not immediately modify mathematical source.



\---



\## 54. Full build is one verification layer, not the whole system



The architecture is intentionally:



&#x20;   metadata

&#x20;       ↓

&#x20;   structural

&#x20;       ↓

&#x20;   targeted Lean

&#x20;       ↓

&#x20;   Comparator

&#x20;       ↓

&#x20;   full build



Different layers answer different questions.



The expensive full build is valuable, but it is not an appropriate default for

every development operation.



\---



\## 55. Fast checks and expensive checks should coexist



Fast checks are useful for development:



\- parser tests

\- indexer tests

\- structural inspection

\- metadata checks

\- targeted files



Expensive checks are useful for final reproduction:



\- large Lean builds

\- Comparator

\- numerical experiments

\- complete reports



The tradeoff is straightforward:



&#x20;   fast path = iteration speed

&#x20;   expensive path = stronger evidence



Trying to make one operation provide both usually makes development unnecessarily

slow.



\---



\## 56. Cross-platform boundaries should be explicit



Windows and WSL are different execution environments.



The architecture should therefore make the boundary explicit rather than

relying on UNC paths and implicit environment inheritance.



\---



\## 57. Generated state should be disposable



The formal source repository is the source of record.



`.lake` and other generated state should be treated as reproducible build state,

not as source.



If generated state becomes corrupted, rebuild/retrieve it rather than modifying

source to accommodate it.



\---



\## 58. Preserve upstream semantics



For pinned external tools such as Comparator, reproduce the upstream execution

model first.



Do not introduce optimizations that silently change what is being verified.



Optimization comes after the baseline reproduction path is trustworthy.



\---



\## 59. Evidence should carry its boundary



Every report should answer:



&#x20;   What exactly did we execute?

&#x20;   What exactly passed?

&#x20;   What exactly failed?

&#x20;   In which environment?

&#x20;   What does that result establish?

&#x20;   What does it NOT establish?



That is the core design principle of Proof Observatory.



\---



\# Current State



The following Observatory components have been exercised successfully during

development:



\- project build/test infrastructure

\- formalization YAML parsing

\- structural indexing

\- metadata verification

\- WSL-side artifact execution

\- simulation execution

\- checkpoint/restart testing

\- Comparator tool discovery/preflight

\- Debug CTest suite after fixing the Windows file-lifetime issue



The pinned upstream formalization has also been inspected structurally and its

four authoritative alignment entries have been recovered.



The complete upstream Lean build and complete Comparator reproduction remain

expensive operations and have encountered environment/cache/build interruptions

during development.



Therefore the current state should be described as:



&#x20;   Observatory infrastructure: substantially working

&#x20;   structural analysis: working

&#x20;   provenance/metadata: working

&#x20;   numerical subsystem: working at basic level

&#x20;   targeted Lean reproduction: environment-dependent

&#x20;   full Lean reproduction: not yet established as a clean completed run

&#x20;   Comparator reproduction: infrastructure prepared, expensive execution

&#x20;                        remains a separate verification stage

&#x20;   criticism/evidence graph: incomplete

&#x20;   final end-to-end Observatory report: incomplete



\---



\# Failure Classification



Use the most specific known class.



Examples:



&#x20;   artifact\_environment\_failure

&#x20;   artifact\_staging\_failure

&#x20;   path\_resolution\_failure



&#x20;   toolchain\_environment\_failure

&#x20;   sandbox\_kernel\_capability\_failure



&#x20;   dependency\_build\_failure

&#x20;   dependency\_cache\_failure

&#x20;   project\_build\_failure



&#x20;   lean\_verification\_failure

&#x20;   execution\_interrupted

&#x20;   verification\_timeout

&#x20;   verification\_incomplete



&#x20;   comparator\_environment\_failure

&#x20;   comparator\_configuration\_failure

&#x20;   comparator\_build\_failure

&#x20;   comparator\_export\_failure

&#x20;   comparator\_rejection

&#x20;   comparator\_internal\_error

&#x20;   comparator\_interrupted

&#x20;   comparator\_timeout



&#x20;   structural\_heuristic

&#x20;   alignment\_metadata



&#x20;   simulation\_configuration\_failure

&#x20;   simulation\_runtime\_failure

&#x20;   numerical\_instability

&#x20;   output\_io\_failure

&#x20;   checkpoint\_restart\_failure

&#x20;   visualization\_failure



&#x20;   feature\_incomplete

&#x20;   integration\_incomplete



\---



\# Evidence Classes



Keep these separate:



| Evidence | Establishes | Does not establish |

|---|---|---|

| Metadata | artifact/environment identity | mathematical correctness |

| Structural analysis | source-level observations | Lean acceptance |

| Lean compilation | Lean accepted the checked terms | independent validation of the research claim |

| Comparator | result of the pinned Comparator workflow | correctness of unrelated claims |

| Numerical experiment | observed numerical behavior | mathematical proof of that behavior |



\---



\# Final rule



A failure in one layer must not silently become a failure in another.



Likewise, success in one layer must not silently become success in another.



The report should always distinguish:



&#x20;   observed

&#x20;   inferred

&#x20;   verified

&#x20;   failed

&#x20;   interrupted

&#x20;   not checked



That distinction is the main reason this notebook exists.

