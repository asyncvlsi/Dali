//
// Created by Yihang Yang on 12/22/19.
//

#include "welllegalizer.h"

#include <algorithm>

WellLegalizer::WellLegalizer() : LGTetrisEx() {
  max_iter_ = 3;
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
  nn_spacing = std::ceil(n_layer->Spacing() / circuit_->GetGridValueX());
  pp_spacing = std::ceil(p_layer->Spacing() / circuit_->GetGridValueX());
  np_spacing = std::ceil(p_layer->OpSpacing() / circuit_->GetGridValueX());
  n_min_width = std::ceil(n_layer->Width() / circuit_->GetGridValueX());
  p_min_width = std::ceil(p_layer->Width() / circuit_->GetGridValueX());

  RowWellStatus tmp_row_well_status(INT_MAX, false);
  row_well_status_.resize(RegionTop() - RegionBottom() + 1, tmp_row_well_status);

  InitLegalizer();
}

void WellLegalizer::SwitchToPlugType(Block &block) {
  auto well = block.Type()->GetWell();
  Assert(well != nullptr, "Block does not have a well, cannot switch to the plugged version: " + *block.Name());
  if (well->IsUnplug()) {
    auto type = block.Type();
    auto cluster = well->GetCluster();
    auto plugged_well = cluster->GetPlug();
    Assert(cluster != nullptr || plugged_well == nullptr,
           "There is not plugged version of this BlockType: " + *type->Name());
    block.SetType(plugged_well->Type());
  }
}

void WellLegalizer::UseSpaceLeft(Block const &block) {
  /****
   * Mark the space used by this block by changing the start point of available space in each related row
   * ****/
  int start_row = StartRow((int) std::round(block.LLY()));
  int end_row = EndRow((int) std::round(block.URY()));

  /*if (end_row >= int(row_well_status_.size())) {
    //std::cout << "  ly:     " << int(block.LLY())       << "\n"
    //          << "  height: " << block.Height()   << "\n"
    //          << "  top:    " << Top()    << "\n"
    //          << "  bottom: " << Bottom() << "\n";
    Assert(false, "Cannot use space out of range");
  }*/

  assert(end_row < int(block_contour_.size()));
  assert(start_row >= 0);

  int end_x = int(block.URX());
  int pn_boundary_row = start_row + block.Type()->well_->GetPNBoundary() - 1;

  for (int i = start_row; i <= end_row; ++i) {
    block_contour_[i] = end_x;
    row_well_status_[i].is_n = (i > pn_boundary_row);
  }
}

