//
// Created by Yihang Yang on 12/22/19.
//

#include "welllegalizer.h"

#include <algorithm>

WellLegalizer::WellLegalizer() : LGTetrisEx() {
  max_iter_ = 20;
  legalize_from_left_ = true;
}

void WellLegalizer::InitWellLegalizer() {
  Assert(circuit_ != nullptr, "Well Legalization fail: no input circuit!");
  Tech *tech_parm = circuit_->GetTech();
  Assert(tech_parm != nullptr, "Well Legalization fail: no technology parameters found!");

  WellLayer *n_layer, *p_layer;
  n_layer = tech_parm->GetNLayer();
  p_layer = tech_parm->GetPLayer();
  Assert(n_layer != nullptr, "Well Legalization fail: no N well parameters found!");
  Assert(p_layer != nullptr, "Well Legalization fail: no P well parameters found!");

  n_max_plug_dist_ = std::ceil(n_layer->MaxPlugDist() / circuit_->GetGridValueX());
  p_max_plug_dist_ = std::ceil(p_layer->MaxPlugDist() / circuit_->GetGridValueX());
  nn_spacing_ = std::ceil(n_layer->Spacing() / circuit_->GetGridValueX());
  pp_spacing_ = std::ceil(p_layer->Spacing() / circuit_->GetGridValueX());
  np_spacing_ = std::ceil(p_layer->OpSpacing() / circuit_->GetGridValueX());
  n_min_width_ = std::ceil(n_layer->Width() / circuit_->GetGridValueX());
  p_min_width_ = std::ceil(p_layer->Width() / circuit_->GetGridValueX());

  RowWellStatus tmp_row_well_status(INT_MAX, false);
  row_well_status_.resize(RegionTop() - RegionBottom() + 1, tmp_row_well_status);

  InitLegalizer();

  std::vector<Block> &block_list = *BlockList();
  int sz = block_list.size();
  init_loc_.resize(sz);
  for (int i = 0; i < sz; ++i) {
    init_loc_[i].x = block_list[i].LLX();
    init_loc_[i].y = block_list[i].LLY();
  }
}

void WellLegalizer::MarkSpaceWellLeft(Block const &block, int p_row) {
  /****
   * Mark the space used by this block by changing the start point of available space in each related row
   * ****/
  int lo_row = StartRow((int) std::round(block.LLY()));
  int last_p_row = lo_row + p_row - 1;
  int hi_row = EndRow((int) std::round(block.URY()));

  /*if (end_row >= int(row_well_status_.size())) {
    //std::cout << "  ly:     " << int(block.LLY())       << "\n"
    //          << "  height: " << block.Height()   << "\n"
    //          << "  top:    " << Top()    << "\n"
    //          << "  bottom: " << Bottom() << "\n";
    Assert(false, "Cannot use space out of range");
  }*/

  assert(hi_row < tot_num_rows_);
  assert(lo_row >= 0);

  int end_x = int(block.URX());

  for (int i = lo_row; i <= hi_row; ++i) {
    block_contour_[i] = end_x;
    row_well_status_[i].is_n = (i > last_p_row);
  }
}

void WellLegalizer::UpdatePNBoundary(Block const &block) {
  /****
   * 1. If the newly placed gate block the alignment line, remove those lines.
   * 2. Adding a new line, which is the P/N boundary of this block.
   * ****/
  auto it_lower = p_n_boundary_.lower_bound(block.LLY());
  auto it_upper = p_n_boundary_.upper_bound(block.URY());
  p_n_boundary_.erase(it_lower, it_upper);
  p_n_boundary_.insert(block.LLY() + block.TypePtr()->WellPtr()->PNBoundary());
}

