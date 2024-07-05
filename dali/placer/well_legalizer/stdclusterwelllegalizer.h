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
#ifndef DALI_PLACER_WELLLEGALIZER_STDCLUSTERWELLLEGALIZER_H_
#define DALI_PLACER_WELLLEGALIZER_STDCLUSTERWELLLEGALIZER_H_

#include <map>
#include <tuple>

#include "blockcluster.h"
#include "blocksegment.h"
#include "griddedrow.h"
#include "dali/circuit/block.h"
#include "dali/circuit/block_type.h"
#include "dali/common/misc.h"
#include "dali/placer/legalizer/LGTetrisEx.h"
#include "dali/placer/placer.h"
#include "spacepartitioner.h"
#include "stripe.h"

namespace dali {

class StdClusterWellLegalizer : public Placer {
  friend class Dali;
 public:
  StdClusterWellLegalizer();
  void LoadConf(std::string const &config_file) override;

  void CheckWellStatus();
  void SetStripePartitionMode(int mode) { stripe_mode_ = mode; }
  void SetFirstRowOrientN(bool is_N) { is_first_row_orient_N_ = is_N; }
  void FetchNpWellParams();
  void SaveInitialBlockLocation();
  void InitializeWellLegalizer(int cluster_width = 0);

  void CreateClusterAndAppendSingleWellBlock(Stripe &stripe, Block &blk);
  void AppendSingleWellBlockToFrontCluster(Stripe &stripe, Block &blk);
  void AppendBlockToColBottomUp(Stripe &stripe, Block &blk);
  void AppendBlockToColTopDown(Stripe &stripe, Block &blk);
  void AppendBlockToColBottomUpCompact(Stripe &stripe, Block &blk);
  void AppendBlockToColTopDownCompact(Stripe &stripe, Block &blk);

  bool StripeLegalizationBottomUp(Stripe &stripe);
  bool StripeLegalizationTopDown(Stripe &stripe);
  bool StripeLegalizationBottomUpCompact(Stripe &stripe);
  bool StripeLegalizationTopDownCompact(Stripe &stripe);

  bool BlockClustering();
  bool BlockClusteringLoose();
  bool BlockClusteringCompact();

  bool TrialClusterLegalization(Stripe &stripe);

  double WireLengthCost(GriddedRow *cluster, int l, int r);
  void FindBestLocalOrder(
      std::vector<Block *> &res,
      double &cost,
      GriddedRow *cluster,
      int cur,
      int l,
      int r,
      int left_bound,
      int right_bound,
      int gap,
      int range
  );
  void LocalReorderInCluster(GriddedRow *cluster, int range = 3);
  void LocalReorderAllClusters();

  //void SingleSegmentClusteringOptimization();

  void UpdateClusterOrient();
  void InsertWellTap();

  void CreateEndCapCellTypes();

  void ClearCachedData();
  bool WellLegalize();

  bool StartPlacement() override;

  void ReportEffectiveSpaceUtilization();

  /****member function for file IO****/
  void GenMatlabClusterTable(std::string const &name_of_file);
  void GenMATLABWellTable(
      std::string const &name_of_file,
      int well_emit_mode
  ) override;
  void GenPPNP(std::string const &name_of_file);
  void EmitDEFWellFile(
      std::string const &name_of_file,
      int well_emit_mode,
      bool enable_emitting_cluster = true
  ) override;
  void EmitPPNPRect(std::string const &name_of_file);
  void ExportPpNpToPhyDB(phydb::PhyDB *phydb_ptr);
  void EmitWellRect(std::string const &name_of_file, int well_emit_mode);
  void ExportWellToPhyDB(phydb::PhyDB *phydb_ptr, int well_emit_mode);
  void EmitClusterRect(std::string const &name_of_file);
 private:
  bool is_first_row_orient_N_ = true;

  /**** well parameters ****/
  bool disable_welltap_ = false;
  int num_of_tap_cell_ = 2;
  int max_unplug_length_;
  int well_tap_cell_width_;
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
  std::map<std::tuple<int /* N height */, int /* P height */>, int> pre_end_cap_cell_np_heights_to_type_id;
  std::map<std::tuple<int /* N height */, int /* P height */>, int> post_end_cap_cell_np_heights_to_type_id;

  /**** stripe parameters ****/
  int stripe_mode_ = 0;
  DefaultSpacePartitioner space_partitioner_;

  /**** cached well tap cell parameters ****/
  BlockType *well_tap_cell_ptr_ = nullptr;
  int tap_cell_p_height_;
  int tap_cell_n_height_;
  int space_to_well_tap_ = 1;

  // list of index loc pair for location sort
  std::vector<BlkInitPair> index_loc_list_;
  std::vector<ClusterStripe> col_list_; // list of stripes

  /**** parameters for legalization ****/
  int max_iter_ = 10;

  /**** initial location ****/
  std::vector<int2d> block_init_locations_;

  // dump result
  bool is_dump = false;
  int dump_count = 0;

};

}

#endif //DALI_PLACER_WELLLEGALIZER_STDCLUSTERWELLLEGALIZER_H_