void WellLegalizer::UpdatePNBoundary(Block const &block) {
  /****
   * 1. If the newly placed gate block the alignment line, remove those lines.
   * 2. Adding a new line, which is the P/N boundary of this block.
   * ****/
  auto it_lower = p_n_boundary.lower_bound(block.LLY());
  auto it_upper = p_n_boundary.upper_bound(block.URY());
  p_n_boundary.erase(it_lower, it_upper);
  p_n_boundary.insert(block.LLY() + block.Type()->well_->GetPNBoundary());
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
  int p_height = block.Type()->well_->GetPNBoundary();

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
        if (tmp_loc > block_contour_[i] && tmp_loc < block_contour_[i] + pp_spacing) {
          tmp_loc = block_contour_[i] + pp_spacing;
        }
      } else {
        if (tmp_loc > block_contour_[i] && tmp_loc < block_contour_[i] + np_spacing) {
          tmp_loc = block_contour_[i] + np_spacing;
        }
      }
    }

    //    N well distance
    for (int i = pn_boundary_row + 1; i <= tmp_end_row; ++i) {
      if (block_contour_[i] == RegionLeft()) continue;
      if (row_well_status_[i].is_n) {
        if (tmp_loc > block_contour_[i] && tmp_loc < block_contour_[i] + nn_spacing) {
          tmp_loc = block_contour_[i] + nn_spacing;
        }
      } else {
        if (tmp_loc > block_contour_[i] && tmp_loc < block_contour_[i] + np_spacing) {
          tmp_loc = block_contour_[i] + np_spacing;
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
        if (shared_length < p_min_width) {
          tmp_loc += pp_spacing;
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
        if (shared_length < n_min_width) {
          tmp_loc += nn_spacing;
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
      tmp_cost -= abutment_benefit;
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
    //std::cout << "Space Illegal\n";
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
    //std::cout << "Cover placed blocks\n";
    return false;
  }

  // 3. check if the N/P well distance to its left hand side neighbors are satisfied
  int boundary_row = lo_row + p_row - 1;
  assert(boundary_row >= lo_row);
  assert(hi_row >= boundary_row);

  bool is_well_rule_clean = true;
  bool tmp_row_well_clean;

  // for the P well of this block, check if the distance is satisfied
  for (int i = lo_row; i <= boundary_row; ++i) {
    if (block_contour_[i] == left_) continue;
    if (!row_well_status_[i].is_n) {
      tmp_row_well_clean = (loc_x == block_contour_[i] || loc_x >= block_contour_[i] + pp_spacing);
    } else {
      tmp_row_well_clean = (loc_x == block_contour_[i] || loc_x >= block_contour_[i] + np_spacing);
    }
    if (!tmp_row_well_clean) {
      is_well_rule_clean = false;
      break;
    }
  }

  // if the P well distance rule is satisfied, then check if the N well distance rule is satisfied
  if (is_well_rule_clean) {
    for (int i = boundary_row + 1; i <= hi_row; ++i) {
      if (block_contour_[i] == left_) continue;
      if (row_well_status_[i].is_n) {
        tmp_row_well_clean = (loc_x == block_contour_[i] || loc_x >= block_contour_[i] + nn_spacing);
      } else {
        tmp_row_well_clean = (loc_x == block_contour_[i] || loc_x >= block_contour_[i] + np_spacing);
      }
      if (!tmp_row_well_clean) {
        is_well_rule_clean = false;
        break;
      }
    }
  }
  if (!is_well_rule_clean) {
    //std::cout << "N/P well distance unsatisfied\n";
    return false;
  }

  // 4. check if the min-width rule is satisfied.
  //    P well min-width
  int shared_length = 0;
  is_well_rule_clean = true;
  for (int i = lo_row; i <= boundary_row; ++i) {
    if (block_contour_[i] == left_) continue;
    if (!(row_well_status_[i].is_n) && block_contour_[i] == loc_x) {
      ++shared_length;
    } else {
      if (shared_length < p_min_width) {
        is_well_rule_clean = false;
      }
    }
  }

  //    N well min-width
  if (is_well_rule_clean) {
    shared_length = 0;
    is_well_rule_clean = true;
    for (int i = boundary_row + 1; i <= hi_row; ++i) {
      if (block_contour_[i] == left_) continue;
      if (row_well_status_[i].is_n && block_contour_[i] == loc_x) {
        ++shared_length;
      } else {
        if (shared_length < n_min_width) {
          is_well_rule_clean = false;
        }
      }
    }
  }
  if (!is_well_rule_clean) {
    return false;
  }

  return true;
}

bool WellLegalizer::FindLocLeft(Value2D<int> &loc, int width, int height, int p_row) {
  /****
   * Returns whether a legal location can be found, and put the final location to @params loc
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

  bool is_well_aligned;
  bool is_loc_legal;

  left_block_bound = (int) std::round(loc.x - k_left_ * width);
  //left_block_bound = loc.x;

  max_search_row = MaxRow(height);
  blk_row_height = HeightToRow(height);

  search_start_row = std::max(0, LocToRow(loc.y - 4 * height));
  search_end_row = std::min(max_search_row, LocToRow(loc.y + 5 * height));

  best_row = 0;
  best_loc_x = INT_MIN;
  min_cost = DBL_MAX;

  for (int tmp_start_row = search_start_row; tmp_start_row <= search_end_row; ++tmp_start_row) {
    tmp_end_row = tmp_start_row + blk_row_height - 1;
    left_white_space_bound = WhiteSpaceBoundLeft(loc.x, loc.x + width, tmp_start_row, tmp_end_row);
    tmp_x = std::max(left_white_space_bound, left_block_bound);

    for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
      tmp_x = std::max(tmp_x, block_contour_[n]);
    }

    tmp_y = RowToLoc(tmp_start_row);
    //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

    tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);

    /*int first_n_row = tmp_start_row + p_row;
    is_well_aligned = !row_well_status_[first_n_row - 1].is_n && row_well_status_[first_n_row].is_n;
    is_loc_legal = IsCurrentLocLegalLeft(tmp_x, width, tmp_start_row, tmp_end_row, p_row);
    if (!is_loc_legal && !is_well_aligned) {
      tmp_cost += well_mis_align_cost_factor;
    }*/

    if (tmp_cost < min_cost) {
      best_loc_x = tmp_x;
      best_row = tmp_start_row;
      min_cost = tmp_cost;
    }
  }

  int best_row_legal = 0;
  int best_loc_x_legal = INT_MIN;
  double min_cost_legal = DBL_MAX;

  is_loc_legal = IsCurrentLocLegalLeft(best_loc_x, width, best_row, best_row + blk_row_height - 1, p_row);

  if (!is_loc_legal) {
    int old_start_row = search_start_row;
    int old_end_row = search_end_row;
    int extended_range = std::min(cur_iter_, 2) * blk_row_height;
    search_start_row = std::max(0, search_start_row - extended_range);
    search_end_row = std::min(max_search_row, search_end_row + extended_range);
    for (int tmp_start_row = search_start_row; tmp_start_row < old_start_row; ++tmp_start_row) {
      tmp_end_row = tmp_start_row + blk_row_height - 1;
      left_white_space_bound = WhiteSpaceBoundLeft(loc.x, loc.x + width, tmp_start_row, tmp_end_row);
      tmp_x = std::max(left_white_space_bound, left_block_bound);

      for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
        tmp_x = std::max(tmp_x, block_contour_[n]);
      }

      tmp_y = RowToLoc(tmp_start_row);
      //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

      tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);

      int first_n_row = tmp_start_row + p_row;
      is_well_aligned = !row_well_status_[first_n_row - 1].is_n && row_well_status_[first_n_row].is_n;

      if (!is_well_aligned) {
        tmp_cost += well_mis_align_cost_factor;
      }

      if (tmp_cost < min_cost) {
        best_loc_x = tmp_x;
        best_row = tmp_start_row;
        min_cost = tmp_cost;
      }

      is_loc_legal = IsCurrentLocLegalLeft(tmp_x, width, tmp_start_row, tmp_end_row, p_row);

      if (is_loc_legal && is_well_aligned) {
        if (tmp_cost < min_cost_legal) {
          best_loc_x_legal = tmp_x;
          best_row_legal = tmp_start_row;
          min_cost_legal = tmp_cost;
        }
      }
    }
    for (int tmp_start_row = old_end_row; tmp_start_row < search_end_row; ++tmp_start_row) {
      tmp_end_row = tmp_start_row + blk_row_height - 1;
      left_white_space_bound = WhiteSpaceBoundLeft(loc.x, loc.x + width, tmp_start_row, tmp_end_row);
      tmp_x = std::max(left_white_space_bound, left_block_bound);

      for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
        tmp_x = std::max(tmp_x, block_contour_[n]);
      }

      tmp_y = RowToLoc(tmp_start_row);
      //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

      tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);

      int first_n_row = tmp_start_row + p_row;
      is_well_aligned = !row_well_status_[first_n_row - 1].is_n && row_well_status_[first_n_row].is_n;

      if (!is_well_aligned) {
        tmp_cost += well_mis_align_cost_factor;
      }

      if (tmp_cost < min_cost) {
        best_loc_x = tmp_x;
        best_row = tmp_start_row;
        min_cost = tmp_cost;
      }

      is_loc_legal = IsCurrentLocLegalLeft(tmp_x, width, tmp_start_row, tmp_end_row, p_row);

      if (is_loc_legal && is_well_aligned) {
        if (tmp_cost < min_cost_legal) {
          best_loc_x_legal = tmp_x;
          best_row_legal = tmp_start_row;
          min_cost_legal = tmp_cost;
        }
      }
    }

  }

  // if still cannot find a legal location, enter fail mode
  is_successful = IsCurrentLocLegalLeft(best_loc_x,
                                        width,
                                        best_row,
                                        best_row + blk_row_height - 1,
                                        p_row);
  if (!is_successful) {
    if (best_loc_x_legal >= left_ && best_loc_x_legal <= right_ - width) {
      is_successful = IsCurrentLocLegalLeft(best_loc_x_legal,
                                            width,
                                            best_row_legal,
                                            best_row_legal + blk_row_height - 1,
                                            p_row);
    }
    if (is_successful) {
      best_loc_x = best_loc_x_legal;
      best_row = best_row_legal;
    }
  }

  loc.x = best_loc_x;
  loc.y = RowToLoc(best_row);

  return is_successful;
}