bool WellLegalizer::FindLocation(Block &block, int2d &res) {
  /****
   * For each row:
   *    Find the non-overlap location
   *    Move the location rightwards to respect well rules if necessary
   *
   * Cost function is displacement
   * ****/
  int init_x = int(block.LLX());
  int init_y = int(block.LLY());

  int height = int(block.Height());
  int start_row = 0;
  int end_row = RegionTop() - RegionBottom() - height;
  //std::cout << "    Starting row: " << start_row << "\n"
  //          << "    Ending row:   " << end_row   << "\n"
  //          << "    Block Height: " << block.Height() << "\n"
  //          << "    Top of the placement region " << Top() << "\n"
  //          << "    Number of rows: " << row_well_status_.size() << "\n"
  //          << "    LX and LY: " << int(block.LLX()) << "  " << int(block.LLY()) << "\n";

  int best_row = 0;
  int best_loc = INT_MIN;
  int min_cost = INT_MAX;
  int p_height = block.TypePtr()->WellPtr()->PNBoundary();

  int tmp_cost = INT_MAX;
  int tmp_end_row = 0;
  int pn_boundary_row = 0;
  int tmp_loc = INT_MIN;
  for (int tmp_row = start_row; tmp_row <= end_row; ++tmp_row) {
    // 1. find the non-overlap location
    tmp_end_row = tmp_row + height - 1;
    tmp_loc = RegionLeft();
    for (int i = tmp_row; i <= tmp_end_row; ++i) {
      tmp_loc = std::max(tmp_loc, block_contour_[i]);
    }

    // 2. legalize the location to respect well rules

    //    2.a check if the distance rule is satisfied
    pn_boundary_row = tmp_row + p_height;
    //    P well distance
    for (int i = tmp_row; i <= pn_boundary_row; ++i) {
      if (block_contour_[i] == RegionLeft()) continue;
      if (!row_well_status_[i].is_n) {
        if (tmp_loc > block_contour_[i] && tmp_loc < block_contour_[i] + pp_spacing_) {
          tmp_loc = block_contour_[i] + pp_spacing_;
        }
      } else {
        if (tmp_loc > block_contour_[i] && tmp_loc < block_contour_[i] + np_spacing_) {
          tmp_loc = block_contour_[i] + np_spacing_;
        }
      }
    }

    //    N well distance
    for (int i = pn_boundary_row + 1; i <= tmp_end_row; ++i) {
      if (block_contour_[i] == RegionLeft()) continue;
      if (row_well_status_[i].is_n) {
        if (tmp_loc > block_contour_[i] && tmp_loc < block_contour_[i] + nn_spacing_) {
          tmp_loc = block_contour_[i] + nn_spacing_;
        }
      } else {
        if (tmp_loc > block_contour_[i] && tmp_loc < block_contour_[i] + np_spacing_) {
          tmp_loc = block_contour_[i] + np_spacing_;
        }
      }
    }

    //    2.b check if the min-width rule is satisfied.
    //    P well min-width
    int shared_length = 0;
    for (int i = tmp_row; i <= pn_boundary_row; ++i) {
      if (block_contour_[i] == RegionLeft()) continue;
      if (!(row_well_status_[i].is_n) && block_contour_[i] == tmp_loc) {
        ++shared_length;
      } else {
        if (shared_length < p_min_width_) {
          tmp_loc += pp_spacing_;
          shared_length = 0;
        }
      }
    }

    shared_length = 0;
    //    N well min-width
    for (int i = pn_boundary_row + 1; i <= tmp_end_row; ++i) {
      if (block_contour_[i] == RegionLeft()) continue;
      if (row_well_status_[i].is_n && block_contour_[i] == tmp_loc) {
        ++shared_length;
      } else {
        if (shared_length < n_min_width_) {
          tmp_loc += nn_spacing_;
          shared_length = 0;
        }
      }
    }

    tmp_cost = std::abs(tmp_loc - init_x) + std::abs(tmp_row + RegionBottom() - init_y);
    bool is_abutted = false;
    for (int i = tmp_row; i <= tmp_end_row; ++i) {
      if (block_contour_[i] == tmp_loc) {
        is_abutted = true;
        break;
      }
    }

    if (is_abutted) {
      tmp_cost -= abutment_benefit_;
    }
    if (tmp_cost < min_cost) {
      best_loc = tmp_loc;
      best_row = tmp_row;
      min_cost = tmp_cost;
    }

  }

  res.x = best_loc;
  res.y = best_row + RegionBottom();

  //std::cout << res.x << "  " << res.y << "  " << min_cost << "  " << block.Num() << "\n";

  return (min_cost < INT_MAX);
}

void WellLegalizer::WellPlace(Block &block) {
  /****
   * 1. if there is no blocks on the left hand side of this block, switch the type to plugged
   * 2. if the current location is legal, use that space
   * 3.   else find a new location
   * ****/
  /*int start_row = int(block.LLY()-Bottom());
  int end_row = start_row + block.Height();
  bool no_left_blocks = true;
  for (int i=start_row; i<=end_row; ++i) {
    if (row_well_status_[i].dist < n_max_plug_dist_) {
      no_left_blocks = false;
      break;
    }
  }
  if (no_left_blocks) {
    SwitchToPlugType(block);
  }*/
  //bool is_cur_loc_legal = IsSpaceLegal(block);
  bool is_cur_loc_legal = false;
  int2d res(block.LLX(), block.LLY());
  if (!is_cur_loc_legal) {
    bool loc_found = FindLocation(block, res);
    if (loc_found) {
      block.SetLoc(res.x, res.y);
    } else {
      Assert(false, "Cannot find legal location");
    }
  }
  UseSpaceLeft(block);
  //UpdatePNBoundary(block);
}

