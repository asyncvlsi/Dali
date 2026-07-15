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
#ifndef DALI_PLACER_WELL_LEGALIZER_GRIDDED_CELL_WELL_LEGALIZER_H_
#define DALI_PLACER_WELL_LEGALIZER_GRIDDED_CELL_WELL_LEGALIZER_H_

#include <functional>
#include <string>

#include "component_segment.h"
#include "dali/circuit/component.h"
#include "dali/circuit/macro.h"
#include "dali/common/misc.h"
#include "dali/common/placement_snapshot_sink.h"
#include "dali/placer/legalizer/extended_tetris_legalizer.h"
#include "dali/placer/placer.h"
#include "exact_gridded_legalization_window_analyzer.h"
#include "exact_gridded_whole_design_model_builder.h"
#include "gridded_capacity_estimator.h"
#include "gridded_detailed_placer.h"
#include "gridded_row.h"
#include "gridded_row_location_optimizer.h"
#include "ortools_compact_gridded_legalizer.h"
#include "ortools_gridded_stripe_optimizer.h"
#include "space_partitioner.h"
#include "stripe.h"
#include "well_row_completer.h"

namespace dali {

/** One stripe that provisional gridded legalization could not fit. */
struct ProvisionalGriddedPlacementViolation {
  int lx = 0;
  int ly = 0;
  int ux = 0;
  int uy = 0;
  int overflow_height = 0;
  std::vector<int> component_ids;
};

/** Outcome of a provisional gridded legalization pass. */
struct ProvisionalGriddedPlacementResult {
  bool feasible = false;
  double hpwl = 0.0;
  double initial_overflow = 0.0;
  double overflow = 0.0;
  bool used_scavenge = false;
  int balanced_component_count = 0;
  std::vector<int> balanced_component_ids;
  std::vector<std::vector<int>> component_rows;
  std::vector<ProvisionalGriddedPlacementViolation> initial_violations;
  std::vector<ProvisionalGriddedPlacementViolation> violations;
};

/**
 * Standard cluster-based well legalizer and DEF/well-shape emitter.
 *
 * The movable-cell path clusters components into gridded rows, assigns row
 * orientation, and optionally performs local reordering to reduce wirelength.
 * Physical completion then inserts well taps and end caps before the well/PPNP
 * geometry is emitted. Fixed-only designs can reuse the physical completion
 * stages without running movable-cell legalization.
 */
class GriddedCellWellLegalizer : public Placer {
  friend class Dali;

 public:
  GriddedCellWellLegalizer();

  /** Callback used by the application to emit visualization snapshots. */
  using SnapshotCallback = std::function<void(
      const std::string& id, const std::string& label, const std::string& group,
      const std::string& subgroup, int iteration)>;

  /** Set a callback invoked after legalization and gridded detailed stages. */
  void SetSnapshotCallback(SnapshotCallback snapshot_callback);

  /** Load well legalizer configuration. */
  void LoadConf(std::string const& config_file) override;

  /** Verify N/P-well prerequisites on the input circuit. */
  void CheckWellStatus();

  /** Set stripe partitioning mode. */
  void SetStripePartitionMode(int mode) { stripe_mode_ = mode; }

  /** Enable capacity-aware reassignment between neighboring stripes. */
  void SetEnableStripeBalancing(bool enable) {
    enable_stripe_balancing_ = enable;
  }

  /** Enable demand-aware nonuniform stripe boundaries. */
  void SetEnableAdaptiveStripeBoundaries(bool enable) {
    enable_adaptive_stripe_boundaries_ = enable;
  }

  /** Enable wirelength-driven reordering within legalized gridded rows. */
  void SetEnableLocalReorder(bool enable) { enable_local_reorder_ = enable; }

  /** Enable cross-row swaps followed by local reordering. */
  void SetEnableDetailedPlacement(bool enable) {
    enable_detailed_placement_ = enable;
  }

  /** Enable exact gridded cross-row moves into existing row whitespace. */
  void SetEnableDetailedRelocation(bool enable) {
    gridded_detailed_placer_.SetEnableRelocation(enable);
  }

  /** Commit gridded assignment cycles in exact-gain order. */
  void SetEnableDetailedAssignmentBatch(bool enable) {
    gridded_detailed_placer_.SetEnableBatchedAssignmentCycles(enable);
  }