bool WellLegalizer::WellLegalizationLeft() {
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

  for (auto &&pair: index_loc_list_) {
    auto &block = block_list[pair.num];
    if (block.IsFixed()) continue;

    res.x = int(std::round(block.LLX()));
    res.y = AlignedLocToRowLoc(block.LLY());
    height = int(block.Height());
    width = int(block.Width());

    p_well_row_height = HeightToRow(block.Type()->well_->GetPNBoundary());

    //std::cout << block.Num() << "\n";
    is_current_loc_legal = IsCurrentLocLegalLeft(res, width, height, p_well_row_height);

    if (!is_current_loc_legal) {
      is_legal_loc_found = FindLocLeft(res, width, height, p_well_row_height);
      if (!is_legal_loc_found) {
        is_successful = false;
      }
    }

    block.SetLoc(res.x, res.y);
    UseSpaceLeft(block);
  }

  return is_successful;
}

void WellLegalizer::UseSpaceRight(Block const &block) {
  /****
   * Mark the space used by this block by changing the start point of available space in each related row
   * ****/
  int start_row = StartRow((int) std::round(block.LLY()));
  int end_row = EndRow((int) std::round(block.URY()));

  /*if (end_row >= int(row_well_status_.size())) {
    //std::cout << "  ly:     " << int(block.LLY())       << "\n"
    //          << "  height: " << block.Height()   << "\n"
    //          << "  top:    " << Top()    << "\n"
    //          << "  bottom: " << Bottom() << "\n";
    Assert(false, "Cannot use space out of range");
  }*/

  assert(block.URY() <= RegionTop());
  assert(end_row < tot_num_rows_);
  assert(start_row >= 0);

  int end_x = int(block.LLX());
  int pn_boundary_row = start_row + block.Type()->well_->GetPNBoundary() - 1;

  for (int i = start_row; i <= end_row; ++i) {
    block_contour_[i] = end_x;
    row_well_status_[i].is_n = (i > pn_boundary_row);
  }
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
    return false;
  }

  // 3. check if the N/P well distance to its left hand side neighbors are satisfied
  int boundary_row = lo_row + p_row - 1;
  assert(boundary_row >= lo_row);
  assert(hi_row >= boundary_row);

  bool is_well_rule_clean = true;
  bool tmp_row_well_clean;
  for (int i = lo_row; i <= boundary_row; ++i) {
    if (block_contour_[i] == right_) continue;
    if (!row_well_status_[i].is_n) {
      tmp_row_well_clean = (loc_x == block_contour_[i] || loc_x <= block_contour_[i] - pp_spacing);
    } else {
      tmp_row_well_clean = (loc_x == block_contour_[i] || loc_x <= block_contour_[i] - np_spacing);
    }
    if (!tmp_row_well_clean) {
      is_well_rule_clean = false;
      break;
    }
  }

  if (is_well_rule_clean) {
    for (int i = boundary_row + 1; i <= hi_row; ++i) {
      if (block_contour_[i] == right_) continue;
      if (row_well_status_[i].is_n) {
        tmp_row_well_clean = (loc_x == block_contour_[i] || loc_x <= block_contour_[i] - nn_spacing);
      } else {
        tmp_row_well_clean = (loc_x == block_contour_[i] || loc_x <= block_contour_[i] - np_spacing);
      }
      if (!tmp_row_well_clean) {
        is_well_rule_clean = false;
        break;
      }
    }
  }
  if (!is_well_rule_clean) {
    return false;
  }

  // 4. check if the min-width rule is satisfied.
  //    P well min-width
  int shared_length = 0;
  is_well_rule_clean = true;
  for (int i = lo_row; i <= boundary_row; ++i) {
    if (block_contour_[i] == right_) continue;
    if (!(row_well_status_[i].is_n) && block_contour_[i] == loc_x) {
      ++shared_length;
    } else {
      if (shared_length < p_min_width) {
        is_well_rule_clean = false;
      }
    }
  }

  //    N well min-width
  if (is_well_rule_clean) {
    shared_length = 0;
    is_well_rule_clean = true;
    for (int i = boundary_row + 1; i <= hi_row; ++i) {
      if (block_contour_[i] == right_) continue;
      if (row_well_status_[i].is_n && block_contour_[i] == loc_x) {
        ++shared_length;
      } else {
        if (shared_length < n_min_width) {
          is_well_rule_clean = false;
        }
      }
    }
  }
  if (!is_well_rule_clean) {
    return false;
  }

  return true;
}

