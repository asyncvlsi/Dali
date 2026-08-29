/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#ifndef DALI_DALI_H_
#define DALI_DALI_H_

#include <phydb/phydb.h>

#include <functional>
#include <iosfwd>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/common/placement_snapshot_sink.h"
#include "dali/placer.h"
#include "dali/placer/detailed_placer/detailed_placer.h"
#include "dali/placer/global_placer/placement_initializer.h"
#include "dali/placer/legalizer/standard_cell_legalizer/standard_cell_legalizer.h"
#include "dali/timing/delay_line_feedback.h"
#include "dali/timing/delay_line_gain_probe.h"
#include "dali/timing/adaptive_delay_line_sizing.h"
#include "dali/timing/star_pi_model_estimator.h"
#include "dali/timing/delay_line_sizing_policy.h"
#include "dali/timing/topology_checkpoint_coordinator.h"
#include "dali/timing/topology_checkpoint_host.h"
#include "dali/timing/timing_snapshot.h"

namespace dali {

class DaliCommandProcessor;
class DelayLineReservationApplicationTest;
class DelayLinePlacementRestoreTest;
struct DelayLineChain;

/**
 * Select which derived physical-completion objects are copied into PhyDB.
 * kFull is the backward-compatible default and includes derived well/implant
 * cover components. kPlacementAnchor omits only those covers while retaining
 * ordinary components, taps, rows, and I/O.
 */
enum class PhyDBExportMode {
  kFull,
  kPlacementAnchor,
};

/** Main application facade that owns the circuit model and placement stages. */
class Dali {
public:
  /** Read-only snapshot of Dali runtime options after config loading. */
  struct RuntimeOptions {
    std::string log_file_name;
    bool disable_log_prefix = false;
    int num_threads = 1;
    WellPartitionMode well_legalization_mode = WellPartitionMode::kStrict;
    int well_emit_mode = 1;
    bool disable_global_place = false;
    bool disable_legalization = false;
    bool disable_detailed_place = false;
    bool disable_io_place = false;
    double target_density = -1;
    double timing_period_target = -1;
    bool timing_use_rc = true;
    int rc_min_routing_layer = 0;
    int net_ignore_threshold = 100;
    int io_metal_layer = 0;
    bool disable_welltap = false;
    WellTapPattern well_tap_pattern = WellTapPattern::kRowEnd;
    bool disable_cell_flip = false;
    double max_row_width = 0;
    bool enable_adaptive_stripe_boundaries = false;
    bool is_standard_cell = false;
    bool enable_filler_cell = false;
    bool enable_end_cap_cell = false;
    bool enable_gridded_global_capacity = false;
    bool enable_gridded_upper_bound_refiner = false;
    bool enable_gridded_upper_bound_balancing = false;
    bool enable_gridded_evacuated_component_feedback = false;
    bool disable_gridded_feedback_rollback = false;
    bool enable_gridded_legalization_pressure = false;
    GlobalRefinementFeedbackMode gridded_legalization_feedback_mode =
        GlobalRefinementFeedbackMode::kYRowTransactionalConsistent;
    bool enable_gridded_stripe_balancing = false;
    bool enable_banded_stripe_assignment = false;
    int banded_stripe_assignment_bands = 32;
    double banded_stripe_assignment_min_hpwl_gain = 0.0;
    bool enable_gridded_local_reorder = false;
    bool enable_gridded_detailed_placement = false;
    bool enable_gridded_detailed_relocation = false;
    bool enable_gridded_assignment_batch = false;
    bool enable_gridded_exhaustive_insertion = false;
    int gridded_detailed_max_candidate_rows = 4;
    int gridded_detailed_max_rounds = 6;
    double gridded_detailed_min_relative_improvement = 0.005;
    bool disable_gridded_vertical_swap = false;
    bool enable_gridded_row_y_optimization = false;
    // Experimental and intentionally disabled until downstream quality is
    // consistent across gridded benchmarks.
    bool enable_vertical_hpwl_row_assignment = false;
    // Select baseline or CP-SAT row membership with a one-round detailed
    // placement preview. This remains separate from the assignment stage so
    // the ordinary flow changes only when explicitly requested.
    bool enable_vertical_hpwl_row_assignment_preview = false;
    // Compare local solver candidates after the same one-round detailed
    // placement closure used by the normal gridded flow.
    bool enable_vertical_hpwl_row_assignment_local_closure = false;
    int vertical_hpwl_row_assignment_closure_windows = 64;
    bool enable_ortools_row_optimization = false;
    bool analyze_exact_gridded_legalization = false;
    bool analyze_exact_adjacent_rows = false;
    bool analyze_exact_row_geometry = false;
    int exact_gridded_window_components = 48;
    int exact_gridded_max_windows = 24;
    double exact_gridded_window_time = 0.25;
    int exact_gridded_max_row_changes = -1;
    bool solve_exact_gridded_legalization = false;
    double exact_gridded_solve_time = 3600.0;
    int exact_gridded_row_radius = 0;
    bool exact_gridded_use_solution_hint = true;
    bool exact_gridded_log_search_progress = false;
    bool enable_exact_gridded_stripe_optimization = false;
    double exact_gridded_stripe_time = 5.0;
    double exact_gridded_stripe_total_time = 120.0;
    int exact_gridded_stripe_sweeps = 2;
    int exact_gridded_stripe_components = 0;
    int exact_gridded_stripe_row_radius = 0;
    double exact_gridded_stripe_displacement_weight = 0.0;
    bool exact_gridded_stripe_fixed_row_prepass = false;
    bool exact_gridded_stripe_before_detailed = false;
    bool exact_gridded_stripe_local_closure = false;
    bool enable_exact_gridded_boundary_optimization = false;
    bool exact_gridded_boundary_before_detailed = false;
    bool exact_gridded_boundary_local_closure = false;
    double exact_gridded_boundary_time = 0.1;
    double exact_gridded_boundary_total_time = 120.0;
    int exact_gridded_boundary_components = 64;
    int exact_gridded_boundary_max_changes = 4;
    bool enable_shrink_off_grid_die_area = false;
    PlacementInitializerType global_initializer =
        PlacementInitializerType::kUniform;
    GlobalLalExpansionMode global_lal_expansion_mode =
        GlobalLalExpansionMode::kSymmetric;
    GlobalLalHotspotMode global_lal_hotspot_mode =
        GlobalLalHotspotMode::kComponentArea;
    double global_lal_affine_weight = 0.65;
    GlobalLalMacroBoundaryMode global_lal_macro_boundary_mode =
        GlobalLalMacroBoundaryMode::kOff;
    int global_min_iterations = 10;
    int global_max_iterations = 100;
    StandardCellLegalizerCostMode standard_cell_legalizer_cost_mode =
        StandardCellLegalizerCostMode::kDisplacement;
    int detailed_max_rounds = 1;
    int detailed_max_move_candidates = 1000;
    std::string output_name = "dali_out";
    bool gui_debug = false;
    std::string gui_pause = "every_snapshot";
    double debug_placement_region_scale = 1.0;
  };

