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
#include <memory>
#include <string>
#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/common/placement_snapshot_sink.h"
#include "dali/common/placement_snapshot_writer.h"
#include "dali/placer.h"
#include "dali/placer/detailed_placer/detailed_placer.h"
#include "dali/placer/global_placer/placement_initializer.h"
#include "dali/placer/legalizer/standard_cell_legalizer/standard_cell_legalizer.h"
#include "dali/timing/star_pi_model_estimator.h"

namespace dali {

/** Main application facade that owns the circuit model and placement stages. */
class Dali {
 public:
  /** Read-only snapshot of Dali runtime options after config loading. */
  struct RuntimeOptions {
    std::string log_file_name;
    bool disable_log_prefix = false;
    int num_threads = 1;
    WellPartitionMode well_legalization_mode = WellPartitionMode::kStrict;
    bool disable_global_place = false;
    bool disable_legalization = false;
    bool disable_detailed_place = false;
    bool disable_io_place = false;
    double target_density = -1;
    int net_ignore_threshold = 100;
    int io_metal_layer = 0;
    bool export_well_cluster_matlab = false;
    bool disable_welltap = false;
    bool disable_cell_flip = false;
    double max_row_width = 0;
    bool enable_adaptive_stripe_boundaries = false;
    bool is_standard_cell = false;
    bool enable_filler_cell = false;
    bool enable_end_cap_cell = false;
    bool enable_gridded_global_capacity = false;
    bool enable_gridded_upper_bound_refiner = false;
    bool enable_gridded_upper_bound_balancing = false;
    bool disable_gridded_legalization_feedback = false;
    bool enable_gridded_stripe_balancing = false;
    bool enable_gridded_local_reorder = false;
    bool enable_gridded_detailed_placement = false;
    int gridded_detailed_max_rounds = 6;
    double gridded_detailed_min_relative_improvement = 0.005;
    bool disable_gridded_vertical_swap = false;
    bool enable_gridded_row_y_optimization = false;
    bool enable_shrink_off_grid_die_area = false;
    PlacementInitializerType global_initializer =
        PlacementInitializerType::kUniform;
    GlobalAnchorSchedule global_anchor_schedule = GlobalAnchorSchedule::kDali;
    GlobalGridSchedule global_grid_schedule = GlobalGridSchedule::kDali;
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
    bool save_intermediate_result = false;
    std::string output_name = "dali_out";
    std::string visualization_dir;
    bool gui_debug = false;
    std::string gui_pause = "every_snapshot";
    double debug_placement_region_scale = 1.0;
  };

  Dali(phydb::PhyDB* phy_db_ptr, const std::string& severity_level,
       const std::string& log_file_name = "");
  Dali(phydb::PhyDB* phy_db_ptr, severity severity_level,
       const std::string& log_file_name = "");
  ~Dali() = default;

  using SnapshotSinkFactory =
      std::function<std::unique_ptr<PlacementSnapshotSink>()>;
  void SetGuiSnapshotSinkFactory(SnapshotSinkFactory factory);

  /** Load runtime options from the ACT config database. */
  void ShowParamsList();
  void LoadParamsFromConfig();

  void SetLogPrefix(bool disable_log_prefix);
  void SetNumThreads(int num_threads);

  Circuit& GetCircuit();
  phydb::PhyDB* GetPhyDBPtr();
  RuntimeOptions GetRuntimeOptions() const;

  bool SetIoPlacerGlobalMetalLayer(std::string const& layer_name);
  bool ConfigIoPlacer();
  bool RunIoPinAutoPlacement();
  bool IoPinPlacement(int argc, char** argv);

  bool ShouldPerformTimingDrivenPlacement();
  void InitializeRCEstimator();
#if PHYDB_USE_GALOIS
  void FetchSlacks();
  void InitializeTimingDrivenPlacement();
  void UpdateRCs();
  void PerformTimingAnalysis();
  void UpdateNetWeights();
  void ReportPerformance();
  bool TimingDrivenPlacement(double density, int number_of_threads);
#endif

  /** Run the default placement pipeline used by the main `dali` app. */
  bool StartPlacement(double density = -1, int number_of_threads = -1);

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

  void AddWellTaps(phydb::Macro* cell, double cell_interval_microns,
                   bool is_checker_board);
  bool AddWellTaps(int argc, char** argv);
  bool GlobalPlace(double density, int num_threads = 1);
  bool UnifiedLegalization();