  /** Set the per-component candidate-row cap in gridded detailed placement. */
  void SetDetailedPlacementMaxCandidateRows(int max_candidate_rows) {
    gridded_detailed_placer_.SetMaxCandidateRows(max_candidate_rows);
  }

  /** Configure gridded detailed-placement convergence. */
  void SetDetailedPlacementConvergence(int max_rounds,
                                       double min_relative_improvement) {
    gridded_detailed_placer_.SetMaxRounds(max_rounds);
    gridded_detailed_placer_.SetMinRelativeImprovement(
        min_relative_improvement);
  }

  /** Enable or disable vertical swaps in gridded detailed placement. */
  void SetEnableDetailedVerticalSwap(bool enable) {
    gridded_detailed_placer_.SetEnableVerticalSwap(enable);
  }

  /** Set the high-fanout cutoff used by gridded detailed placement. */
  void SetDetailedPlacementNetIgnoreThreshold(int net_ignore_threshold) {
    gridded_detailed_placer_.SetNetIgnoreThreshold(net_ignore_threshold);
  }

  /** Enable HPWL-aware vertical movement of legal gridded row groups. */
  void SetEnableRowLocationOptimization(bool enable) {
    enable_row_location_optimization_ = enable;
  }

  /** Enable fixed-row CP-SAT X refinement with the shared fanout cutoff. */
  void SetEnableOrToolsRowOptimization(bool enable, int net_ignore_threshold) {
    enable_ortools_row_optimization_ = enable;
    ortools_net_ignore_threshold_ = net_ignore_threshold;
  }

  /** Configure read-only exact analysis of bounded legal row windows. */
  void SetExactLegalizationAnalysis(bool enable, bool analyze_adjacent_rows,
                                    int target_components_per_window,
                                    int maximum_windows,
                                    double maximum_time_seconds_per_window,
                                    int maximum_row_assignment_changes,
                                    int net_ignore_threshold) {
    enable_exact_legalization_analysis_ = enable;
    exact_legalization_analysis_config_.target_components_per_window =
        target_components_per_window;
    exact_legalization_analysis_config_.maximum_components_per_window =
        2 * target_components_per_window;
    exact_legalization_analysis_config_.maximum_windows = maximum_windows;
    exact_legalization_analysis_config_.maximum_time_seconds_per_window =
        maximum_time_seconds_per_window;
    exact_legalization_analysis_config_.maximum_row_assignment_changes =
        maximum_row_assignment_changes;
    exact_legalization_analysis_config_.net_ignore_threshold =
        net_ignore_threshold;
    exact_legalization_analysis_config_.minimum_rows_per_window =
        analyze_adjacent_rows ? 2 : 1;
    exact_legalization_analysis_config_.maximum_row_displacement =
        analyze_adjacent_rows ? 1 : -1;
    exact_legalization_analysis_config_.fix_row_geometry =
        analyze_adjacent_rows;
    exact_legalization_analysis_config_.use_compact_solver =
        analyze_adjacent_rows;
    exact_legalization_analysis_config_.overlap_row_windows =
        analyze_adjacent_rows;
  }

  /**
   * Configure read-only compact CP-SAT analysis over every movable component.
   */
  void SetWholeDesignExactLegalization(bool enable, double maximum_time_seconds,
                                       int number_of_workers,
                                       int maximum_row_displacement,
                                       bool use_solution_hint,
                                       bool log_search_progress,
                                       int net_ignore_threshold) {
    enable_whole_design_exact_legalization_ = enable;
    whole_design_exact_legalization_config_.maximum_time_seconds =
        maximum_time_seconds;
    whole_design_exact_legalization_config_.number_of_workers =
        number_of_workers;
    whole_design_exact_legalization_config_.maximum_row_displacement =
        maximum_row_displacement;
    whole_design_exact_legalization_config_.use_solution_hint =
        use_solution_hint;
    // The monolithic fixed-row model otherwise spends its short diagnostic
    // budget in presolve before recording the validated production incumbent.
    whole_design_exact_legalization_config_.use_presolve = !use_solution_hint;
    whole_design_exact_legalization_config_.log_search_progress =
        log_search_progress;
    whole_design_exact_legalization_config_.validate_solution_hint = true;
    whole_design_exact_net_ignore_threshold_ = net_ignore_threshold;
  }

