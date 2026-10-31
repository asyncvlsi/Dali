# Public-derived gridded consensus suite

The small adder is an integration fixture. This suite exercises real public
circuit connectivity at 29,517 to 130,661 movable cells. These are modified
engineering benchmarks, not the original ICCAD 2017 legalization task or a
comparison with published contest scores.

Development families: `pci_bridge32_a_md1` (29,517 movable cells and four fixed
macros), `fft_2_md2` (32,281), `edit_dist_1_md1` (130,661). The held-out family
is `des_perf_a_md1` (108,288 and four fixed macros). Six variants of one family
are still one circuit family. Owners do not include zero-area IO terminals,
fixed macros or subsequently inserted taps.

## Conversion contract

`consensus_prepare_public source.lef source.def NEW_DIRECTORY single|mixed DENSITY`
is a C++ converter linked to Dali and the configured PhyDB. Its target is
excluded from ordinary builds. It refuses to overwrite a directory. Source
files are read only; the runner records and rechecks hashes.

The source SITE width is the X grid; one tenth of the SITE height is the Y
grid. Source widths and net-pin incidence are retained. Each original height
class becomes that many alternating N/P regions, ten Y sites apiece, with
4/6-site P/N bands. Single-height controls collapse every movable owner to one
region and proportionally scale nominal pin Y. Pins are point abstractions
from the source pin centers; generated LEF uses small symmetric rectangles.
Their corners are quantized to the declared 0.001-micron manufacturing grid
on native import, which the independent audit also checks.
The current native pin model reflects nominal Y under FS and does not deform
pins when wells stretch. Neither routing nor timing signoff is represented.

Requested density means nominal movable area / (core area - fixed-macro area).
The frame preserves the source aspect ratio, with rounding up to the grid.
Both whitespace and raw/core density are reported; final stretching is
separate. Fixed macros retain dimensions and N orientation, with positions
affinely scaled and clamped to the new frame. Unsupported fixed orientations,
masters shared by fixed/movable owners and unplaced ports are rejected.
Any overlap between the relocated fixed macros is rejected before output
creation. The initial frozen 2026-10-31 study exposed eight such invalid
variants; their native attempts remain in that study but are excluded from
quality results. The added converter preflight leaves accepted inputs and
the measured placement binary unchanged.
Movable starts and preplaced IO anchors are scaled/snapped to the new frame;
initial orientations are N. Connectivity, owner mapping, geometry and hashes
are exported in `conversion.json`, `owners.tsv` and `connectivity.tsv`.

Original fences, rail patterns, routing blockages, diffusion constraints,
movement limits and technology are replaced by explicit synthetic P/N rules.
A 2-by-12-site tap has 5/7-site bands. Maximum plug distance is longer than
the core diagonal, so the tested topology uses one column and two boundary
taps per occupied row. It does not qualify dense multi-column tap insertion.
Full stretched geometry requires the companion `.multi_region.*` state;
ordinary DEF with nominal LEF does not encode it alone.

## Reproduction

Build the `dali` and `consensus_prepare_public` C++ targets using the same
configured Release build. Run from any directory with Python 3 for orchestration
and independent file audits (no Python optimization):

```sh
python3 -B run_suite.py --source-root /absolute/ICCAD2017LEFDEF \
  --binaries /absolute/Dali/bin --output /absolute/NEW_STUDY \
  --phase development
python3 -B run_suite.py --source-root /absolute/ICCAD2017LEFDEF \
  --binaries /absolute/Dali/bin --output /absolute/NEW_STUDY \
  --phase held_out
```

Each family has single/mixed variants at 50%, 65% and 80% requested whitespace
density. Native baseline and consensus use the same bounded initialization,
with global and IO placement disabled. Refinement considers all movable owners
and fixes row/order/Y/orientation/stretching. The primary stopping settings
are a cooperative one-second optimizer budget, at most 128 iterations, recovery
every eight iterations and four stagnant checkpoints. Setup, validation and
export lie outside the optimizer budget. An iteration cap/deadline is not a
convergence certificate.

Three repetitions run serially with alternating arm order. Repetition zero is
the declared quality sample; runtime reports the median and all samples.
Per-child wall/CPU/peak RSS, stage timings, native legality and numerical
agreement are recorded separately. Source-state hashes must match across arms
and repetitions. Failures retain logs and any diagnostic state and are not
scored as zero improvement. Conversion has a 120-second watchdog; placement
has a 180-second watchdog. Do not run other heavy jobs during timed execution.

The runner freezes binaries and source hashes before development and refuses
changed software for the held-out phase. All source originals are rechecked
afterward. Independent audits check original/derived/exported topology,
full-rectangle geometry, every region's ownership and NP/stretch equations,
fixed anchors, exported DEF poses, LEF pin offsets and unchanged metadata.
The optional Boost-header-only `consensus_check_objective` C++ target evaluates
serialized fixed-decision pin spans against a 50-decimal-digit reference;
it checks numerical evaluation, not optimality of the fixed-order solve.

Keep quality, floating-point error, legality and runtime as separate columns.
This suite can establish usability and expose defects; it cannot establish
publication novelty or performance parity with the original public legalizers.