  Dali(phydb::PhyDB *phy_db_ptr, const std::string &severity_level,
       const std::string &log_file_name = "");
  Dali(phydb::PhyDB *phy_db_ptr, severity severity_level,
       const std::string &log_file_name = "");
  ~Dali() = default;

  using SnapshotSinkFactory =
      std::function<std::unique_ptr<PlacementSnapshotSink>()>;
  void SetGuiSnapshotSinkFactory(SnapshotSinkFactory factory);

  /**
   * Keep an active GUI available for a command session after placement.
   *
   * The standalone application enables this before running a recipe when
   * `-interactive` is present. The interactive session performs the final GUI
   * close-wait after the user exits the command prompt.
   */
  void SetInteractiveSessionExpected(bool expected);

  /** Load runtime options from the ACT config database. */
  void ShowParamsList();
  void LoadParamsFromConfig();

  void SetLogPrefix(bool disable_log_prefix);
  void SetNumThreads(int num_threads);

  Circuit &GetCircuit();
  phydb::PhyDB *GetPhyDBPtr();
  RuntimeOptions GetRuntimeOptions() const;

  bool SetIoPlacerGlobalMetalLayer(std::string const &layer_name);
  bool ConfigIoPlacer();
  bool RunIoPinAutoPlacement();
  /**
   * Place I/O pins from an argv-style command (interactive API).
   * @return true on success.
   */
  bool IoPinPlacement(int argc, char **argv);

  bool ShouldPerformTimingDrivenPlacement();
  /** Load schema-v1 declarations for adjustable timing-delay meta components.
   */
  bool ReadDelayRepairSites(const std::string &file_name);
  /** Synchronize placement and report timing through an attached timing host.
   */
  bool ReportTiming();
  /** Print mutually exclusive runtime totals accumulated by this Dali session. */
  bool ReportRuntimeBreakdown() const;
  /** Synchronize timing and write an advisory delay-repair plan as JSON. */
  bool WriteTimingRepairPlan(const std::string &file_name);
  /** Synchronize timing and write complete semantic constraint identities. */
  bool WriteTimingConstraintIdentities(
      const std::string &file_name,
      const std::vector<std::string> &replaceable_site_prefixes = {});
  /** Write identities from the already-current timing state. */
  bool WriteCurrentTimingConstraintIdentities(
      const std::string &file_name,
      const std::vector<std::string> &replaceable_site_prefixes = {},
      unsigned long long *identity_digest = nullptr,
      std::size_t *identity_count = nullptr);
  /** Write structural identities without requiring current slack values. */
  bool WriteCurrentTimingConstraintEndpointIdentities(
      const std::string &file_name,
      const std::vector<std::string> &replaceable_site_prefixes = {},
      unsigned long long *identity_digest = nullptr,
      std::size_t *identity_count = nullptr);
  /**
   * Write every constraint's cell/wire decomposition from the current state.
   *
   * Read-only: it captures what the last timing analysis produced and does not
   * refresh timing, touch placement, or change attribution. Uses the full
   * capture rather than the identity-only one, because the identity capture
   * deliberately omits the witnesses this report is made of.
   */
  bool WriteCurrentTimingDecomposition(
      const std::string &file_name,
      const std::vector<std::string> &delay_site_prefixes = {});
  /** Report timing and fail when configured timing constraints are violated. */
  bool CheckTiming();
  /** Multiply physically mapped critical-cycle net weights. */
  bool WeightCriticalCycleNets(double multiplier);

  /**
   * Weight the datapath nets on the fast side of relative-timing constraints.
   *
   * The counterpart to critical-cycle weighting, and the one that matches the
   * bundled-data objective. The critical cycle here *is* the control ring, so
   * weighting it shortens the matched delay lines' own wires -- which lowers
   * the period without letting the delay lines be made any shorter. What the
   * period actually depends on is the datapath: each delay line must cover its
   * stage's logic, so shortening the logic is what permits a smaller delay
   * line and therefore a faster circuit.
   */
  bool WeightFastPathNets(double multiplier);

  /**
   * Zigzag a delay line's interior so its internal nets carry more delay.
   *
   * The lever net weighting cannot reach. Weighting can only make a net
   * shorter, which serves the fast side of a constraint; repairing the slow
   * side needs a net made deliberately longer, and a chain of identical
   * inverters is where the room is because any placer abuts it.
   *
   * Both endpoints keep their locations. The last element drives the next
   * stage's fork root, so moving it would change a neighbouring constraint for
   * an unrelated reason, and pinning the ends is what keeps detours on
   * different delay lines independent.
   *
   * This moves cells and does not legalize; it is the geometry step, and the
   * amplitude is the caller's to choose because the delay a detour buys is not
   * predictable in closed form. Fixed components are reported and skipped
   * rather than silently ignored.
   *
   * `amplitude` is in microns. Placement coordinates are in grid units whose
   * x and y spacings differ, so the shape is computed in microns and converted
   * back; passing grid units through unconverted silently scales the detour by
   * the grid spacing and skews it.
   */
  bool DetourDelayLine(const std::string &name_prefix, double amplitude);

  /**
   * Spread a delay line across rows so consecutive elements sit far apart.
   *
   * The gridded counterpart to `DetourDelayLine`, and the form that survives
   * legalization. Rows here are built by the legalizer rather than declared,
   * and each is exactly tall enough for its tallest cell, so a delay line --
   * being one macro repeated -- defines rows of a single known height wherever
   * its elements land. Placing those elements is therefore not a displacement
   * the legalizer may undo but a lattice it can build against.
   *
   * `separation` is measured in rows. Every hop crosses about that many, so
   * delay grows with it; zero lays the chain out in reading order and is the
   * compact baseline. The reachable range is bounded by available vertical
   * extent rather than by chain length, because consecutive elements need not
   * take adjacent rows and the rows between them belong to whatever else the
   * placer puts there.
   */
  bool SpreadDelayLineAcrossRows(const std::string &name_prefix,
                                 int separation, int column_stride = 1,
                                 int column_pitch = 1);