bool WellLegalizer::IsCurLocWellDistanceLeft(int loc_x, int lo_row, int hi_row, int p_row) {
  /****
   * Returns if the N/P well distance to its left hand side neighbors are satisfied
   ****/
  int last_p_row = lo_row + p_row - 1;
  assert(last_p_row >= lo_row);
  assert(hi_row >= last_p_row);

  bool is_well_distance_legal = true;
  bool tmp_row_well_clean;

  // for the P well of this block, check if the distance is satisfied
  for (int i = lo_row; i <= last_p_row; ++i) {
    if (block_contour_[i] == left_) continue;
    if (!row_well_status_[i].is_n) {
      tmp_row_well_clean = (loc_x == block_contour_[i] || loc_x >= block_contour_[i] + pp_spacing_);
    } else {
      tmp_row_well_clean = (loc_x == block_contour_[i] || loc_x >= block_contour_[i] + np_spacing_);
    }
    if (!tmp_row_well_clean) {
      is_well_distance_legal = false;
      break;
    }
  }

  // if the P well distance rule is satisfied, then check if the N well distance rule is satisfied
  if (is_well_distance_legal) {
    for (int i = last_p_row + 1; i <= hi_row; ++i) {
      if (block_contour_[i] == left_) continue;
      if (row_well_status_[i].is_n) {
        tmp_row_well_clean = (loc_x == block_contour_[i] || loc_x >= block_contour_[i] + nn_spacing_);
      } else {
        tmp_row_well_clean = (loc_x == block_contour_[i] || loc_x >= block_contour_[i] + np_spacing_);
      }
      if (!tmp_row_well_clean) {
        is_well_distance_legal = false;
        break;
      }
    }
  }

  // if the in range distance rule is satisfied,
  // then check if the lower left and upper left well respect distance rule
  if (is_well_distance_legal) {
    // P well
    if (row_well_status_[lo_row].IsNWell()) {
      for (int i = 1; i <= pp_spacing_; ++i) {
        int lo_row_minus_i = lo_row - i;
        if (lo_row_minus_i >= 0 && row_well_status_[lo_row_minus_i].IsPWell()) {
          return false;
        }
      }
    }
    if (row_well_status_[last_p_row].IsNWell()) {
      for (int i = 1; i <= pp_spacing_; ++i) {
        int last_p_add_i = last_p_row + i;
        if (last_p_add_i < tot_num_rows_ && row_well_status_[last_p_add_i].IsPWell()) {
          return false;
        }
      }
    }

    // N well
    int first_n_row = last_p_row + 1;
    if (row_well_status_[first_n_row].IsPWell()) {
      for (int i = 1; i <= nn_spacing_; ++i) {
        int first_n_row_minus_i = first_n_row - i;
        if (first_n_row_minus_i >= 0 && row_well_status_[first_n_row_minus_i].IsNWell()) {
          return false;
        }
      }
    }

    if (row_well_status_[hi_row].IsPWell()) {
      for (int i = 1; i <= nn_spacing_; ++i) {
        int hi_row_add_i = hi_row + i;
        if (hi_row_add_i < tot_num_rows_ && row_well_status_[hi_row_add_i].IsNWell()) {
          return false;
        }
      }
    }

  }

  return is_well_distance_legal;
}

bool WellLegalizer::IsCurLocWellMinWidthLeft(int loc_x, int lo_row, int hi_row, int p_row) {
  // check if the min-width rule is satisfied.
  //    P well min-width
  int shared_well_height = 0;
  int boundary_row = lo_row + p_row - 1;
  bool is_well_min_width_legal = true;
  for (int i = lo_row; i <= boundary_row; ++i) {
    if (block_contour_[i] == left_) continue;
    if (!(row_well_status_[i].is_n) && block_contour_[i] == loc_x) {
      ++shared_well_height;
    } else {
      if (shared_well_height < p_min_width_) {
        is_well_min_width_legal = false;
      }
    }
  }

  //    N well min-width
  if (is_well_min_width_legal) {
    shared_well_height = 0;
    is_well_min_width_legal = true;
    for (int i = boundary_row + 1; i <= hi_row; ++i) {
      if (block_contour_[i] == left_) continue;
      if (row_well_status_[i].is_n && block_contour_[i] == loc_x) {
        ++shared_well_height;
      } else {
        if (shared_well_height < n_min_width_) {
          is_well_min_width_legal = false;
        }
      }
    }
  }

  return is_well_min_width_legal;
}

