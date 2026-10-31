# ConsensusPlacer

`dali::ConsensusPlacer` is a separate, optional horizontal refinement stage for
an already legal gridded/mixed-height placement. It derives from `Placer` and
is built into `dalilib`. It coordinates row and incident-net copies through
one shared coordinate per physical owner. Displacement is charged once per
owner and horizontal pin HPWL once per net.

The numerical core was developed and checked in Dali's mixed-height research
work. It uses bounded weighted row projections, net-range proximal updates,
scaled duals and an owner L1 proximal update. Integer recovery satisfies the
whole fixed-order DAG, and only a complete checked improvement replaces the
input/incumbent. Integer recovery is heuristic; convergence residuals do not
certify integer/global optimality. No new publication claim is made here.

## Ordinary Dali flow

Enable it explicitly after gridded legalization:

```sh
dali -lef cells.lef -def input.def -cell cells.cell \
  -enable_gridded_consensus \
  -gridded_consensus_time 0.25 \
  -gridded_consensus_iterations 128 \
  -gridded_consensus_stagnation 4
```

It is disabled by default. The application passes the legalizer's actual
orientation, tap, coverage, spacing and end-cap validation settings to the
separate placer. The optimizer is not embedded in the legalizer. Its placement
metrics use the `consensus` stage; setup/validation and optimizer time are
separate. The command's current row model is the ordinary one-row-per-owner
gridded flow. Multi-region ownership uses the C++ interface below after the
older multi-region assignment/stretching path.

Zero seconds/iterations retains the validated input. Stagnation 0 disables
that stopping condition. Time limits are cooperative, not OS deadlines, and
exclude application setup, physical validation and output. Existing default
placement behavior is unchanged when the flag is absent.

## Separate C++ placer

```cpp
#include "dali/placer/consensus_placer/consensus_placer.h"

dali::ConsensusPlacer placer;
placer.SetCircuit(&circuit);
placer.SetRows(&stripe_columns, dali::ConsensusRowModel::kMultiRegion);
dali::ConsensusPlacerOptions options;
options.time_limit_seconds = 0.25;
options.max_iterations = 128;
options.max_move_sites = 20;
placer.SetOptions(options);
if (!placer.StartPlacement()) {
  // Input coordinates are retained; Report().error describes the failure.
}
const auto& report = placer.Report();
```

Use `kGridded` for `GriddedRow::Components` ownership, or `kMultiRegion` for
the older `ComponentRegions` representation with all occupied rows supplied.
The caller retains ownership/lifetime of the Circuit and row containers.

The objective is weighted horizontal component-pin HPWL in microns plus
optional L1 movement from this invocation's input. `Circuit::WeightedHPWLX`
already returns microns. Separate `Net::IoPinPtrs` entries are omitted by that
existing metric; dummy IO components are included. The objective is not the
public mixed-height benchmark score.

Row assignment, X order, Y, orientation and stretching remain fixed. This
stage does not initialize a placement or perform swaps/vertical moves. The
outer placer/legalizer owns those decisions. Both models preserve connected
source bounds and fixed obstacles; gridded completion is checked by the
existing physical validator. The multi-region model additionally checks
complete region ownership, NP-edge alignment, stretching and full rectangles.

Pass the actual physical-completion configuration in `options.validation` for
the gridded model. Application-specific fences or technology rules can use
`SetAdditionalPlacementCheck`. Callbacks (including cancellation) must be
read-only with respect to the circuit and rows. Rejected trials restore the
input; exceptions return false and restore component poses. Initializers and
library loaders are separate from placement transactions.

`Report()` distinguishes owners included in the model, owners updated during
completed iterations, owners actually moved, committed status, stopping
reason, source/result HPWL, objective, residuals and runtime phases. A legal
source retained on budget/no-improvement stop is successful. Invalid inputs
or numerical/checker exceptions return false with an error.

## Complete multi-region CELL import

Some current PhyDB/Circuit imports retain only one N/P pair per master. For a
complete paired CELL file, restore the regions before constructing row
assignments:

```cpp
#include "dali/placer/consensus_placer/consensus_cell_regions.h"
std::string error;
if (!dali::LoadConsensusCellRegions(circuit, cell_file, nullptr, &error)) {
  // Reject this initialization input.
}
```

All format/geometry checks are staged before updating any master. Master/Pin
identity is preserved. Data must be paired, abutted and on grid; this is an
extension for initialization, not a replacement general LEF/CELL parser or a
guarantee of recovery from allocation failure.

## Validation and scope

Tests cover small exact references, the public class on a non-unit grid,
deadline/cancellation/source retention, malformed rows/options, movement caps,
checker rejection/exception rollback, N/FS asymmetric stretching, complete
CELL import and paired saved adder inputs. The adder geometry is constructed,
sparse physical test data: it is not a foundry library, timing result or
strong-baseline quality claim. No fabrication DRC or general pin-deformation
model is introduced by this stage.

Run the focused checks in a configured build:

```sh
cmake --build build --target consensus_core_test consensus_placer_test
ctest --test-dir build -R '^consensus_' --output-on-failure
```

Numerical helper headers are internal implementation details. New callers
should use the `ConsensusPlacer` class rather than the research adapters.