  /**
   * Register a delay line by name without imposing any geometry on it.
   *
   * The line takes part in constraint attribution, closure reporting and
   * element insertion; where its inverters sit is left to the placer. Use this
   * when latency is to come from inserted elements rather than from wire.
   */
  bool RegisterDelayLine(const std::string &name_prefix);

  /**
   * A delay line asked to stay spread, by name prefix and row separation.
   *
   * Registered rather than applied once, because global placement would
   * otherwise abut the chain again: shortening a delay line's internal nets is
   * precisely what a wirelength objective wants to do.
   */
  /** One pipeline stage asked to occupy a horizontal band, by name prefix. */
  struct StageBandRequest {
    std::vector<std::string> name_prefixes;
    std::vector<int> component_ids;
  };

  /** Declare the next stage band; declaration order is bottom to top. */
  bool DeclareStageBand(const std::vector<std::string> &name_prefixes);

  struct DelayLineSpreadRequest {
    std::string name_prefix;
    /**
     * Whether this line's geometry is imposed on the placer.
     *
     * False registers the line for constraint attribution and element
     * insertion while leaving its cells to ordinary placement. Registration and
     * shaping used to be the same act, so obtaining free placement also
     * deregistered the line and left nothing to attribute a violated
     * constraint to.
     */
    bool shaped = true;
    int separation = 0;
    int column_stride = 1;
    /**
     * Column spacing as a multiple of the element width.
     *
     * The lever of last resort, escalated by the timing controller only once
     * separation is at its cap and still not buying anything. Separation and
     * stride both run out on a short chain -- a fourteen-element line folds to
     * seven columns, so stride has almost nothing to permute -- whereas pitch
     * has no such bound, since spacing columns further apart adds wire without
     * needing more of them.
     */
    int column_pitch = 1;
    double previous_slack = 0.0;
    int previous_separation = -1;
    double gain_per_row = 0.0;
    std::vector<int> matched_constraint_ids;
    double binding_slack = 0.0;
    bool has_binding_slack = false;
    /** Which constraint bound it, so two samples can be told apart. */
    int binding_constraint_id = -1;
  };

  /**
   * Choose each delay line's separation from measured slack, then re-place.
   *
   * The closed loop. Delay lines are declared, so the elements on a violating
   * constraint's slow path are known without inference, and a detour on one is
   * separable: it moves that constraint and leaves the others alone.
   *
   * The step is measured rather than computed. What a row of separation buys
   * cannot be predicted in closed form -- across designs it varies more than
   * twelvefold per micron of added wire, because capacitive loading grows with
   * wire length while RC grows with its square -- so this places, measures the
   * gain per row actually obtained, and corrects. The first round has nothing
   * measured yet and uses `initial_step` only to get a second data point.
   *
   * Growth overshoots by construction, because rows bought while the chain is
   * still nearly abutted add far less wire than later ones, so the rate
   * measured near zero separation understates the eventual one. A second phase
   * therefore bisects the bracket that growth leaves behind; on `wire_repair`
   * that is the difference between separation 83 and separation 48 for the same
   * closure, and separation is bought with wire and routing area.
   *
   * Rounds after the first continue from the previous round's placement, with
   * only the delay lines reshaped to the separation about to be tried. Order
   * matters: reshaping before placing is what makes the carried-over placement
   * a usable anchor, since the round's first solve would otherwise pull toward
   * the previous separation. Re-initializing instead discards a good datapath
   * placement and makes each round an independent search, so consecutive rounds
   * differ by much more than the one variable under test -- measured on
   * `wire_repair`, the gap between the separation the search accepts and the
   * confirming re-place falls from 15.5 ps to 1.7 ps.
   *
   * Some drift remains, because each placement run also inserts physical
   * completion cells that persist into the next round. `margin` absorbs it.
   *
   * Runs at most `max_rounds` rounds across both phases.
   */
  bool CloseTimingWithDelayLineSpread(double margin, int initial_step,
                                      int max_rounds);

  /**
   * Retune delay-line separation from timing inside global placement itself.
   *
   * The same closed loop as `CloseTimingWithDelayLineSpread`, moved in one
   * level. That form pays a whole placement, legalization and physical
   * completion for every separation it tries, and the inserted completion cells
   * persist into the next attempt, so consecutive attempts differ by more than
   * the variable under test. Here one placement runs and the separation is
   * retuned between its iterations, off the trajectory already in progress.
   *
   * The step is still measured rather than computed, for the same reason: what
   * a row buys is not predictable in closed form. `damping` scales each
   * correction, because a step taken mid-placement is applied to a placement
   * that has not settled, and an undamped correction from an unsettled estimate
   * oscillates.
   *
   * Slack measured mid-placement is not the slack of a legal placement -- cells
   * still overlap and nothing has been legalized -- so it reads optimistically,
   * by a stable offset rather than randomly. On `wire_repair` the final slack
   * lands about 23 ps below the margin whatever the margin, so `margin` is what
   * pays for the bias and a margin of zero finishes in violation at -21.4 ps.
   * Calibrate it once per design against the slack the completed flow reports.
   *
   * `max_separation` bounds how far the lever may be pushed. Widening degrades
   * the driven edge -- on `wire_repair` an inverter's output transition goes
   * from 65.6 ps unloaded to 171.6 ps at separation 42 -- and past roughly
   * separation 103 that transition exceeds the library's characterized input
   * slew, so the next stage is timed by extrapolation and the delays stop
   * meaning anything. The binding limit is characterization range, not the
   * declared `default_max_transition`, which is far higher. Reaching the bound
   * is the signal that the residual belongs to the other lever, inserting
   * elements, which adds delay and sharpens the edge instead of blunting it.
   */
  void EnableDelayLineTimingFeedback(double margin, int initial_step,
                                     int warmup, int interval, int freeze,
                                     double damping, int max_separation);
  /** Return the most recently captured placement-synchronized timing state. */
  const TimingSnapshot &LastTimingSnapshot() const {
    return last_timing_snapshot_;
  }
  /** Return the source-level repair actions from the most recent timing report.
   */
  std::vector<TimingRepairSitePlanItem> LastTimingRepairPlan() const;
  /** Translate registered spread requests into shapes for the global placer. */
  void ConfigureDelayLineShapes();
  /** Hand the declared stage bands to the global placer, bottom-up. */
  void ConfigureStageBands();
  /** Build one shape per registered delay line at its current separation. */
  std::vector<GlobalPlacer::DelayLineShape> BuildDelayLineShapes();
  /** Copy ordered delay-line membership for the live visualization. */
  std::vector<PlacementDelayLineVisualization>
  BuildDelayLineVisualization();
  std::vector<PlacementTopologySiteChange>
  BuildTopologyChangeVisualization();
  std::vector<int> BuildTopologyAddedComponentIds();

