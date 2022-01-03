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
#include "stripe.h"

#include <algorithm>

namespace dali {

bool Stripe::HasNoRowsSpillingOut() const {
  if (gridded_rows_.empty()) return true;

  // check if the first row fully inside the placement region
  bool is_first_row_fully_in = gridded_rows_[0].LLY() >= LLY() &&
      gridded_rows_[0].URY() <= URY();
  // check if the last row fully inside the placement region
  bool is_last_row_fully_in = gridded_rows_.back().LLY() >= LLY() &&
      gridded_rows_.back().URY() <= URY();

  return is_first_row_fully_in && is_last_row_fully_in;
}

void Stripe::MinDisplacementAdjustment() {
  for (auto &row: gridded_rows_) {
    row.UpdateMinDisplacementLLY();
  }

  std::sort(
      gridded_rows_.begin(),
      gridded_rows_.end(),
      [](const GriddedRow &cluster0, const GriddedRow &cluster1) {
        return cluster0.MinDisplacementLLY()
            < cluster1.MinDisplacementLLY();
      }
  );

  std::vector<ClusterSegment> segments;

  int sz = (int) gridded_rows_.size();
  int lower_bound = ly_;
  int upper_bound = ly_ + height_;
  for (int i = 0; i < sz; ++i) {
    // create a segment which contains only this block
    GriddedRow &row = gridded_rows_[i];
    double init_y = row.MinDisplacementLLY();
    if (init_y < lower_bound) {
      init_y = lower_bound;
    }
    if (init_y + row.Height() > upper_bound) {
      init_y = upper_bound - row.Height();
    }
    segments.emplace_back(&row, init_y);

    // if this new segment is the only segment, do nothing
    size_t seg_sz = segments.size();
    if (seg_sz == 1) continue;

    // check if this segment overlap with the previous one, if yes, merge these two segments
    // repeats until this is no overlap or only one segment left

    ClusterSegment *cur_seg = &(segments[seg_sz - 1]);
    ClusterSegment *prev_seg = &(segments[seg_sz - 2]);
    while (prev_seg->IsNotOnBottom(*cur_seg)) {
      prev_seg->Merge(*cur_seg, lower_bound, upper_bound);
      segments.pop_back();

      seg_sz = segments.size();
      if (seg_sz == 1) break;
      cur_seg = &(segments[seg_sz - 1]);
      prev_seg = &(segments[seg_sz - 2]);
    }
  }

  for (auto &seg: segments) {
    seg.UpdateClusterLocation();
  }
}

void Stripe::SortBlocksBasedOnLLY() {
  std::sort(
      block_list_.begin(),
      block_list_.end(),
      [](const Block *lhs, const Block *rhs) {
        return (lhs->LLY() < rhs->LLY())
            || (lhs->LLY() == rhs->LLY() && lhs->LLX() < rhs->LLX());
      }
  );
}

void Stripe::SortBlocksBasedOnURY() {
  std::sort(
      block_list_.begin(),
      block_list_.end(),
      [](const Block *lhs, const Block *rhs) {
        return (lhs->URY() < rhs->URY())
            || (lhs->URY() == rhs->URY() && lhs->LLX() < rhs->LLX());
      }
  );
}

void Stripe::SortBlocksBasedOnStretchedURY() {
  std::sort(
      block_list_.begin(),
      block_list_.end(),
      [](const Block *lhs, const Block *rhs) {
        return (lhs->URY() > rhs->URY())
            || (lhs->URY() == rhs->URY() && lhs->LLX() < rhs->LLX());
      }
  );
}

void Stripe::SortBlocksBasedOnYLocation(int criterion) {
  switch (criterion) {
    case 0: {
      SortBlocksBasedOnLLY();
      break;
    }
    case 1: {
      SortBlocksBasedOnURY();
      break;
    }
    case 2: {
      SortBlocksBasedOnStretchedURY();
      break;
    }
    default: {
      DaliExpects(false, "unknown block sorting criterion");
    }
  }
}

void Stripe::PrecomputeWellTapCellLocation(
    bool is_checker_board_mode,
    int tap_cell_interval_grid,
    BlockType *well_tap_type_ptr
) {
  is_checkerboard_mode_ = is_checker_board_mode;
  DaliExpects(tap_cell_interval_grid > 0,
              "Non-positive well-tap cell interval?");
  DaliExpects(well_tap_type_ptr != nullptr, "Well-tap cell is a nullptr?");
  well_tap_cell_width_ = well_tap_type_ptr->Width();
  DaliExpects(width_ > well_tap_cell_width_,
              "Stripe width is smaller than well-tap cell width?");

  std::vector<int> locations;

  if (is_checkerboard_mode_) {
    if (tap_cell_interval_grid & 1) { // if this interval is an odd number
      BOOST_LOG_TRIVIAL(info)
        << "Rounding well tap cell interval from " << tap_cell_interval_grid--
        << " to " << tap_cell_interval_grid << "\n";
    }
    tap_cell_interval_grid = tap_cell_interval_grid / 2;
  }

  // compute how many well-tap cells are needed
  int space_to_left = 0; // TODO: this can be a parameter exposed to users
  int well_tap_cell_loc = lx_ + space_to_left;
  int number_of_well_tap_cell =
      (width_ - space_to_left) / tap_cell_interval_grid + 1;
  for (int i = 0; i < number_of_well_tap_cell; ++i) {
    locations.emplace_back(well_tap_cell_loc);
    well_tap_cell_loc += tap_cell_interval_grid;
  }

  // check if the ux of the last well-tap cell is out of the region
  int last_ux = locations.back() + well_tap_cell_width_;
  int right_most_loc = URX() - well_tap_cell_width_;
  if (last_ux > URX()) {
    locations.back() = right_most_loc;
  } else {
    // check if the last segment is longer than half of the well tap cell interval
    if (URX() - last_ux >= tap_cell_interval_grid / 2.0) {
      locations.emplace_back(right_most_loc);
    }
  }

  // finalize well-tap cell locations
  if (is_checkerboard_mode_) {
    size_t sz = locations.size();
    size_t half_sz = (sz + 1) >> 1; // divide by 2
    well_tap_cell_location_even_.reserve(half_sz);
    well_tap_cell_location_odd_.reserve(half_sz);
    for (size_t i = 0; i < sz; ++i) {
      int lo_loc = locations[i];
      int hi_loc = locations[i] + well_tap_cell_width_;
      if (i & 1) { // if index i is an odd number
        well_tap_cell_location_odd_.emplace_back(lo_loc, hi_loc);
      } else {
        well_tap_cell_location_even_.emplace_back(lo_loc, hi_loc);
      }
    }
  } else {
    size_t sz = locations.size();
    well_tap_cell_location_even_.reserve(sz);
    well_tap_cell_location_odd_.reserve(sz);
    for (size_t i = 0; i < sz; ++i) {
      int lo_loc = locations[i];
      int hi_loc = locations[i] + well_tap_cell_width_;
      well_tap_cell_location_odd_.emplace_back(lo_loc, hi_loc);
      well_tap_cell_location_even_.emplace_back(lo_loc, hi_loc);
    }
  }
}

void Stripe::UpdateFrontClusterUpward(int p_height, int n_height) {
  ++front_id_;
  if (front_id_ >= static_cast<int>(gridded_rows_.size())) {
    gridded_rows_.emplace_back();
  }
  gridded_rows_[front_id_].SetLLX(lx_);
  gridded_rows_[front_id_].SetWidth(width_);

  // determine the orientation and the lower y location of the front cluster
  int ly;
  bool is_orient_N;
  if (front_id_ == 0) {
    ly = LLY();
    is_orient_N = is_first_row_orient_N_;
  } else {
    ly = gridded_rows_[front_id_ - 1].URY();
    is_orient_N = !gridded_rows_[front_id_ - 1].IsOrientN();
  }
  gridded_rows_[front_id_].SetLLY(ly);
  gridded_rows_[front_id_].SetOrient(is_orient_N);

  gridded_rows_[front_id_].UpdateWellHeightUpward(p_height, n_height);
  if (front_id_ & 1) {
    gridded_rows_[front_id_].UpdateSegments(well_tap_cell_location_odd_);
  } else {
    gridded_rows_[front_id_].UpdateSegments(well_tap_cell_location_even_);
  }
}

/****
 * Add following clusters and set orientation accordingly.
 * Add the corresponding block region into these clusters based on row orientation and block orientation.
 *
 * @param p_blk
 */
void Stripe::SimplyAddFollowingClusters(Block *p_blk, bool is_upward) {
  BlockTypeWell *well = p_blk->TypePtr()->WellPtr();
  int region_count = well->RegionCount();
  for (int i = 1; i < region_count; ++i) {
    int row_index = front_id_ + i;
    if (row_index >= static_cast<int>(gridded_rows_.size())) {
      gridded_rows_.emplace_back();
    }
    bool is_orient_N = !gridded_rows_[row_index - 1].IsOrientN();
    gridded_rows_[row_index].SetOrient(is_orient_N);
    int region_id = is_upward ? i : region_count - 1 - i;
    gridded_rows_[row_index].AddBlockRegion(p_blk, region_id, true);
  }
}

bool Stripe::AddBlockToFrontCluster(Block *p_blk, bool is_upward) {
  bool res = gridded_rows_[front_id_].AttemptToAdd(p_blk, is_upward);
  if (!res) return false;

  // add this block to other clusters above the front cluster
  SimplyAddFollowingClusters(p_blk, is_upward);

  return true;
}

size_t Stripe::FitBlocksToFrontSpaceUpward(
    size_t start_id,
    int current_iteration
) {
  std::vector<Block *> legalized_blks;
  std::vector<Block *> skipped_blks;

  size_t blks_sz = block_list_.size();
  for (size_t i = start_id; i < blks_sz; ++i) {
    Block *p_blk = block_list_[i];
    if (!gridded_rows_[front_id_].IsOverlap(p_blk, current_iteration, true)) {
      break;
    }
    if (gridded_rows_[front_id_].IsOrientMatching(p_blk, 0)) {
      if (AddBlockToFrontCluster(p_blk, true)) {
        legalized_blks.push_back(p_blk);
      } else {
        skipped_blks.push_back(p_blk);
      }
    } else {
      skipped_blks.push_back(p_blk);
    }
  }

  // put legalized blocks back to the sorted list
  for (size_t i = 0; i < legalized_blks.size(); ++i) {
    block_list_[i + start_id] = legalized_blks[i];
  }

  // put skipped blocks back to the sorted list
  start_id = start_id + legalized_blks.size();
  for (size_t i = 0; i < skipped_blks.size(); ++i) {
    block_list_[i + start_id] = skipped_blks[i];
  }

  return start_id;
}

void Stripe::LegalizeFrontCluster() {
  gridded_rows_[front_id_].LegalizeSegmentsX();
}

void Stripe::UpdateRemainingClusters(
    int p_height,
    int n_height,
    bool is_upward
) {
  size_t sz = gridded_rows_.size();
  for (size_t i = front_id_ + 1; i < sz; ++i) {
    gridded_rows_[i].SetLLX(lx_);
    gridded_rows_[i].SetWidth(width_);
    gridded_rows_[i].RecomputeHeight(p_height, n_height);
    if (is_upward) {
      gridded_rows_[i].SetLLY(gridded_rows_[i - 1].URY());
    } else {
      gridded_rows_[i].SetURY(gridded_rows_[i - 1].LLY());
    }
  }
}

void Stripe::UpdateBlockStretchLength() {
  for (auto &cluster: gridded_rows_) {
    cluster.InitializeBlockStretching();
  }

  int sz = static_cast<int>(gridded_rows_.size());
  for (int i = 1; i < sz; ++i) {
    GriddedRow &cur_cluster = gridded_rows_[i];
    GriddedRow &pre_cluster = gridded_rows_[i - 1];
    for (auto &blk_region: cur_cluster.blk_regions_) {
      int id = blk_region.region_id;
      if (id >= 1) {
        Block *p_blk = blk_region.p_blk;
        BlockTypeWell *well = blk_region.p_blk->TypePtr()->WellPtr();
        --id;
        int well_edge_distance =
            well->AdjacentRegionEdgeDistance(id, p_blk->IsFlipped());
        int actual_edge_distance =
            (cur_cluster.LLY() + cur_cluster.PNEdge()) -
                (pre_cluster.LLY() + pre_cluster.PNEdge());
        int length = actual_edge_distance - well_edge_distance;
        p_blk->SetStretchLength(id, length);
      }
    }
  }
}

void Stripe::UpdateFrontClusterDownward(int p_height, int n_height) {
  ++front_id_;
  if (front_id_ >= static_cast<int>(gridded_rows_.size())) {
    gridded_rows_.emplace_back();
  }
  gridded_rows_[front_id_].SetLLX(lx_);
  gridded_rows_[front_id_].SetWidth(width_);

  // determine the orientation and the upper y of the front cluster
  int uy;
  bool is_orient_N;
  if (front_id_ == 0) {
    uy = URY();
    if (block_list_.empty()) {
      is_orient_N = is_first_row_orient_N_;
    } else {
      BlockTypeWell *well_ptr = block_list_[0]->TypePtr()->WellPtr();
      bool is_blk_flipped = block_list_[0]->IsFlipped();
      if (is_blk_flipped) {
        is_orient_N = !well_ptr->IsNwellAbovePwell(0);
      } else {
        int region_id = well_ptr->RegionCount() - 1;
        is_orient_N = well_ptr->IsNwellAbovePwell(region_id);
      }
    }
  } else {
    uy = gridded_rows_[front_id_ - 1].LLY();
    is_orient_N = !gridded_rows_[front_id_ - 1].IsOrientN();
  }
  gridded_rows_[front_id_].SetURY(uy);
  gridded_rows_[front_id_].SetOrient(is_orient_N);
  gridded_rows_[front_id_].UpdateWellHeightDownward(p_height, n_height);
  if (front_id_ & 1) {
    gridded_rows_[front_id_].UpdateSegments(well_tap_cell_location_odd_);
  } else {
    gridded_rows_[front_id_].UpdateSegments(well_tap_cell_location_even_);
  }
}

size_t Stripe::FitBlocksToFrontSpaceDownward(
    size_t start_id,
    int current_iteration
) {
  std::vector<Block *> legalized_blks;
  std::vector<Block *> skipped_blks;

  size_t blks_sz = block_list_.size();
  for (size_t i = start_id; i < blks_sz; ++i) {
    Block *p_blk = block_list_[i];
    if (!gridded_rows_[front_id_].IsOverlap(p_blk, current_iteration, false)) {
      break;
    }
    int region_id = p_blk->TypePtr()->WellPtr()->RegionCount() - 1;
    if (gridded_rows_[front_id_].IsOrientMatching(p_blk, region_id)) {
      if (AddBlockToFrontCluster(p_blk, false)) {
        legalized_blks.push_back(p_blk);
      } else {
        skipped_blks.push_back(p_blk);
      }
    } else {
      skipped_blks.push_back(p_blk);
    }
  }

  // put legalized blocks back to the sorted list
  for (size_t i = 0; i < legalized_blks.size(); ++i) {
    block_list_[i + start_id] = legalized_blks[i];
  }

  // put skipped blocks back to the sorted list
  start_id = start_id + legalized_blks.size();
  for (size_t i = 0; i < skipped_blks.size(); ++i) {
    block_list_[i + start_id] = skipped_blks[i];
  }

  return start_id;
}

void Stripe::UpdateBlockYLocation() {
  if (!is_bottom_up_) {
    std::reverse(gridded_rows_.begin(), gridded_rows_.end());
    is_bottom_up_ = true;
  }

  for (auto &row: gridded_rows_) {
    row.LegalizeSegmentsY();
  }
}

size_t Stripe::AddWellTapCells(
    Circuit *p_ckt,
    BlockType *well_tap_type_ptr,
    size_t start_id
) {
  size_t row_cnt = gridded_rows_.size();
  for (size_t i = 0; i < row_cnt; ++i) {
    if (i & 1) {
      start_id = gridded_rows_[i].AddWellTapCells(
          p_ckt, well_tap_type_ptr, start_id, well_tap_cell_location_odd_
      );
    } else {
      start_id = gridded_rows_[i].AddWellTapCells(
          p_ckt, well_tap_type_ptr, start_id, well_tap_cell_location_even_
      );
    }
  }
  return start_id;
}

#if DALI_USE_CPLEX
void Stripe::SaveInitialLocationX() {
  for (auto &blk_ptr: block_list_) {
    init_x_locs_.insert(
        std::unordered_map<Block *, double>::value_type(blk_ptr, blk_ptr->LLX())
    );
  }
}

void Stripe::RestoreInitialLocationX() {
  for (auto &[blk_ptr, init_x]: init_x_locs_) {
    blk_ptr->SetLLX(init_x);
  }
}

void Stripe::SortBlocksInEachRow() {
  for (auto &row: gridded_rows_) {
    row.SortBlockRegions();
  }
}

void Stripe::PopulateVariableArray(IloModel &model, IloNumVarArray &x) {
  IloEnv env = model.getEnv();
  IloInt cnt = 0;
  for (auto &row: gridded_rows_) {
    for (auto &blk_region: row.blk_regions_) {
      Block *blk_ptr = blk_region.p_blk;
      if (blk_ptr_2_tmp_id.find(blk_ptr) == blk_ptr_2_tmp_id.end()) {
        //x.add(IloNumVar(env, lx_, lx_ + width_));
        x.add(IloNumVar(env, 0, IloInfinity));
        blk_ptr_2_tmp_id[blk_ptr] = cnt;
        blk_tmp_id_2_ptr[cnt] = blk_ptr;
        ++cnt;
      }
    }
  }
}

void Stripe::AddVariableConstraints(
    IloModel &model,
    IloNumVarArray &x,
    IloRangeArray &c
) {
  for (auto &row: gridded_rows_) {
    size_t blk_cnt = row.blk_regions_.size();
    for (size_t i = 0; i < blk_cnt; ++i) {
      if (i > 0) {
        Block *blk_ptr0 = row.blk_regions_[i - 1].p_blk;
        IloInt id0 = blk_ptr_2_tmp_id[blk_ptr0];
        int width = blk_ptr0->Width();
        Block *blk_ptr1 = row.blk_regions_[i].p_blk;
        IloInt id1 = blk_ptr_2_tmp_id[blk_ptr1];
        c.add(x[id1] - x[id0] >= width);
      }
    }
  }

  model.add(c);
}

void Stripe::ConstructQuadraticObjective(
    IloModel &model,
    IloNumVarArray &x
) {
  IloEnv env = model.getEnv();
  IloExpr objExpr(env);
  for (auto &row: gridded_rows_) {
    for (auto &blk_region: row.blk_regions_) {
      Block *blk_ptr = blk_region.p_blk;
      double init_x = blk_ptr->X();
      IloInt id = blk_ptr_2_tmp_id[blk_ptr];
      objExpr += 1.0 * x[id] * x[id] - 2 * init_x * x[id];
    }
  }
  IloObjective obj = IloMinimize(env, objExpr);
  model.add(obj);
  objExpr.end();
}

void Stripe::CreateQPModel(
    IloModel &model,
    IloNumVarArray &x,
    IloRangeArray &c
) {
  PopulateVariableArray(model, x);
  AddVariableConstraints(model, x, c);
  ConstructQuadraticObjective(model, x);
}

void Stripe::SolveQPProblem(IloCplex &cplex, IloNumVarArray &var) {
  IloEnv env = var.getEnv();

  IloBool solved = cplex.solve();

  if (solved) {
    //BOOST_LOG_TRIVIAL(info)
    //  << "Solution status = " << cplex.getStatus() << "\n";
    //BOOST_LOG_TRIVIAL(info)
    //  << "Solution value  = " << cplex.getObjValue() << "\n";

    IloNumArray val(env);
    cplex.getValues(val, var);
    IloInt nvars = var.getSize();
    for (IloInt j = 0; j < nvars; ++j) {
      //env.out() << "Variable " << j << ": Value = " << val[j] << endl;
      Block *blk_ptr = blk_tmp_id_2_ptr[j];
      blk_ptr->SetLLX(val[j]);
    }
    val.end();
  } else {
    BOOST_LOG_TRIVIAL(info) << "Problem cannot be solved\n";
  }
}

bool Stripe::OptimizeDisplacementUsingQuadraticProgramming() {
  bool is_successful = true;

  RestoreInitialLocationX();
  SortBlocksInEachRow();

  IloEnv env;
  try {
    // create a QP problem
    IloModel model(env);
    IloNumVarArray var(env);
    IloRangeArray con(env);
    CreateQPModel(model, var, con);

    IloCplex cplex(model);

    // solve the QP problem
    SolveQPProblem(cplex, var);
  } catch (IloException &e) {
    BOOST_LOG_TRIVIAL(error) << "Concert exception caught: " << e << "\n";
    is_successful = false;
  } catch (...) {
    BOOST_LOG_TRIVIAL(error) << "Unknown exception caught" << "\n";
    is_successful = false;
  }

  env.end();
  return is_successful;
}
#endif

Stripe *ClusterStripe::GetStripeMatchSeg(SegI seg, int y_loc) {
  Stripe *res = nullptr;
  for (auto &Stripe: stripe_list_) {
    if ((Stripe.URY() == y_loc) && (Stripe.LLX() == seg.lo)
        && (Stripe.URX() == seg.hi)) {
      res = &Stripe;
      break;
    }
  }
  return res;
}

Stripe *ClusterStripe::GetStripeMatchBlk(Block *blk_ptr) {
  Stripe *res = nullptr;
  double center_x = blk_ptr->X();
  double center_y = blk_ptr->Y();
  for (auto &&Stripe: stripe_list_) {
    if ((Stripe.LLY() <= center_y) &&
        (Stripe.URY() > center_y) &&
        (Stripe.LLX() <= center_x) &&
        (Stripe.URX() > center_x)) {
      res = &Stripe;
      break;
    }
  }
  return res;
}

Stripe *ClusterStripe::GetStripeClosestToBlk(Block *blk_ptr, double &distance) {
  Stripe *res = nullptr;
  double center_x = blk_ptr->X();
  double center_y = blk_ptr->Y();
  double min_distance = DBL_MAX;
  for (auto &Stripe: stripe_list_) {
    double tmp_distance;
    if ((Stripe.LLY() <= center_y) && (Stripe.URY() > center_y) &&
        (Stripe.LLX() <= center_x) && (Stripe.URX() > center_x)) {
      res = &Stripe;
      tmp_distance = 0;
    } else if ((Stripe.LLX() <= center_x) && (Stripe.URX() > center_x)) {
      tmp_distance = std::min(
          std::abs(center_y - Stripe.LLY()),
          std::abs(center_y - Stripe.URY())
      );
    } else if ((Stripe.LLY() <= center_y) && (Stripe.URY() > center_y)) {
      tmp_distance = std::min(
          std::abs(center_x - Stripe.LLX()),
          std::abs(center_x - Stripe.URX())
      );
    } else {
      tmp_distance = std::min(
          std::abs(center_x - Stripe.LLX()),
          std::abs(center_x - Stripe.URX())
      ) + std::min(
          std::abs(center_y - Stripe.LLY()),
          std::abs(center_y - Stripe.URY())
      );
    }
    if (tmp_distance < min_distance) {
      min_distance = tmp_distance;
      res = &Stripe;
    }
  }

  distance = min_distance;
  return res;
}

void ClusterStripe::AssignBlockToSimpleStripe() {
  for (auto &Stripe: stripe_list_) {
    Stripe.block_count_ = 0;
    Stripe.block_list_.clear();
  }

  for (auto &blk_ptr: block_list_) {
    double tmp_dist;
    auto Stripe = GetStripeClosestToBlk(blk_ptr, tmp_dist);
    Stripe->block_count_++;
  }

  for (auto &Stripe: stripe_list_) {
    Stripe.block_list_.reserve(Stripe.block_count_);
  }

  for (auto &blk_ptr: block_list_) {
    double tmp_dist;
    auto Stripe = GetStripeClosestToBlk(blk_ptr, tmp_dist);
    Stripe->block_list_.push_back(blk_ptr);
  }
}

}