  void ExternalDetailedPlaceAndLegalize(std::string const& engine,
                                        bool load_dp_result = true);

  void ExportToPhyDB();
  void Close();

  /** Export generated end-cap LEF when that flow is enabled. */
  void MaybeExportToLEF(std::string const& input_lef_file_full_name,
                        std::string const& output_lef_name);
  /** Write placement outputs and placement-quality reports to DEF files. */
  void ExportToDEF(std::string const& input_def_file_full_name,
                   std::string const& output_def_name = "circuit");

  void InstantiateIoPlacer();

 private:
  // options
  std::string prefix_ = "dali.";
  severity severity_level_ = severity::info;
  std::string log_file_name_;
  bool disable_log_prefix_ = false;
  int num_threads_ = 1;
  WellPartitionMode well_legalization_mode_ = WellPartitionMode::kStrict;
  bool disable_global_place_ = false;
  bool disable_legalization_ = false;
  bool disable_detailed_place_ = false;
  bool disable_io_place_ = false;
  double target_density_ = -1;
  int net_ignore_threshold_ = 100;
  int io_metal_layer_ = 0;
  bool export_well_cluster_matlab_ = false;
  bool disable_welltap_ = false;
  bool disable_cell_flip_ = false;
  double max_row_width_ = 0;
  bool enable_adaptive_stripe_boundaries_ = false;
  bool is_standard_cell_ = false;
  bool enable_filler_cell_ = false;
  bool enable_end_cap_cell_ = false;
  bool enable_gridded_global_capacity_ = false;
  bool enable_gridded_upper_bound_refiner_ = false;
  bool enable_gridded_upper_bound_balancing_ = false;
  bool disable_gridded_legalization_feedback_ = false;
  bool enable_gridded_stripe_balancing_ = false;
  bool enable_gridded_local_reorder_ = false;
  bool enable_gridded_detailed_placement_ = false;
  int gridded_detailed_max_rounds_ = 6;
  double gridded_detailed_min_relative_improvement_ = 0.005;
  bool disable_gridded_vertical_swap_ = false;
  bool enable_gridded_row_y_optimization_ = false;
  bool enable_shrink_off_grid_die_area_ = false;
  PlacementInitializerType global_initializer_ =
      PlacementInitializerType::kUniform;
  GlobalAnchorSchedule global_anchor_schedule_ = GlobalAnchorSchedule::kDali;
  GlobalGridSchedule global_grid_schedule_ = GlobalGridSchedule::kDali;
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
  bool save_intermediate_result_ = false;
  std::string output_name_ = "dali_out";
  std::string visualization_dir_;
  bool gui_debug_ = false;
  std::string gui_pause_ = "every_snapshot";
  double debug_placement_region_scale_ = 1.0;

  // circuit and placer
  Circuit circuit_;
  phydb::PhyDB* phy_db_ptr_ = nullptr;
  GlobalPlacer gb_placer_;
  StandardCellLegalizer standard_cell_legalizer_;
  ExtendedTetrisLegalizer legalizer_;
  DetailedPlacer detailed_placer_;
  GriddedCellWellLegalizer well_legalizer_;
  FillerCellPlacer filler_cell_placer_;
  std::unique_ptr<IoPlacer> io_placer_;
  std::unique_ptr<StarPiModelEstimator> rc_estimator;

  int max_td_place_num_ = 2;

  static void ReportIoPlacementUsage();

  std::string CreateDetailedPlacementAndLegalizationScript(
      std::string const& engine, std::string const& script_name);

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
  /** Run well tap and end-cap stages for designs with no movable cells. */
  void RunFixedOnlyWellCompletion();
  bool RunWellLegalization();
  /** Run physical completion stages after core placement. */
  bool RunPostPlacementCompletionStages();
  bool RunFillerCellPlacement();
  bool RunIoPinPlacementStage();
  void InitializeVisualizationSnapshots();
  void WriteVisualizationSnapshot(
      const std::string& id, const std::string& label, const std::string& group,
      const std::string& subgroup = "", int iteration = -1,
      std::vector<PlacementWellRect> well_rects = {});
  /** Let live visualization backends repaint before long placement stages. */
  void FlushVisualizationEvents();
  void FinishVisualizationSnapshots();

  bool is_circuit_initialized_ = false;
  std::unique_ptr<PlacementSnapshotSink> snapshot_sink_;
  SnapshotSinkFactory gui_snapshot_sink_factory_;
};

}  // namespace dali

#endif  // DALI_DALI_H_
