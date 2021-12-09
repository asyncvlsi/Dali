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
#include "metarowlegalizer.h"

#include "dali/common/helper.h"
#include "dali/common/logging.h"
#include "dali/common/memory.h"
#include "dali/common/timing.h"
#include "helper.h"

namespace dali {

void MetaRowLegalizer::CheckWellInfo() {
  for (auto &multi_well: p_ckt_->tech().MultiWells()) {
    multi_well.CheckLegality();
  }

  Tech &tech = p_ckt_->tech();
  WellLayer &n_well_layer = tech.NwellLayer();
  double grid_value_x = p_ckt_->GridValueX();
  int same_spacing = std::ceil(n_well_layer.Spacing() / grid_value_x);
  int op_spacing = std::ceil(n_well_layer.OppositeSpacing() / grid_value_x);
  well_spacing_ = std::max(same_spacing, op_spacing);

  BlockType *well_tap_cell_ = (p_ckt_->tech().WellTapCellPtrs()[0]);
  auto *tap_cell_well = well_tap_cell_->MultiWellPtr();
  tap_cell_p_height_ = tap_cell_well->PwellHeight(0);
  tap_cell_n_height_ = tap_cell_well->NwellHeight(0);
}

void MetaRowLegalizer::SetPartitionMode(StripePartitionMode mode) {
  stripe_mode_ = mode;
}

void MetaRowLegalizer::PartitionSpaceAndBlocks() {
  space_partitioner_.SetCircuit(p_ckt_);
  space_partitioner_.SetOutput(&col_list_);
  space_partitioner_.SetReservedSpaceToBoundaries(
      well_spacing_, well_spacing_, 1, 1
  );
  space_partitioner_.SetPartitionMode(stripe_mode_);
  space_partitioner_.StartPartitioning();
}

bool MetaRowLegalizer::StripeLegalizationBottomUp(Stripe &stripe) {
  stripe.cluster_list_.clear();
  stripe.contour_ = stripe.LLY();
  stripe.front_id_ = -1;
  stripe.is_bottom_up_ = true;

  std::sort(
      stripe.block_list_.begin(),
      stripe.block_list_.end(),
      [](const Block *lhs, const Block *rhs) {
        return (lhs->LLY() < rhs->LLY())
            || (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
      }
  );

  size_t processed_blk_cnt = 0;
  //int count = 0;
  while (processed_blk_cnt < stripe.block_list_.size()) {
    stripe.UpdateFrontCluster(tap_cell_p_height_, tap_cell_n_height_);
    processed_blk_cnt = stripe.FitBlocksToFrontSpace(processed_blk_cnt);
    stripe.LegalizeFrontCluster();
    //++count;
    //if (count == 20) {
    //  stripe.UpdateRemainingClusters(tap_cell_p_height_, tap_cell_n_height_);
    //  GenMatlabClusterTable("sc_result");
    //  GenMATLABWellTable("scw", 0);
    //  exit(1);
    //}
  }
  stripe.UpdateRemainingClusters(tap_cell_p_height_, tap_cell_n_height_);

  return true;
}

bool MetaRowLegalizer::BlockClustering() {
  for (auto &col: col_list_) {
    for (auto &stripe: col.stripe_list_) {
      StripeLegalizationBottomUp(stripe);
    }
  }

  return true;
}

bool MetaRowLegalizer::StartPlacement() {
  BOOST_LOG_TRIVIAL(info)
    << "---------------------------------------\n"
    << "Start Meta-row Well Legalization\n";

  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  bool is_successful = true;

  CheckWellInfo();
  PartitionSpaceAndBlocks();
  BlockClustering();

  ReportHPWL();

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  BOOST_LOG_TRIVIAL(info)
    << "(wall time: " << wall_time << "s, cpu time: " << cpu_time << "s)\n";

  ReportMemory();

  return is_successful;
}

void MetaRowLegalizer::GenMatlabClusterTable(std::string const &name_of_file) {
  std::string frame_file = name_of_file + "_outline.txt";
  GenMATLABTable(frame_file);
  GenClusterTable(name_of_file, col_list_);
}

}
