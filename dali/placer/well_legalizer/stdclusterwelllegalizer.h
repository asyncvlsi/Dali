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

#include "blockcluster.h"
#include "blocksegment.h"
#include "griddedrow.h"
#include "dali/circuit/block.h"
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
  ~StdClusterWellLegalizer() override;
  void LoadConf(std::string const &config_file) override;

  void CheckWellStatus();
  void SetStripePartitionMode(int32_t mode) { stripe_mode_ = mode; }
  void SetFirstRowOrientN(bool is_N) { is_first_row_orient_N_ = is_N; }
  void FetchNpWellParams();
  void SaveInitialBlockLocation();
  void InitializeWellLegalizer(int32_t cluster_width = 0);

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

  double WireLengthCost(GriddedRow *cluster, int32_t l, int32_t r);
  void FindBestLocalOrder(
      std::vector<Block *> &res,
      double &cost,
      GriddedRow *cluster,
      int32_t cur,
      int32_t l,
      int32_t r,
      int32_t left_bound,
      int32_t right_bound,
      int32_t gap,
      int32_t range
  );
  void LocalReorderInCluster(GriddedRow *cluster, int32_t range = 3);
  void LocalReorderAllClusters();

  //void SingleSegmentClusteringOptimization();

  void UpdateClusterOrient();
  void InsertWellTap();

  void ClearCachedData();
  bool WellLegalize();

  bool StartPlacement() override;

  void ReportEffectiveSpaceUtilization();

  /****member function for file IO****/
  void GenMatlabClusterTable(std::string const &name_of_file);
  void GenMATLABWellTable(
      std::string const &name_of_file,
      int32_t well_emit_mode
  ) override;
  void GenPPNP(std::string const &name_of_file);
  void EmitDEFWellFile(
      std::string const &name_of_file,
      int32_t well_emit_mode,
      bool enable_emitting_cluster = true
  ) override;
  void EmitPPNPRect(std::string const &name_of_file);
  void ExportPpNpToPhyDB(phydb::PhyDB *phydb_ptr);
  void EmitWellRect(std::string const &name_of_file, int32_t well_emit_mode);
  void ExportWellToPhyDB(phydb::PhyDB *phydb_ptr, int32_t well_emit_mode);
  void EmitClusterRect(std::string const &name_of_file);
 private:
  bool is_first_row_orient_N_ = true;

  /**** well parameters ****/
  int32_t max_unplug_length_;
  int32_t well_tap_cell_width_;
  int32_t well_spacing_;

  /**** stripe parameters ****/
  int32_t stripe_mode_ = 0;
  DefaultSpacePartitioner space_partitioner_;

  /**** cached well tap cell parameters ****/
  BlockType *well_tap_cell_ = nullptr;
  int32_t tap_cell_p_height_;
  int32_t tap_cell_n_height_;
  int32_t space_to_well_tap_ = 1;

  // list of index loc pair for location sort
  std::vector<BlkInitPair> index_loc_list_;
  std::vector<ClusterStripe> col_list_; // list of stripes

  /**** parameters for legalization ****/
  int32_t max_iter_ = 10;

  /**** initial location ****/
  std::vector<int2d> block_init_locations_;

  // dump result
  bool is_dump = false;
  int32_t dump_count = 0;

};

}

#endif //DALI_PLACER_WELLLEGALIZER_STDCLUSTERWELLLEGALIZER_H_