bool WellLegalizer::IsBlockPerfectMatchLeft(int loc_x, int lo_row, int hi_row, int p_row) {

  return true;
}

bool WellLegalizer::IsCurrentLocLegalLeft(Value2D<int> &loc, int width, int height, int p_row) {
  int lo_row = StartRow(loc.y);
  int hi_row = EndRow(loc.y + height);
  return IsCurrentLocLegalLeft(loc.x, width, lo_row, hi_row, p_row);
}

bool WellLegalizer::IsCurrentLocLegalLeft(int loc_x, int width, int lo_row, int hi_row, int p_row) {
  /****
   * Returns whether the current location is legal
   *
   * 1. if the space itself is illegal, then return false
   * 2. if the space covers placed blocks, then return false
   * 3. if the N/P well distance to its left hand side neighbors are not satisfied, then return false
   * 4. if the N/P well minimum spacing rule is not satisfied, then return false
   * 3. otherwise, return true
   * ****/

  bool is_current_loc_legal;

  // 1. check if the space itself is legal
  is_current_loc_legal = IsSpaceLegal(loc_x, loc_x + width, lo_row, hi_row);
  if (!is_current_loc_legal) {
    //std::cout << "Space illegal\n";
    return false;
  }

  // 2. check if the space covers any placed blocks
  is_current_loc_legal = true;
  for (int i = lo_row; i <= hi_row; ++i) {
    if (block_contour_[i] > loc_x) {
      is_current_loc_legal = false;
      break;
    }
  }
  if (!is_current_loc_legal) {
    //std::cout << "Overlap illegal\n";
    return false;
  }

  // 3. check if the N/P well distance to its left hand side neighbors are satisfied
  bool is_well_distance_legal = IsCurLocWellDistanceLeft(loc_x, lo_row, hi_row, p_row);
  if (!is_well_distance_legal) {
    //std::cout << "Well distance illegal\n";
    return false;
  }

  // 4. check the N/P well minimum width rule
  bool is_well_min_width_legal = IsCurLocWellMinWidthLeft(loc_x, lo_row, hi_row, p_row);
  if (!is_well_min_width_legal) {
    //std::cout << "Min width illegal\n";
    return false;
  }

  return true;
}