  /** Configure decomposed exact refinement of finalized gridded stripes. */
  void SetExactStripeOptimization(
      bool enable, double maximum_time_seconds_per_stripe,
      double maximum_total_time_seconds, int maximum_sweeps,
      int target_components_per_model, int number_of_workers,
      int maximum_row_displacement, int maximum_row_assignment_changes,
      bool use_solution_hint, int net_ignore_threshold) {
    enable_exact_stripe_optimization_ = enable;
    exact_stripe_optimizer_config_.maximum_time_seconds_per_stripe =
        maximum_time_seconds_per_stripe;
    exact_stripe_optimizer_config_.maximum_total_time_seconds =
        maximum_total_time_seconds;
    exact_stripe_optimizer_config_.maximum_sweeps = maximum_sweeps;
    exact_stripe_optimizer_config_.target_components_per_model =
        target_components_per_model;
    if (target_components_per_model > 0) {
      exact_stripe_optimizer_config_.maximum_components_per_model =
          2 * target_components_per_model;
    }
    exact_stripe_optimizer_config_.number_of_workers = number_of_workers;
    exact_stripe_optimizer_config_.maximum_row_displacement =
        maximum_row_displacement;
    exact_stripe_optimizer_config_.maximum_row_assignment_changes =
        maximum_row_assignment_changes;
    exact_stripe_optimizer_config_.use_solution_hint = use_solution_hint;
    exact_stripe_optimizer_config_.net_ignore_threshold = net_ignore_threshold;
  }

  /** Set maximum legalized row width in microns. */
  void SetMaxRowWidth(double max_row_width_microns);

  /** Set the orientation of the first generated row. */
  void SetFirstRowOrientN(bool is_N) { is_first_row_orient_N_ = is_N; }

  /** Load N/P-well parameters from the input circuit. */
  void FetchNpWellParams();

  /** Build the capacity model shared by global and final legalization. */
  GriddedCapacityConfig BuildGriddedCapacityConfig(double target_density);

  /** Calibrate gridded demand so target density keeps its raw-area meaning. */
  double EstimateGriddedDemandNormalization(
      const GriddedCapacityConfig& config) const;

  /** Cache component locations before legalization. */
  void SaveInitialComponentLocation();
  /** Restore component locations and orientations saved before legalization. */
  void RestoreInitialComponentLocation();

  /** Initialize stripes, clusters, and cached parameters. */
  void InitializeWellLegalizer(int cluster_width = -1);

  /**
   * Roughly legalize the current global-placement upper bound.
   *
   * This method forms gridded rows and optionally applies mandatory row
   * orientation plus row-location optimization. It always skips detailed
   * placement, taps, end caps, and well geometry. A successful pass commits
   * provisional component coordinates; a failed pass restores every incoming
   * coordinate and orientation.
   */
  ProvisionalGriddedPlacementResult RunProvisionalPlacement(
      bool enable_overflow_balancing = false, bool refine_row_geometry = false);

  /** Release row and stripe state retained by provisional legalization. */
  void ClearProvisionalState();

  void CreateClusterAndAppendSingleWellComponent(Stripe& stripe,
                                                 Component& component);
  void AppendSingleWellComponentToFrontCluster(Stripe& stripe,
                                               Component& component);
  void AppendComponentToColBottomUp(Stripe& stripe, Component& component);
  void AppendComponentToColTopDown(Stripe& stripe, Component& component);
  void AppendComponentToColBottomUpCompact(Stripe& stripe,
                                           Component& component);
  void AppendComponentToColTopDownCompact(Stripe& stripe, Component& component);

  bool StripeLegalizationBottomUp(Stripe& stripe);
  bool StripeLegalizationTopDown(Stripe& stripe);
  bool StripeLegalizationBottomUpCompact(Stripe& stripe);
  bool StripeLegalizationTopDownCompact(Stripe& stripe);

  bool ComponentClustering();
  bool ComponentClusteringLoose();
  bool ComponentClusteringCompact();

  bool TrialClusterLegalization(Stripe& stripe);

  // void SingleSegmentClusteringOptimization();

  void UpdateClusterOrient();

  void ClearCachedData();
  bool WellLegalize();

  bool StartPlacement() override;