  /**
   * Attach the latest timing witnesses to a snapshot, if any exist.
   *
   * Only where a timing sample was actually taken. Every global-placement
   * iteration would otherwise carry a full copy of 512 constraints' geometry
   * for a placement whose timing nobody measured, which is both a large copy
   * per animation frame and a lie about when the numbers were taken.
   */
  void PopulateTimingVisualization(PlacementSnapshotMetadata *metadata);

  /** Decision evidence copied forward, one entry per site Dali changed. */
  std::vector<PlacementSizingDecisionEvidence> BuildSizingDecisionEvidence();
  /** Measure slack mid-placement and retune separation from what it shows. */
  bool RetuneDelayLineSeparation(int iteration,
                                 std::vector<GlobalPlacer::DelayLineShape> *shapes);
  void InitializeRCEstimator();
#if PHYDB_USE_GALOIS
  void FetchSlacks();
  void InitializeTimingDrivenPlacement();
  void UpdateRCs();
  void PerformTimingAnalysis();
  void UpdateNetWeights();
  void ReportPerformance();
  /** Run placement with net weights driven by timing.
   * @return true on success. */
  bool TimingDrivenPlacement(double density, int number_of_threads);
#endif

  /** Run the default placement pipeline used by the main `dali` app. */
  bool StartPlacement(double density = -1, int number_of_threads = -1);

  /** Load the LEF technology/library input before the circuit is initialized.
   */
  bool ReadLef(const std::string &file_name);
  /** Load the DEF design input after LEF and before circuit initialization. */
  bool ReadDef(const std::string &file_name);
  /** Load optional gridded-cell well data after LEF/DEF. */
  bool ReadCell(const std::string &file_name);
  /** Set explicit placement grids before loading the circuit model. */
  bool SetPlacementGrids(double grid_x, double grid_y);
  /** Return true when both LEF and DEF inputs are available in PhyDB. */
  bool HasInputDesign() const;

  /**
   * Execute one already-tokenized Dali command.
   *
   * This is the integration point for command hosts such as `interact`, which
   * already provide an argv-style command. Recipes and the standalone
   * interactive prompt use the same dispatcher.
   */
  bool ExecuteCommand(const std::vector<std::string> &arguments);

  /** Tokenize and execute one line in the Dali command language. */
  bool ExecuteCommandLine(const std::string &command_line);

  /**
   * Execute a `.dali` command file.
   *
   * Commands run in order and execution stops at the first malformed or failed
   * command. Errors include the source filename and logical line number.
   */
  bool RunCommandFile(const std::string &file_name);

  /**
   * Open a command-driven session on the loaded design.
   *
   * The design is initialized but placement is not run implicitly. Commands
   * may inspect or edit an existing placement, execute `run placement`, or
   * source a recipe.
   */
  bool RunInteractiveSession(std::istream &input, std::ostream &output,
                             bool show_prompt = true);

  /** Return true when global placement has movable components and nets to use.
   */
  bool ShouldRunGlobalPlacement() const;

  /**
   * Return true when legalization should move ordinary components.
   *
   * Fixed-only designs still need physical completion stages such as well tap
   * and end-cap insertion, but should skip movable-cell legalization.
   */
  bool ShouldRunMovableCellLegalization() const;

  /** Insert well taps of `cell` at a fixed micron pitch. */
  void AddWellTaps(phydb::Macro *cell, double cell_interval_microns,
                   bool is_checker_board);
  /** Insert well taps from an argv-style command (interactive API). */
  bool AddWellTaps(int argc, char **argv);
  /** Run global placement to the given target density.
   * @return true on success. */
  bool GlobalPlace(double density, int num_threads = 1);
  bool UnifiedLegalization();

  void ExternalDetailedPlaceAndLegalize(std::string const &engine,
                                        bool load_dp_result = true);

  /** Export the current placement using the selected PhyDB object set. */
  void ExportToPhyDB(PhyDBExportMode mode = PhyDBExportMode::kFull);
  /** Return the existing gridded validator's final-placement report. */
  GriddedPlacementLegalityReport ValidatePlacementLegality() const {
    return well_legalizer_.ValidateFinalPlacementReport();
  }
  /**
   * Export the current placement and its updated PhyDB representation.
   *
   * A nonempty output name overrides the configured `output_name`. This is the
   * implementation behind the command-language `write-def` command.
   */
  bool ExportPlacement(const std::string &output_name = "");
  /** Return true after an explicit `write-def` command exports the design. */
  bool HasExplicitPlacementExport() const;
  void Close();

  /**
   * Register the authority that supplies netlist changes at checkpoints.
   *
   * Dali keeps the loop: it decides when a checkpoint happens and whether to
   * take one, and it validates and applies whatever comes back. The host only
   * answers the question. Passing nullptr removes it.
   *
   * A registered host is consulted only when a checkpoint schedule is
   * configured; with the default schedule no checkpoint is ever taken, so
   * registering a host does not by itself change a placement.
   */
  void SetTopologyCheckpointHost(TopologyCheckpointHost *host) {
    topology_checkpoint_host_ = host;
  }

  /**
   * Rebind to a PhyDB that was rebuilt underneath this Dali instance.
   *
   * A host that re-elaborates ACT must rebuild PhyDB from the regenerated
   * design, which destroys the one Dali was constructed against. Everything
   * Dali holds into PhyDB is a bare pointer, so it is repointed here rather
   * than left dangling; the RC estimator is rebuilt because it captured the old
   * database at construction.
   *
   * The circuit itself is deliberately not reloaded. Dali owns its placement,
   * and the whole point of a checkpoint is that the placement survives the
   * topology change; correspondence with PhyDB is by component name, which
   * ACT preserves across a delay-site parameter change, so PhyDB renumbering
   * is not a hazard.
   *
   * Audited ownership, rather than a count. Every type in Dali that stores a
   * `phydb::PhyDB *` was enumerated, and each is handled by what it is:
   *
   *   - `circuit_`               repointed; it owns its own technology and
   *                              netlist, so only the pointer is stale.
   *   - `filler_cell_placer_`    repointed.
   *   - `io_placer_`             repointed through `IoPlacer::SetPhyDB`, when
   *                              one exists. Missing this left a dangling
   *                              pointer that survived only because the fixed
   *                              experiment performs no I/O work after a
   *                              checkpoint.
   *   - timing bridge            rebuilt when it was initialized before the
   *                              rebind. The fresh PhyDB needs new ACT pin
   *                              mappings and parasitics nodes, and the RC
   *                              estimator captured the old database.
   *   - `last_timing_snapshot_`  discarded. It holds a pointer *and* describes
   *                              a netlist that no longer exists, so
   *                              repointing it would leave stale measurements
   *                              looking current.
   *   - `StandardRowWellTapInserter` is not a member: it is constructed from
   *                              the current pointer at each use, so it cannot
   *                              go stale.
   *
   * A type added later that stores a PhyDB pointer must be added here.
   */
  void RebindPhyDB(phydb::PhyDB *phy_db_ptr);