bool WellLegalizer::FindLocLeft(Value2D<int> &loc, int num, int width, int height, int p_row) {
  /****
   * Returns whether a legal location can be found, and put the final location to @params loc
   *
   * 1. first, try to search locations around the current location, find the location with the minimum cost function
   *    1). make sure there is no cell overlap
   *    2). if there is well distance issues, add a penalty term to the cost function
   *    3). if there is well width issues, add a penalty term to the cost function
   *    4). if the block touches its top block, add a benefit term
   *    the formula of the cost function:
   *        C = |tmp_x - cur_x| + |tmp_y - cur_y|
   *          + k_distance * cur_iteration_num * I(disrespect distance rule)
   *          + k_min_width * cur_iteration_num * I(disrespect min-width rule)
   *          - 1 * I(touch top/bottom blocks) (this factor is not taking into account yet)
   * 2. if the best location using the above procedure found is illegal
   *    (blocks may be placed on the top of blocks or out of the placement range)
   *    enlarge the search range, and try to find a legal location
   * ****/
  bool is_successful;

  int blk_row_height;
  int left_block_bound;
  int left_white_space_bound;

  int max_search_row;
  int search_start_row;
  int search_end_row;

  int best_row;
  int best_loc_x;
  double min_cost;

  double tmp_cost;
  int tmp_end_row;
  int tmp_x;
  int tmp_y;

  left_block_bound = (int) std::round(loc.x - k_left_ * width);
  //left_block_bound = loc.x;

  max_search_row = MaxRow(height);
  blk_row_height = HeightToRow(height);

  search_start_row = std::max(0, LocToRow(loc.y - 1 * height));
  search_end_row = std::min(max_search_row, LocToRow(loc.y + 2 * height));

  best_row = 0;
  best_loc_x = INT_MIN;
  min_cost = DBL_MAX;

  for (int tmp_start_row = search_start_row; tmp_start_row <= search_end_row; ++tmp_start_row) {
    tmp_end_row = tmp_start_row + blk_row_height - 1;
    left_white_space_bound = WhiteSpaceBoundLeft(loc.x, loc.x + width, tmp_start_row, tmp_end_row);

    tmp_x = std::max(left_white_space_bound, left_block_bound);

    // make sure no overlap
    for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
      tmp_x = std::max(tmp_x, block_contour_[n]);
    }

    tmp_y = RowToLoc(tmp_start_row);
    tmp_cost = std::abs(tmp_x - init_loc_[num].x) + std::abs(tmp_y - init_loc_[num].y);

    bool is_well_distance_clean = IsCurLocWellDistanceLeft(tmp_x, tmp_start_row, tmp_end_row, p_row);
    if (!is_well_distance_clean) {
      tmp_cost += k_distance_ * cur_iter_;
    }

    bool is_well_minwidth_clean = IsCurLocWellMinWidthLeft(tmp_x, tmp_start_row, tmp_end_row, p_row);
    if (!is_well_minwidth_clean) {
      tmp_cost += k_min_width_ * cur_iter_;
    }

    if (tmp_cost < min_cost) {
      best_loc_x = tmp_x;
      best_row = tmp_start_row;
      min_cost = tmp_cost;
    }

    /*if (num == 614) {
      std::cout << "x: " << tmp_x << "\n"
                << "y: " << RowToLoc(tmp_start_row) << "\n"
                << "cost: " << tmp_cost << "\n";
    }*/

  }
  is_successful = IsCurrentLocLegalLeft(best_loc_x,
                                        width,
                                        best_row,
                                        best_row + blk_row_height - 1,
                                        p_row);

  loc.x = best_loc_x;
  loc.y = RowToLoc(best_row);

  return is_successful;
}

bool WellLegalizer::WellLegalizationLeft() {
  int fail_count = 0;
  bool is_successful = true;
  block_contour_.assign(block_contour_.size(), left_);
  std::vector<Block> &block_list = *BlockList();

  int sz = index_loc_list_.size();
  for (int i = 0; i < sz; ++i) {
    index_loc_list_[i].num = i;
    index_loc_list_[i].x = block_list[i].LLX();
    index_loc_list_[i].y = block_list[i].LLY();
  }
  std::sort(index_loc_list_.begin(), index_loc_list_.end());

  int height;
  int width;
  int p_well_row_height;

  Value2D<int> res;
  bool is_current_loc_legal;
  bool is_legal_loc_found;

  for (auto &pair: index_loc_list_) {
    auto &block = block_list[pair.num];
    if (block.IsFixed()) continue;

    res.x = int(std::round(block.LLX()));
    res.y = AlignLocToRowLoc(block.LLY());
    height = int(block.Height());
    width = int(block.Width());

    p_well_row_height = HeightToRow(block.TypePtr()->WellPtr()->PNBoundary());

    is_current_loc_legal = IsCurrentLocLegalLeft(res, width, height, p_well_row_height);

    if (!is_current_loc_legal) {
      is_legal_loc_found = FindLocLeft(res, pair.num, width, height, p_well_row_height);
      if (!is_legal_loc_found) {
        //printf("LNum: %d, lx: %d, ly: %d\n", pair.num, res.x, res.y);
        ++fail_count;
        is_successful = false;
      }
    }
    //printf("LNum final: %d, lx: %d, ly: %d\n", pair.num, res.x, res.y);

    block.SetLoc(res.x, res.y);
    MarkSpaceWellLeft(block, p_well_row_height);
  }

  std::cout << "fail count: " << fail_count << "\n";

  return is_successful;
}

void WellLegalizer::MarkSpaceWellRight(Block const &block, int p_row) {
  /****
   * Mark the space used by this block by changing the start point of available space in each related row
   * ****/
  int lo_row = StartRow((int) std::round(block.LLY()));
  int last_p_row = lo_row + p_row - 1;
  int hi_row = EndRow((int) std::round(block.URY()));

  /*if (end_row >= int(row_well_status_.size())) {
    //std::cout << "  ly:     " << int(block.LLY())       << "\n"
    //          << "  height: " << block.Height()   << "\n"
    //          << "  top:    " << Top()    << "\n"
    //          << "  bottom: " << Bottom() << "\n";
    Assert(false, "Cannot use space out of range");
  }*/

  assert(block.URY() <= RegionTop());
  assert(hi_row < tot_num_rows_);
  assert(lo_row >= 0);

  int end_x = int(block.LLX());

  for (int i = lo_row; i <= hi_row; ++i) {
    block_contour_[i] = end_x;
    row_well_status_[i].is_n = (i > last_p_row);
  }
}