bool WellLegalizer::FindLocRight(Value2D<int> &loc, int width, int height, int p_row) {
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

  bool is_well_aligned;
  bool is_loc_legal;

  right_block_bound = (int) std::round(loc.x + k_left_ * width);
  //right_block_bound = loc.x;

  max_search_row = MaxRow(height);
  blk_row_height = HeightToRow(height);

  search_start_row = std::max(0, LocToRow(loc.y - 4 * height));
  search_end_row = std::min(max_search_row, LocToRow(loc.y + 5 * height));

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
    tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);

    /*int first_n_row = tmp_start_row + p_row;
    is_well_aligned = !row_well_status_[first_n_row - 1].is_n && row_well_status_[first_n_row].is_n;
    is_loc_legal = IsCurrentLocLegalRight(tmp_x - width, width, tmp_start_row, tmp_end_row, p_row);
    if (!is_loc_legal && !is_well_aligned) {
      tmp_cost += well_mis_align_cost_factor;
    }*/

    if (tmp_cost < min_cost) {
      best_loc_x = tmp_x;
      best_row = tmp_start_row;
      min_cost = tmp_cost;
    }
  }

  int best_row_legal = 0;
  int best_loc_x_legal = INT_MAX;
  double min_cost_legal = DBL_MAX;
  is_loc_legal = IsCurrentLocLegalRight(best_loc_x - width,
                                             width,
                                             best_row,
                                             best_row + blk_row_height - 1,
                                             p_row);;

  if (!is_loc_legal) {
    int old_start_row = search_start_row;
    int old_end_row = search_end_row;
    int extended_range = std::min(cur_iter_, 2) * blk_row_height;
    search_start_row = std::max(0, search_start_row - extended_range);
    search_end_row = std::min(max_search_row, search_end_row + extended_range);
    for (int tmp_start_row = search_start_row; tmp_start_row < old_start_row; ++tmp_start_row) {
      tmp_end_row = tmp_start_row + blk_row_height - 1;
      right_white_space_bound = WhiteSpaceBoundRight(loc.x - width, loc.x, tmp_start_row, tmp_end_row);

      tmp_x = std::min(right_white_space_bound, right_block_bound);

      for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
        tmp_x = std::min(tmp_x, block_contour_[n]);
      }

      tmp_y = RowToLoc(tmp_start_row);

      int first_n_row = tmp_start_row + p_row;
      is_well_aligned = !row_well_status_[first_n_row - 1].is_n && row_well_status_[first_n_row].is_n;

      tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);

      if (!is_well_aligned) {
        tmp_cost += well_mis_align_cost_factor;
      }

      if (tmp_cost < min_cost) {
        best_loc_x = tmp_x;
        best_row = tmp_start_row;
        min_cost = tmp_cost;
      }

      is_loc_legal = IsCurrentLocLegalRight(tmp_x - width, width, tmp_start_row, tmp_end_row, p_row);

      if (is_loc_legal && is_well_aligned) {
        if (tmp_cost < min_cost_legal) {
          best_loc_x_legal = tmp_x;
          best_row_legal = tmp_start_row;
          min_cost_legal = tmp_cost;
        }
      }
    }
    for (int tmp_start_row = old_end_row; tmp_start_row < search_end_row; ++tmp_start_row) {
      tmp_end_row = tmp_start_row + blk_row_height - 1;
      right_white_space_bound = WhiteSpaceBoundRight(loc.x - width, loc.x, tmp_start_row, tmp_end_row);

      tmp_x = std::min(right_white_space_bound, right_block_bound);

      for (int n = tmp_start_row; n <= tmp_end_row; ++n) {
        tmp_x = std::min(tmp_x, block_contour_[n]);
      }

      tmp_y = RowToLoc(tmp_start_row);

      int first_n_row = tmp_start_row + p_row;
      is_well_aligned = !row_well_status_[first_n_row - 1].is_n && row_well_status_[first_n_row].is_n;

      tmp_cost = std::abs(tmp_x - loc.x) + std::abs(tmp_y - loc.y);

      if (!is_well_aligned) {
        tmp_cost += well_mis_align_cost_factor;
      }
      if (tmp_cost < min_cost) {
        best_loc_x = tmp_x;
        best_row = tmp_start_row;
        min_cost = tmp_cost;
      }

      is_loc_legal = IsCurrentLocLegalRight(tmp_x - width, width, tmp_start_row, tmp_end_row, p_row);

      if (is_loc_legal && is_well_aligned) {
        if (tmp_cost < min_cost_legal) {
          best_loc_x_legal = tmp_x;
          best_row_legal = tmp_start_row;
          min_cost_legal = tmp_cost;
        }
      }
    }

  }

  // if still cannot find a legal location, enter fail mode
  is_successful = IsCurrentLocLegalRight(best_loc_x - width,
                                         width,
                                         best_row,
                                         best_row + blk_row_height - 1,
                                         p_row);
  if (!is_successful) {
    if (best_loc_x_legal <= right_ && best_loc_x_legal >= left_ + width) {
      is_successful = IsCurrentLocLegalRight(best_loc_x_legal - width,
                                             width,
                                             best_row_legal,
                                             best_row_legal + blk_row_height - 1,
                                             p_row);
    }
    if (is_successful) {
      best_loc_x = best_loc_x_legal;
      best_row = best_row_legal;
    }
  }

  loc.x = best_loc_x;
  loc.y = RowToLoc(best_row);;

  return is_successful;
}