  /** Build the RC estimator directly, as an early timing capture would. */
  void InitializeRCEstimatorForTesting() { InitializeRCEstimator(); }
  StarPiModelEstimator *RcEstimatorForTesting() const {
    return rc_estimator.get();
  }

  /** Install and inspect the I/O placer, so a rebind can be proven to reach it. */
  void SetIoPlacerForTesting(std::unique_ptr<IoPlacer> io_placer) {
    io_placer_ = std::move(io_placer);
  }
  const IoPlacer *IoPlacerForTesting() const { return io_placer_.get(); }

  /**
   * Give each ACT-added component a starting position near its own delay line.
   *
   * The host names the new cells but knows nothing about where the placement
   * has put anything, so every addition would otherwise start at the origin and
   * drag its line across the die on the first solve. Seeding at the centroid of
   * the cells already registered under the same prefix is the least
   * presumptuous starting point that is not simply wrong; the placer decides
   * where they actually go. Components with no registered line keep whatever
   * seed the delta carried.
   */
  void SeedTopologyDeltaComponents(TopologyDelta &delta) const;

  /**
   * The whole automatic decision, taken once at one eligible checkpoint.
   *
   * Measures every registered site from current timing, decides with
   * DecideDelayLineSizing, and -- only if there is a request -- asks the host to
   * apply it and holds the returned delta to what was asked for. Declining
   * never reaches the host: there is nothing to apply, and calling it anyway
   * would blur "nothing to do" into "the host had nothing to say".
   */
  TopologyMutationResult HandleTopologyCheckpoint(
      const TopologyCheckpointContext &context);

  /** Sends a decided request to the host and validates the returned delta. */
  /**
   * One delay-line sizing decision between global placement and legalization.
   *
   * The stage boundary, not a checkpoint. Amendment R disqualified timing taken
   * inside the iteration loop: across shared iterations the incremental gain
   * ranged from -26.94 to +368.88 ps per pair, and there were iterations where
   * adding pairs made checkpoint slack worse while improving the final result.
   * Amendment S found that the same measurement taken once the loop has ended
   * predicts canonical final slack to within 0.023 pairs, which is what makes
   * this boundary worth having and the earlier one not.
   *
   * Placement is over when this runs, so nothing resumes into the iteration
   * loop; the remaining stages run on the changed netlist.
   */
  bool RunStageBoundaryTopologySizing();

  /** Run bounded measurement-driven insertion epochs after initial legality. */
  bool RunAdaptiveDelayLineSizingEpochs();

  /** Refresh timing, optionally retaining full path witnesses. */
  bool RefreshTiming(bool capture_witnesses);

  /** Promote added cells to placed once legalization has given them sites. */
  bool PromoteLegalizedTopologyComponents();

  /** Measure, decide, and -- only on a request -- ask the host to apply. */
  TopologyMutationResult DecideAndApplyTopologyChange(
      const TopologyCheckpointContext &context, const std::string &stage);

  TopologyMutationResult ApplyTopologyChangeRequest(
      const TopologyCheckpointContext &context,
      const TopologyChangeBatch &batch);

  /**
   * Record value-only timing snapshots at each named placement state.
   *
   * Observation only: it writes files and changes nothing. Enabling it must
   * leave the trajectory, the final coordinates, the final timing, and the
   * checkpoint decision exactly as they were, which is what the neutrality
   * test asserts.
   *
   * `prefix` names the files; each state appends its own suffix.
   */
  void EnableTimingDomainObservation(const std::string &prefix,
                                     std::vector<std::string> sites);

  /** Captures one named state, if observation is enabled. */
  void ObserveTimingDomain(const std::string &stage, int iteration);

  /**
   * Write the run's typed result manifest.
   *
   * The acceptance path reads this, not the log. Regex over a run log is how a
   * checker ends up asserting on a line that moved, or silently matching
   * nothing -- this project has already had two checks pass while inspecting
   * fewer records than they claimed. A typed artifact either parses or does not.
   */
  bool WriteSizingManifest(const std::string &file_name);

  /**
   * Whether the estimator in use carries the configured routing layer.
   *
   * Public so a test can drive it directly, and so a caller that is about to
   * trust a slack can ask before it does.
   */
  bool VerifyRcConfigurationIsEffective(const std::string &context);

  /** The layer actually in force, which is not always the configured one. */
  int EffectiveRcRoutingLayer() const {
    return rc_estimator == nullptr ? -1 : rc_estimator->MinRoutingLayer();
  }

  /** Per-site characterized gain, from the recipe. Site, ps/pair, and range. */
  void SetDelayLineCharacterization(const std::string &site, double ps_per_pair,
                                    int min_pairs, int max_pairs);
  /** Add one ordered point to a single-anchor incremental response table. */
  bool AddDelayLineResponse(const std::string &site, int current_pairs,
                            int target_pairs, double covered_deficit_ps);

  /**
   * Emit one predetermined request instead of deciding.
   *
   * The fixed experiment travels the same transport as the automatic one --
   * same request struct, same host call, same delta validation -- so that gate
   * keeps testing the path the policy uses rather than a parallel one that
   * could drift away from it. What it skips is only the choosing.
   */
  void SetFixedTopologyRequest(const std::string &site, int current_pairs,
                               int requested_pairs);

  /**
   * The checkpoint schedule, which is Dali's decision and not the host's.
   *
   * `warmup` is the first eligible iteration, `interval` the minimum gap
   * between checkpoints, and `max_checkpoints` a hard cap; zero disables
   * checkpoints entirely, which is the default. Every iteration of the
   * bd_pipeline profile is checkpoint-eligible, so a schedule is chosen for
   * its cost rather than for where an eligible state happens to exist.
   */
  void SetTopologyCheckpointSchedule(int warmup, int interval,
                                     int max_checkpoints) {
    topology_checkpoint_warmup_ = warmup;
    topology_checkpoint_interval_ = interval;
    topology_checkpoint_max_ = max_checkpoints;
  }