bool WellLegalizer::IsCurLocWellDistanceRight(int loc_x, int lo_row, int hi_row, int p_row) {
  int last_p_row = lo_row + p_row - 1;
  assert(last_p_row >= lo_row);
  assert(hi_row >= last_p_row);

  bool is_well_distance_legal = true;
  bool tmp_row_well_clean;
  for (int i = lo_row; i <= last_p_row; ++i) {
    if (block_contour_[i] == right_) continue;
    if (!row_well_status_[i].is_n) {
      tmp_row_well_clean = (loc_x == block_contour_[i] || loc_x <= block_contour_[i] - pp_spacing_);
    } else {
      tmp_row_well_clean = (loc_x == block_contour_[i] || loc_x <= block_contour_[i] - np_spacing_);
    }
    if (!tmp_row_well_clean) {
      is_well_distance_legal = false;
      break;
    }
  }

  if (is_well_distance_legal) {
    for (int i = last_p_row + 1; i <= hi_row; ++i) {
      if (block_contour_[i] == right_) continue;
      if (row_well_status_[i].is_n) {
        tmp_row_well_clean = (loc_x == block_contour_[i] || loc_x <= block_contour_[i] - nn_spacing_);
      } else {
        tmp_row_well_clean = (loc_x == block_contour_[i] || loc_x <= block_contour_[i] - np_spacing_);
      }
      if (!tmp_row_well_clean) {
        is_well_distance_legal = false;
        break;
      }
    }
  }

  // if the in range distance rule is satisfied,
  // then check if the lower left and upper left well respect distance rule
  if (is_well_distance_legal) {
    // P well
    if (row_well_status_[lo_row].IsNWell()) {
      for (int i = 1; i <= pp_spacing_; ++i) {
        int lo_row_minus_i = lo_row - i;
        if (lo_row_minus_i >= 0 && row_well_status_[lo_row_minus_i].IsPWell()) {
          return false;
        }
      }
    }
    if (row_well_status_[last_p_row].IsNWell()) {
      for (int i = 1; i <= pp_spacing_; ++i) {
        int last_p_add_i = last_p_row + i;
        if (last_p_add_i < tot_num_rows_ && row_well_status_[last_p_add_i].IsPWell()) {
          return false;
        }
      }
    }

    // N well
    int first_n_row = last_p_row + 1;
    if (row_well_status_[first_n_row].IsPWell()) {
      for (int i = 1; i <= nn_spacing_; ++i) {
        int first_n_row_minus_i = first_n_row - i;
        if (first_n_row_minus_i >= 0 && row_well_status_[first_n_row_minus_i].IsNWell()) {
          return false;
        }
      }
    }

    if (row_well_status_[hi_row].IsPWell()) {
      for (int i = 1; i <= nn_spacing_; ++i) {
        int hi_row_add_i = hi_row + i;
        if (hi_row_add_i < tot_num_rows_ && row_well_status_[hi_row_add_i].IsNWell()) {
          return false;
        }
      }
    }

  }

  return is_well_distance_legal;
}

bool WellLegalizer::IsCurLocWellMinWidthRight(int loc_x, int lo_row, int hi_row, int p_row) {
  int shared_well_height = 0;
  int boundary_row = lo_row + p_row - 1;
  bool is_well_min_width_legal = true;
  for (int i = lo_row; i <= boundary_row; ++i) {
    if (block_contour_[i] == right_) continue;
    if (!(row_well_status_[i].is_n) && block_contour_[i] == loc_x) {
      ++shared_well_height;
    } else {
      if (shared_well_height < p_min_width_) {
        is_well_min_width_legal = false;
      }
    }
  }

  //    N well min-width
  if (is_well_min_width_legal) {
    shared_well_height = 0;
    is_well_min_width_legal = true;
    for (int i = boundary_row + 1; i <= hi_row; ++i) {
      if (block_contour_[i] == right_) continue;
      if (row_well_status_[i].is_n && block_contour_[i] == loc_x) {
        ++shared_well_height;
      } else {
        if (shared_well_height < n_min_width_) {
          is_well_min_width_legal = false;
        }
      }
    }
  }

  return is_well_min_width_legal;
}