  /****member function for file IO****/
  void GenMatlabClusterTable(std::string const& name_of_file);
  void GenMATLABWellTable(std::string const& name_of_file,
                          int well_emit_mode) override;
  void GenPPNP(std::string const& name_of_file);
  void EmitDEFWellFile(std::string const& name_of_file, int well_emit_mode,
                       bool enable_emitting_cluster = true) override;
  void EmitPPNPRect(std::string const& name_of_file);
  void ExportPpNpToPhyDB(phydb::PhyDB* phydb_ptr);
  void EmitWellRect(std::string const& name_of_file, int well_emit_mode);
  void ExportWellToPhyDB(phydb::PhyDB* phydb_ptr, int well_emit_mode);
  void EmitClusterRect(std::string const& name_of_file);
  /** Return current well rectangles in micron coordinates for visualization. */
  std::vector<PlacementWellRect> CollectWellVisualizationRects();

 private:
  struct ComponentPlacementSnapshot;

  /** Return x-capacity reserved for taps/end caps in every gridded row. */
  int PhysicalCompletionReservedWidth() const;

  /** Return the left row margin reserved for physical completion. */
  int PhysicalCompletionLeftMargin() const;

  /** Return the right row margin reserved for physical completion. */
  int PhysicalCompletionRightMargin() const;

  /** Update a row so it can physically fit future tap/end-cap cells. */
  void ReservePhysicalCompletionSpace(GriddedRow* row, bool grows_upward);

  /** Replace missing generated end-cap widths with a usable fallback width. */
  void EnsureUsableEndCapWidths();
  int LeftTapLx(const Stripe& stripe) const;
  int LeftTapUx(const Stripe& stripe) const;
  int RightTapLx(const Stripe& stripe) const;
  int RightTapUx(const Stripe& stripe) const;

  /** Return the boundary-cell configuration for finalized gridded rows. */
  WellRowCompletionConfig BuildRowCompletionConfig() const;

  bool RunComponentClusteringStage();
  /** Trial uniform and adaptive clustering and retain the lower legal HPWL. */
  bool RunBestBoundaryClusteringStage();
  /** Return the column pitch boundaries used by the current partition. */
  std::vector<int> CollectColumnBoundaries() const;
  /** Apply one of the two legal alternating orientation phases to a column. */
  void ApplyColumnOrientationPhase(StripeColumn* column,
                                   bool first_row_orient_n);
  /** Choose column orientation phases using exact weighted HPWL. */
  double OptimizeColumnOrientationPhases();
  void RunClusterOrientationStage();
  void RunRowLocationOptimizationStage();
  /** Refine legal row X coordinates through the optional CP-SAT backend. */
  void RunOrToolsRowOptimizationStage();
  /** Measure bounded exact-legalization headroom without changing placement. */
  void RunExactLegalizationAnalysisStage();
  /**
   * Measure whole-design CP-SAT headroom without changing the placement.
   *
   * The configured row radius controls movement around each component's
   * current row. Stripe membership and the number of active rows remain fixed
   * so this experimental model can be evaluated on complete designs.
   */
  void RunWholeDesignExactLegalizationStage();
  /** Refine finalized stripes with sequential conditional CP-SAT solves. */
  void RunExactStripeOptimizationStage();
  /**
   * Alternate column orientation phases and row Y locations to convergence.
   *
   * Row movement changes the HPWL preference between the two legal
   * orientation phases of a column. Rechecking orientation after each row
   * optimization exposes improvements that the previous one-pass schedule
   * could not see. Both substeps accept exact HPWL improvements only.
   */
  void RunJointOrientationAndRowLocationOptimization();
  std::vector<GriddedRow*> CollectGriddedRows();
  void RunGriddedDetailedPlacementStage();
  /** Run all configured stages after component clustering. */
  void RunPostClusteringStages(bool clustering_succeeded);
  bool RunMovableCellLegalizationStages();
  void RunWellTapStage();
  void RunEndCapStage();
  void RunPhysicalCompletionStages();
  /** Emit a placement snapshot with the current legalization attempt prefix. */
  void EmitSnapshot(const std::string& id, const std::string& label,
                    const std::string& group, const std::string& subgroup = "",
                    int iteration = -1);
  /** Retry strict partitioning with last-column scavenging when needed. */
  bool RetryMovableCellLegalizationWithScavenging();
  /** Retry strict clustering after balancing measured stripe overflow. */
  bool RetryMovableCellLegalizationWithBalancing();
  /** Log why a stripe could not be legalized inside its assigned whitespace. */
  void LogStripeLegalizationFailure(const StripeColumn& col,
                                    const Stripe& stripe, int column_index,
                                    int stripe_index) const;
  /** Save structured failure data for the last clustering attempt. */
  void RecordStripeLegalizationFailure(const Stripe& stripe);
  /** Log a summary after component clustering to make failures debuggable. */
  void LogComponentClusteringSummary(int failed_stripe_count) const;
  /** Count component rectangle overlaps after legalization. */
  size_t CountComponentOverlapsInRows() const;