  /** Export generated end-cap LEF when that flow is enabled. */
  void MaybeExportToLEF(std::string const &input_lef_file_full_name,
                        std::string const &output_lef_name);
  /** Write placement outputs and placement-quality reports to DEF files. */
  void ExportToDEF(std::string const &input_def_file_full_name,
                   std::string const &output_def_name = "circuit");

  void InstantiateIoPlacer();

private:
  friend class DaliCommandProcessor;
  friend class DelayLineReservationApplicationTest;
  friend class DelayLinePlacementRestoreTest;

  // options
  std::string prefix_ = "dali.";
  severity severity_level_ = severity::info;
  std::string log_file_name_;
  bool disable_log_prefix_ = false;
  int num_threads_ = 1;
  WellPartitionMode well_legalization_mode_ = WellPartitionMode::kStrict;
  int well_emit_mode_ = 1;
  bool disable_global_place_ = false;
  bool disable_legalization_ = false;
  bool disable_detailed_place_ = false;
  bool disable_io_place_ = false;
  double target_density_ = -1;
  double timing_period_target_ = -1;
  /**
   * Cells an applied topology delta added, awaiting a real legal position.
   *
   * They are created UNPLACED and stay that way until legalization has given
   * them a site. Nothing else promotes the status, so without this the export
   * writes them with no coordinates at all.
   */
  std::vector<std::string> components_awaiting_legal_placement_;
  /** ACT-added identities retained after promotion for snapshots and audits. */
  std::vector<std::string> topology_added_component_names_;
  /** The decided request is visible before the apply-only host mutates ACT. */
  std::vector<TopologyChangeRequest> visualization_topology_requests_;
  int topology_generation_ = 0;
  /** Which stage the newest timing sample was taken at, empty when none. */
  std::string last_timing_sample_stage_;
  /** Where the typed run manifest is written, if the recipe asked for one. */
  std::string sizing_manifest_path_;
  /** Everything the manifest reports about the boundary decision. */
  std::vector<DelayLineSiteMeasurement> manifest_boundary_sites_;
  std::vector<SiteVerdict> manifest_verdicts_;
  std::vector<TopologyChangeRequest> manifest_requests_;
  int manifest_added_components_ = 0;
  int manifest_added_nets_ = 0;
  int manifest_retired_nets_ = 0;
  int manifest_rewired_nets_ = 0;
  bool manifest_mutation_applied_ = false;
  size_t manifest_components_before_ = 0;
  size_t manifest_nets_before_ = 0;
  /**
   * Cumulative wall time for peer-reviewable placement-flow accounting.
   *
   * Timing nested inside an ACT topology refresh is subtracted from the host
   * bucket, so every leaf total printed by ReportRuntimeBreakdown is mutually
   * exclusive. The placement total is an inclusive scope and is labelled as
   * such in the report rather than mixed into the leaf percentages.
   */
  struct RuntimeBreakdown {
    double placement_scope_wall_seconds = 0.0;
    double global_placement_wall_seconds = 0.0;
    double legalization_wall_seconds = 0.0;
    double io_placement_wall_seconds = 0.0;
    double filler_placement_wall_seconds = 0.0;
    double topology_host_wall_seconds = 0.0;
    double topology_bookkeeping_wall_seconds = 0.0;
    double timing_initialize_wall_seconds = 0.0;
    double timing_export_locations_wall_seconds = 0.0;
    double timing_update_rc_wall_seconds = 0.0;
    double timing_analysis_wall_seconds = 0.0;
    double timing_witness_capture_wall_seconds = 0.0;
    double timing_other_wall_seconds = 0.0;
    int timing_refreshes = 0;
    int topology_host_calls = 0;
    int placement_runs = 0;
    int global_placement_runs = 0;
    int legalization_runs = 0;
    int io_placement_runs = 0;
    int filler_placement_runs = 0;
  } runtime_breakdown_;
  /** Size every eligible site at once rather than only the worst one. */
  bool topology_batch_sizing_ = false;
  /** Whether an uncharacterized short site fails the run or is excluded. */
  bool topology_batch_require_characterized_ = false;
  /** Whether the recipe asked for one sizing decision at the stage boundary. */
  bool topology_stage_boundary_sizing_ = false;
  /** Learn delay response online through repeated legal insertion epochs. */
  bool topology_adaptive_sizing_ = false;
  int topology_adaptive_probe_pairs_ = 0;
  int topology_adaptive_max_step_pairs_ = 0;
  int topology_adaptive_max_epochs_ = 0;
  int topology_adaptive_max_nonpositive_probes_ = 0;
  std::vector<AdaptiveDelayLineHistory> adaptive_sizing_histories_;
  std::vector<ConstraintAttributionEvidence>
      adaptive_constraint_attribution_evidence_;
  struct AdaptiveEpochManifest {
    int epoch = 0;
    AdaptiveSizingOutcome outcome = AdaptiveSizingOutcome::kInvalid;
    std::vector<AdaptiveDelayLineSample> samples;
    std::vector<AdaptiveSiteDecision> decisions;
    int added_components = 0;
    int added_nets = 0;
    int rewired_nets = 0;
    std::string reason;
  };
  std::vector<AdaptiveEpochManifest> manifest_adaptive_epochs_;
  /** At most one, per run, whatever it decided. */
  bool stage_boundary_attempted_ = false;
  bool timing_use_rc_ = true;
  int rc_min_routing_layer_ = 0;
  /** Increments per observed sample, so a stale record can be told from a new one. */
  long long timing_generation_ = 0;
  int net_ignore_threshold_ = 100;
  int io_metal_layer_ = 0;
  bool disable_welltap_ = false;
  WellTapPattern well_tap_pattern_ = WellTapPattern::kRowEnd;
  bool disable_cell_flip_ = false;
  double max_row_width_ = 0;
  bool enable_adaptive_stripe_boundaries_ = false;
  bool is_standard_cell_ = false;
  bool enable_filler_cell_ = false;
  bool enable_end_cap_cell_ = false;
  bool enable_gridded_global_capacity_ = false;
  bool enable_gridded_upper_bound_refiner_ = false;
  bool enable_gridded_upper_bound_balancing_ = false;
  bool enable_gridded_evacuated_component_feedback_ = false;
  bool disable_gridded_feedback_rollback_ = false;
  bool enable_gridded_legalization_pressure_ = false;
  GlobalRefinementFeedbackMode gridded_legalization_feedback_mode_ =
      GlobalRefinementFeedbackMode::kYRowTransactionalConsistent;
  bool enable_gridded_stripe_balancing_ = false;
  bool enable_banded_stripe_assignment_ = false;
  int banded_stripe_assignment_bands_ = 32;
  double banded_stripe_assignment_min_hpwl_gain_ = 0.0;
  bool enable_gridded_local_reorder_ = false;
  bool enable_gridded_detailed_placement_ = false;
  bool enable_gridded_detailed_relocation_ = false;
  bool enable_gridded_assignment_batch_ = false;
  bool enable_gridded_exhaustive_insertion_ = false;
  int gridded_detailed_max_candidate_rows_ = 4;
  int gridded_detailed_max_rounds_ = 6;
  double gridded_detailed_min_relative_improvement_ = 0.005;
  bool disable_gridded_vertical_swap_ = false;
  bool enable_gridded_row_y_optimization_ = false;
  bool enable_vertical_hpwl_row_assignment_ = false;
  bool enable_vertical_hpwl_row_assignment_preview_ = false;
  bool enable_vertical_hpwl_row_assignment_local_closure_ = false;
  int vertical_hpwl_row_assignment_closure_windows_ = 64;
  bool enable_ortools_row_optimization_ = false;
  bool analyze_exact_gridded_legalization_ = false;
  bool analyze_exact_adjacent_rows_ = false;
  bool analyze_exact_row_geometry_ = false;
  int exact_gridded_window_components_ = 48;
  int exact_gridded_max_windows_ = 24;
  double exact_gridded_window_time_ = 0.25;
  int exact_gridded_max_row_changes_ = -1;
  bool solve_exact_gridded_legalization_ = false;
  double exact_gridded_solve_time_ = 3600.0;
  int exact_gridded_row_radius_ = 0;
  bool exact_gridded_use_solution_hint_ = true;
  bool exact_gridded_log_search_progress_ = false;
  bool enable_exact_gridded_stripe_optimization_ = false;
  double exact_gridded_stripe_time_ = 5.0;
  double exact_gridded_stripe_total_time_ = 120.0;
  int exact_gridded_stripe_sweeps_ = 2;
  int exact_gridded_stripe_components_ = 0;
  int exact_gridded_stripe_row_radius_ = 0;
  double exact_gridded_stripe_displacement_weight_ = 0.0;
  bool exact_gridded_stripe_fixed_row_prepass_ = false;
  bool exact_gridded_stripe_before_detailed_ = false;
  bool exact_gridded_stripe_local_closure_ = false;
  bool enable_exact_gridded_boundary_optimization_ = false;
  bool exact_gridded_boundary_before_detailed_ = false;
  bool exact_gridded_boundary_local_closure_ = false;
  double exact_gridded_boundary_time_ = 0.1;
  double exact_gridded_boundary_total_time_ = 120.0;
  int exact_gridded_boundary_components_ = 64;
  int exact_gridded_boundary_max_changes_ = 4;
  bool enable_shrink_off_grid_die_area_ = false;
  PlacementInitializerType global_initializer_ =
      PlacementInitializerType::kUniform;
  GlobalLalExpansionMode global_lal_expansion_mode_ =
      GlobalLalExpansionMode::kSymmetric;
  GlobalLalHotspotMode global_lal_hotspot_mode_ =
      GlobalLalHotspotMode::kComponentArea;
  double global_lal_affine_weight_ = 0.65;
  GlobalLalMacroBoundaryMode global_lal_macro_boundary_mode_ =
      GlobalLalMacroBoundaryMode::kOff;
  int global_min_iterations_ = 10;
  int global_max_iterations_ = 100;
  StandardCellLegalizerCostMode standard_cell_legalizer_cost_mode_ =
      StandardCellLegalizerCostMode::kDisplacement;
  int detailed_max_rounds_ = 1;
  int detailed_max_move_candidates_ = 1000;
  std::string output_name_ = "dali_out";
  std::string input_lef_file_name_;
  std::string input_def_file_name_;
  std::string input_cell_file_name_;
  bool has_explicit_placement_export_ = false;
  bool gui_debug_ = false;
  std::string gui_pause_ = "every_snapshot";
  bool interactive_session_expected_ = false;
  double debug_placement_region_scale_ = 1.0;