bool WellLegalizer::IsCurrentLocLegalRight(Value2D<int> &loc, int width, int height, int p_row) {
  int lo_row = StartRow(loc.y);
  int hi_row = EndRow(loc.y + height);
  return IsCurrentLocLegalRight(loc.x, width, lo_row, hi_row, p_row);
}

bool WellLegalizer::IsCurrentLocLegalRight(int loc_x, int width, int lo_row, int hi_row, int p_row) {
/****
   * Returns whether the current location is legal
   *
   * 1. if the space itself is illegal, then return false
   * 2. if the space covers placed blocks, then return false
   * 3. if the N/P well distance to its left hand side neighbors are not satisfied, then return false
   * 4. if the N/P well minimum spacing rule is not satisfied, then return false
   * 3. otherwise, return true
   * ****/

  bool is_current_loc_legal;

  // 1. check if the space itself is legal
  is_current_loc_legal = IsSpaceLegal(loc_x - width, loc_x, lo_row, hi_row);
  if (!is_current_loc_legal) {
    //std::cout << "Space illegal\n";
    return false;
  }

  // 2. check if the space covers any placed blocks
  is_current_loc_legal = true;
  for (int i = lo_row; i <= hi_row; ++i) {
    if (block_contour_[i] < loc_x) {
      is_current_loc_legal = false;
      break;
    }
  }
  if (!is_current_loc_legal) {
    //std::cout << "Overlap illegal\n";
    return false;
  }

  // 3. check if the N/P well distance to its left hand side neighbors are satisfied
  bool is_well_distance_legal = IsCurLocWellDistanceRight(loc_x, lo_row, hi_row, p_row);
  if (!is_well_distance_legal) {
    //std::cout << "Well distance illegal\n";
    return false;
  }


  // 4. check the N/P well minimum width rule
  bool is_well_min_width_legal = IsCurLocWellMinWidthRight(loc_x, lo_row, hi_row, p_row);
  if (!is_well_min_width_legal) {
    //std::cout << "Min width illegal\n";
    return false;
  }

  return true;
}

bool WellLegalizer::FindLocRight(Value2D<int> &loc, int num, int width, int height, int p_row) {
  bool is_successful;

  int blk_row_height;
  int right_block_bound;
  int right_white_space_bound;

  int max_search_row;
  int search_start_row;
  int search_end_row;

  int best_row;
  int best_loc_x;
  double min_cost;

  double tmp_cost;
  int tmp_end_row;
  int tmp_x;
  int tmp_y;

  right_block_bound = (int) std::round(loc.x + k_left_ * width);
  //right_block_bound = loc.x;

  max_search_row = MaxRow(height);
  blk_row_height = HeightToRow(height);

  search_start_row = std::max(0, LocToRow(loc.y - 1 * height));
  search_end_row = std::min(max_search_row, LocToRow(loc.y + 2 * height));

  best_row = 0;
  best_loc_x = INT_MAX;
  min_cost = DBL_MAX;

  for (int tmp_start_row = search_start_row; tmp_start_row <= search_end_row; ++tmp_start_row) {
    tmp_end_row = tmp_start_row + blk_row_height - 1;
    right_white_space_bound = WhiteSpaceBoundRight(loc.x - width, loc.x, tmp_start_row, tmp_end_row);

    tmp_x = std::min(right_white_space_bound, right_block_bound);
    //tmp_x = std::min(right_, right_block_bound);

    for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
      tmp_x = std::min(tmp_x, block_contour_[n]);
    }

    tmp_y = RowToLoc(tmp_start_row);
    tmp_cost = std::abs(tmp_x - init_loc_[num].x - width) + std::abs(tmp_y - init_loc_[num].y);

    bool is_well_distance_clean = IsCurLocWellDistanceRight(tmp_x, tmp_start_row, tmp_end_row, p_row);
    if (!is_well_distance_clean) {
      tmp_cost += k_distance_ * cur_iter_;
    }

    bool is_well_minwidth_clean = IsCurLocWellMinWidthRight(tmp_x, tmp_start_row, tmp_end_row, p_row);
    if (!is_well_minwidth_clean) {
      tmp_cost += k_min_width_ * cur_iter_;
    }

    if (tmp_cost < min_cost) {
      best_loc_x = tmp_x;
      best_row = tmp_start_row;
      min_cost = tmp_cost;
    }
  }

  // if still cannot find a legal location, enter fail mode
  is_successful = IsCurrentLocLegalRight(best_loc_x,
                                         width,
                                         best_row,
                                         best_row + blk_row_height - 1,
                                         p_row);

  loc.x = best_loc_x;
  loc.y = RowToLoc(best_row);;

  return is_successful;
}