bool WellLegalizer::WellLegalizationRight() {
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

  for (auto &&pair: index_loc_list_) {
    auto &block = block_list[pair.num];
    if (block.IsFixed()) continue;

    res.x = int(std::round(block.URX()));
    res.y = AlignedLocToRowLoc(block.LLY());
    height = int(block.Height());
    width = int(block.Width());

    p_well_row_height = HeightToRow(block.Type()->well_->GetPNBoundary());

    //std::cout << block.Num() << "\n";
    is_current_loc_legal = IsCurrentLocLegalRight(res, width, height, p_well_row_height);

    if (!is_current_loc_legal) {
      is_legal_loc_found = FindLocRight(res, width, height, p_well_row_height);
      if (!is_legal_loc_found) {
        is_successful = false;
      }
    }

    block.SetURX(res.x);
    block.SetLLY(res.y);

    UseSpaceRight(block);
  }

  return is_successful;
}

void WellLegalizer::StartPlacement() {
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
            << "    NN spacing: " << nn_spacing << "\n"
            << "    PP spacing: " << pp_spacing << "\n"
            << "    NP spacing: " << np_spacing << "\n"
            << "    MaxNDist:   " << n_max_plug_dist_ << "\n"
            << "    MaxPDist:   " << p_max_plug_dist_ << "\n"
            << "    MinNWidth:  " << n_min_width << "\n"
            << "    MinPWidth:  " << p_min_width << "\n";

  bool is_success = false;
  for (cur_iter_ = 0; cur_iter_ < max_iter_; ++cur_iter_) {
    std::cout << "Current iteration: " << cur_iter_ << "\n";
    well_mis_align_cost_factor = cur_iter_ + 1;
    if (legalize_from_left_) {
      is_success = WellLegalizationLeft();
    } else {
      is_success = WellLegalizationRight();
    }
    std::cout << "Current iteration: " << is_success << "  " << legalize_from_left_ << "\n";
    legalize_from_left_ = !legalize_from_left_;
    //++k_left_;
    GenMATLABWellTable("lg" + std::to_string(cur_iter_) + "_result");
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

}