  /** Return total gridded-row overflow area in grid units. */
  double ProvisionalOverflowArea() const;

  /** Return component ids in physical X order for provisional gridded rows. */
  std::vector<std::vector<int>> CollectProvisionalComponentRows() const;

  /** Apply cheap mandatory row geometry to a provisional legal placement. */
  void RefineProvisionalRowGeometry();

  /** Move a minimal HPWL-ranked set out of overflowing provisional stripes. */
  bool TryBalanceProvisionalPlacement(
      const std::vector<ComponentPlacementSnapshot>& incoming_placement,
      ProvisionalGriddedPlacementResult* result);

  /** Log estimated gridded-row demand before component clustering. */
  void LogEstimatedGriddedCapacity();

  /** Log actual gridded-row area after component clustering. */
  void LogActualGriddedUtilization() const;

  bool is_first_row_orient_N_ = true;

  /**** well parameters ****/
  bool disable_welltap_ = false;
  int well_tap_count_per_cluster_ = 2;
  int max_unplug_length_;
  int well_tap_width_;
  int well_spacing_;

  /**** cell orientation ****/
  bool disable_cell_flip_ = false;

  /**** end cap cell ****/
  bool enable_end_cap_cell_ = false;
  int pre_end_cap_min_width_ = 0;
  int pre_end_cap_min_p_height_ = 0;
  int pre_end_cap_min_n_height_ = 0;
  int post_end_cap_min_width_ = 0;
  int post_end_cap_min_p_height_ = 0;
  int post_end_cap_min_n_height_ = 0;
  /**** stripe parameters ****/
  int stripe_mode_ = 0;
  int max_row_width_ = -1;
  bool enable_stripe_balancing_ = false;
  bool enable_adaptive_stripe_boundaries_ = false;
  double adaptive_boundary_blend_ = 1.0;
  std::vector<int> stripe_boundaries_override_;
  bool enable_local_reorder_ = false;
  bool enable_detailed_placement_ = false;
  bool enable_row_location_optimization_ = false;
  bool enable_ortools_row_optimization_ = false;
  int ortools_net_ignore_threshold_ = 100;
  bool enable_exact_legalization_analysis_ = false;
  ExactGriddedWindowAnalyzerConfig exact_legalization_analysis_config_;
  bool enable_whole_design_exact_legalization_ = false;
  ExactGriddedLegalizationConfig whole_design_exact_legalization_config_;
  int whole_design_exact_net_ignore_threshold_ = 100;
  bool enable_exact_stripe_optimization_ = false;
  OrToolsGriddedStripeOptimizerConfig exact_stripe_optimizer_config_;
  WellSpacePartitioner space_partitioner_;
  GriddedDetailedPlacer gridded_detailed_placer_;
  SnapshotCallback snapshot_callback_;
  int snapshot_attempt_ = 0;
  std::vector<ProvisionalGriddedPlacementViolation> last_clustering_violations_;

  /**** cached well tap cell parameters ****/
  Macro* well_tap_macro_ = nullptr;
  int well_tap_p_height_;
  int well_tap_n_height_;
  int space_to_well_tap_ = 1;
  const Circuit* physical_parameter_circuit_ = nullptr;

  // list of index loc pair for location sort
  std::vector<ComponentInitialLocation> index_loc_list_;
  std::vector<StripeColumn> col_list_;  // list of stripes

  /**** parameters for legalization ****/
  int max_iter_ = 10;

  struct ComponentPlacementSnapshot {
    int lx = 0;
    int ly = 0;
    ComponentOrient orient = N;
  };

  /** Capture every component's placement before a trial legalization change. */
  std::vector<ComponentPlacementSnapshot> CaptureComponentPlacement() const;

  /** Restore every component's placement from a captured snapshot. */
  void RestoreComponentPlacement(
      const std::vector<ComponentPlacementSnapshot>& component_snapshots);

  /**** initial placement before movable-cell well legalization attempts ****/
  std::vector<ComponentPlacementSnapshot> component_init_locations_;

  // dump result
  bool is_dump = false;
  int dump_count = 0;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_GRIDDED_CELL_WELL_LEGALIZER_H_