bool WellLegalizer::WellLegalizationRight() {
  int fail_count = 0;
  bool is_successful = true;
  block_contour_.assign(block_contour_.size(), right_);
  std::vector<Block> &block_list = *BlockList();

  int sz = index_loc_list_.size();
  for (int i = 0; i < sz; ++i) {
    index_loc_list_[i].num = i;
    index_loc_list_[i].x = block_list[i].URX();
    index_loc_list_[i].y = block_list[i].LLY();
  }
  std::sort(index_loc_list_.begin(),
            index_loc_list_.end(),
            [](const IndexLocPair<int> &lhs, const IndexLocPair<int> &rhs) {
              return (lhs.x > rhs.x) || (lhs.x == rhs.x && lhs.y > rhs.y);
            });

  int height;
  int width;
  int p_well_row_height;

  Value2D<int> res;
  bool is_current_loc_legal;
  bool is_legal_loc_found;

  for (auto &pair: index_loc_list_) {
    auto &block = block_list[pair.num];
    if (block.IsFixed()) continue;

    res.x = int(std::round(block.URX()));
    res.y = AlignLocToRowLoc(block.LLY());
    height = int(block.Height());
    width = int(block.Width());

    p_well_row_height = HeightToRow(block.TypePtr()->WellPtr()->PNBoundary());

    //std::cout << block.Num() << "\n";
    is_current_loc_legal = IsCurrentLocLegalRight(res, width, height, p_well_row_height);

    if (!is_current_loc_legal) {
      //printf("Num: %d, lx: %d, ly: %d\n", pair.num, res.x, res.y);
      is_legal_loc_found = FindLocRight(res, pair.num, width, height, p_well_row_height);
      if (!is_legal_loc_found) {
        //printf("RNum: %d, lx: %d, ly: %d\n", pair.num, res.x, res.y);
        ++fail_count;
        is_successful = false;
      }
    }

    block.setURX(res.x);
    block.setLLY(res.y);

    MarkSpaceWellRight(block, p_well_row_height);
  }

  std::cout << "fail count: " << fail_count << "\n";

  return is_successful;
}

bool WellLegalizer::StartPlacement() {
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "---------------------------------------\n"
              << "Start Well Legalization\n";
  }

  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  InitWellLegalizer();
  std::cout << "  Number of rows: " << row_well_status_.size() << "\n"
            << "  Number of blocks: " << index_loc_list_.size() << "\n"
            << "  Well Rules:\n"
            << "    NN spacing: " << nn_spacing_ << "\n"
            << "    PP spacing: " << pp_spacing_ << "\n"
            << "    NP spacing: " << np_spacing_ << "\n"
            << "    MaxNDist:   " << n_max_plug_dist_ << "\n"
            << "    MaxPDist:   " << p_max_plug_dist_ << "\n"
            << "    MinNWidth:  " << n_min_width_ << "\n"
            << "    MinPWidth:  " << p_min_width_ << "\n";

  bool is_success = false;
  for (cur_iter_ = 0; cur_iter_ < max_iter_; ++cur_iter_) {
    std::cout << "Current iteration: " << cur_iter_ << "\n";
    //well_mis_align_cost_factor_ = cur_iter_ + 1;
    if (legalize_from_left_) {
      is_success = WellLegalizationLeft();
    } else {
      is_success = WellLegalizationRight();
    }
    //std::cout << "Current iteration: " << is_success << "  " << legalize_from_left_ << "\n";
    legalize_from_left_ = !legalize_from_left_;
    ++k_left_;
    GenMATLABWellTable("lg" + std::to_string(cur_iter_) + "_result", 0);
    ReportHPWL(LOG_CRITICAL);
    if (is_success) {
      break;
    }
  }

  if (!is_success) {
    std::cout << "Well Legalization fails\n";
  }

  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "\033[0;36m"
              << "Well Legalization complete!\n"
              << "\033[0m";
  }

  ReportHPWL(LOG_CRITICAL);

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "(wall time: "
              << wall_time << "s, cpu time: "
              << cpu_time << "s)\n";
  }

  ReportMemory(LOG_CRITICAL);

  return true;
}
