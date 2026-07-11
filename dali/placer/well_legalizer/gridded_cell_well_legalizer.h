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

#include "component_cluster.h"
#include "component_segment.h"
#include "dali/circuit/component.h"
#include "dali/circuit/macro.h"
#include "dali/common/misc.h"
#include "dali/common/placement_snapshot_sink.h"
#include "dali/placer/legalizer/extended_tetris_legalizer.h"
#include "dali/placer/placer.h"
#include "gridded_detailed_placer.h"
#include "gridded_row.h"
#include "space_partitioner.h"
#include "stripe.h"
#include "well_row_completer.h"

namespace dali {

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

  /** Set maximum legalized row width in microns. */
  void SetMaxRowWidth(double max_row_width_microns);

  /** Set the orientation of the first generated row. */
  void SetFirstRowOrientN(bool is_N) { is_first_row_orient_N_ = is_N; }

  /** Load N/P-well parameters from the input circuit. */
  void FetchNpWellParams();

  /** Cache component locations before legalization. */
  void SaveInitialComponentLocation();
  /** Restore component locations and orientations saved before legalization. */
  void RestoreInitialComponentLocation();

  /** Initialize stripes, clusters, and cached parameters. */
  void InitializeWellLegalizer(int cluster_width = -1);

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

  void ReportEffectiveSpaceUtilization();

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
  /** Return x-capacity reserved for taps/end caps in every gridded row. */
  int PhysicalCompletionReservedWidth() const;

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
  void RunClusterOrientationStage();
  std::vector<GriddedRow*> CollectGriddedRows();
  void RunGriddedDetailedPlacementStage();
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
  /** Log why a stripe could not be legalized inside its assigned whitespace. */
  void LogStripeLegalizationFailure(const ClusterStripe& col,
                                    const Stripe& stripe, int column_index,
                                    int stripe_index) const;
  /** Log a summary after component clustering to make failures debuggable. */
  void LogComponentClusteringSummary(int failed_stripe_count) const;
  /** Count component rectangle overlaps after legalization. */
  size_t CountComponentOverlapsInRows() const;

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
  DefaultSpacePartitioner space_partitioner_;
  GriddedDetailedPlacer gridded_detailed_placer_;
  SnapshotCallback snapshot_callback_;
  int snapshot_attempt_ = 0;

  /**** cached well tap cell parameters ****/
  Macro* well_tap_macro_ = nullptr;
  int well_tap_p_height_;
  int well_tap_n_height_;
  int space_to_well_tap_ = 1;

  // list of index loc pair for location sort
  std::vector<ComponentInitialLocation> index_loc_list_;
  std::vector<ClusterStripe> col_list_;  // list of stripes

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