  // circuit and placer
  Circuit circuit_;
  phydb::PhyDB *phy_db_ptr_ = nullptr;
  GlobalPlacer gb_placer_;
  StandardCellLegalizer standard_cell_legalizer_;
  ExtendedTetrisLegalizer legalizer_;
  DetailedPlacer detailed_placer_;
  GriddedCellWellLegalizer well_legalizer_;
  FillerCellPlacer filler_cell_placer_;
  std::unique_ptr<IoPlacer> io_placer_;
  std::unique_ptr<StarPiModelEstimator> rc_estimator;
  TopologyCheckpointHost *topology_checkpoint_host_ = nullptr;
  int topology_checkpoint_warmup_ = 0;
  int topology_checkpoint_interval_ = 1;
  int topology_checkpoint_max_ = 0;
  /** Eligibility, sizing bounds, and per-site gains, all from the recipe. */
  CheckpointEligibilityConfig checkpoint_eligibility_;
  double topology_sizing_margin_ps_ = 0.0;
  int topology_sizing_max_added_pairs_ = 0;
  int topology_sizing_max_pairs_ = 0;
  struct SiteCharacterization {
    double ps_per_pair = 0.0;
    int min_pairs = 0;
    int max_pairs = 0;
  };
  std::map<std::string, SiteCharacterization> delay_line_characterization_;
  struct SiteResponseCharacterization {
    int current_pairs = 0;
    std::vector<DelayLineSiteMeasurement::ResponsePoint> points;
  };
  std::map<std::string, SiteResponseCharacterization>
      delay_line_response_characterization_;
  bool timing_domain_observation_ = false;
  /** Capture expensive timing-domain evidence on every global iteration. */
  bool timing_observe_global_iterations_ = true;
  std::string timing_domain_prefix_;
  std::vector<std::string> timing_domain_sites_;
  bool has_fixed_topology_request_ = false;
  TopologyChangeRequest fixed_topology_request_;
  TimingSnapshot last_timing_snapshot_;
  std::vector<DelayRepairSite> delay_repair_sites_;
  std::vector<DelayLineSpreadRequest> delay_line_spread_requests_;
  std::vector<StageBandRequest> stage_band_requests_;
  /**
   * Whether the controller may grow separation when stride is not enough.
   *
   * False makes the column permutation the sole timing mechanism, so a line
   * that cannot close reports its residual deficit instead of consuming die
   * height chasing it. That is the useful outcome when the intended remedy is
   * inserting elements: a deficit in picoseconds sizes the insertion, whereas a
   * line pinned at the separation cap only says it gave up.
   */
  bool delay_line_separation_escalation_ = false;
  /** Report each line's closure state and residual deficit after placement. */
  void ReportDelayLineClosure(bool timing_is_current = false);
  std::vector<std::pair<int, PlaceStatus>> delay_line_legalization_statuses_;
  int delay_line_fold_count_ = 1;
  /** In-placement retuning state; see EnableDelayLineTimingFeedback. */
  struct DelayLineFeedbackState {
    bool enabled = false;
    double margin = 0.0;
    int initial_step = 1;
    int warmup = 0;
    int interval = 1;
    int freeze = 0;
    double damping = 1.0;
    int max_separation = 0;
  };
  DelayLineFeedbackState delay_line_feedback_;
  std::vector<DelayLineFeedbackEvent> pending_delay_line_feedback_events_;
  bool timing_analysis_initialized_ = false;

  int max_td_place_num_ = 2;

  static void ReportIoPlacementUsage();

  std::string
  CreateDetailedPlacementAndLegalizationScript(std::string const &engine,
                                               std::string const &script_name);

  void ExportOrdinaryComponentsToPhyDB();
  void ExportWellTapCellsToPhyDB();
  void ExportFillerCellsToPhyDB();
  void ExportComponentsToPhyDB();
  void ExportIoPinsToPhyDB();
  void ExportMiniRowsToPhyDB();
  void ExportPpNpToPhyDB();
  void ExportWellToPhyDB();
  void InitializeCircuitFromPhyDBIfNeeded();

  /** Apply explicit `StartPlacement` arguments before the flow starts. */
  void ApplyPlacementOverrides(double density, int number_of_threads);
  /** Set one command-language runtime option after validating its value. */
  bool SetRuntimeOption(const std::string &name, const std::string &value);
  /** Initialize the circuit model and reset metrics for a standalone run. */
  void InitializeMainPlacementCircuit();
  /** Compute and record certified HPWL lower bounds for this circuit. */
  void RecordPlacementLowerBounds();
  /** Enlarge the circuit placement boundary for controlled debug experiments.
   */
  void ApplyDebugPlacementRegionScale();
  /** Choose the target density when the user did not provide one. */
  void ResolveTargetDensity();
  /** Return true when the loaded design has at least one movable component. */
  bool HasMovableComponents() const;
  /** Return true when the loaded design has at least one net. */
  bool HasNets() const;
  /** Run global placement and optional global-placement debug export. */
  bool RunGlobalPlacementStage();
  /** Run the configured legalization path and optional legalization export. */
  bool RunLegalizationStage();
  /** Run global placement and legalization before post-placement completion. */
  bool RunCorePlacementStages();
  bool RunStandardCellLegalization();
  /** Run HPWL-improving detailed placement after legal standard-cell placement.
   */
  bool RunDetailedPlacement();
  /** Configure shared options before either well legalization path runs. */
  void ConfigureWellLegalizer();
  /** Reserve registered delay-line cells at their requested legal positions. */
  bool ReserveDelayLineComponentsForLegalization();
  /** Restore delay-line placement statuses after the legalizer has completed. */
  void RestoreDelayLineComponentStatuses();
  /** Return the fold count needed to fit a delay line in the current region. */
  /** Return the maximum separation that fits the current folded footprint. */
  /** Return true when a delay line's band has more room below its head. */
  bool ShouldExtendDelayLineDownward(const DelayLineChain& chain,
                                     const Macro* macro) const;
  int MaxDelayLineSeparation(const DelayLineChain& chain,
                             const Macro* macro) const;
  /** Cap separation by the number of legal rows available to its folds. */
  int ClampDelayLineSeparation(const DelayLineChain& chain,
                               const Macro* macro, int separation,
                               const char* reason) const;
  /** Declared metadata plus registered delay lines, for one timing capture. */
  std::vector<DelayRepairSite> EffectiveDelayRepairSites(
      const std::vector<std::string> &additional_prefixes = {}) const;
  /** Digest of every I/O pin's placement, for boundary attribution. */
  unsigned long long IoPinPlacementDigest();
  /** Semantic identity of a constraint id in the last timing snapshot. */
  std::string BindingConstraintIdentity(int constraint_id) const;
  int DelayLineRequestColumnStride(const std::string& name_prefix) const;
  int DelayLineRequestColumnPitch(const std::string& name_prefix) const;
  /** Move a delay line's movable cells only, with no request bookkeeping. */
  void MoveDelayLineCells(const DelayLineChain& chain, Macro* macro,
                          int separation, int column_stride, int column_pitch,
                          std::vector<Component>* components, int* moved,
                          int* fixed);
  /** Placement of every cell a line's shape may move, for exact restore. */
  std::vector<ComponentPlacement> SnapshotDelayLinePlacement(
      const std::string& name_prefix);
  void RestoreDelayLinePlacement(
      const std::vector<ComponentPlacement>& placements);
  /**
   * Measure every site's gain against one frozen placement.
   *
   * Returns one probe per proposed site, each restored to the baseline before
   * the next was tried. Nothing here applies an accepted shape; that happens
   * once, together, after every site has been measured.
   */
  std::vector<IsolatedGainProbe> ProbeDelayLineGains(
      int iteration, const std::vector<ProbeSiteRequest>& requests);
  /** Run well tap and end-cap stages for designs with no movable cells. */
  void RunFixedOnlyWellCompletion();
  bool RunWellLegalization();
  bool RunFillerCellPlacement();
  bool RunIoPinPlacementStage();
  std::vector<PlacementSnapshotStage> ExpectedSnapshotStages() const;
  void InitializeVisualizationSnapshots();
  void WriteVisualizationSnapshot(
      const std::string &id, const std::string &label, const std::string &group,
      const std::string &subgroup = "", int iteration = -1,
      std::vector<PlacementWellRect> well_rects = {});
  /** Publish the accepted feedback updates after their shapes are applied. */
  void PublishDelayLineFeedbackSnapshots(int iteration);
  /** Refresh the live GUI after a state-changing interactive command. */
  void WriteInteractiveCommandSnapshot(const std::string &command);
  /** Let live visualization backends repaint before long placement stages. */
  void FlushVisualizationEvents();
  void FinishVisualizationSnapshots();
  /** Log each registered delay line's span at a placement-stage boundary. */
  void LogDelayLineStageSpans(const std::string& stage);

  bool is_circuit_initialized_ = false;
  std::unique_ptr<PlacementSnapshotSink> snapshot_sink_;
  SnapshotSinkFactory gui_snapshot_sink_factory_;
};

} // namespace dali

#endif // DALI_DALI_H_
