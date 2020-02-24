//
// Created by Yihang Yang on 12/22/19.
//

#include "welllegalizer.h"

#include <algorithm>

WellLegalizer::WellLegalizer() = default;

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

  Row tmp_row(RegionLeft(), INT_MAX, false);
  all_rows_.resize(RegionTop() - RegionBottom() + 1, tmp_row);

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

bool WellLegalizer::IsSpaceLegal(int lo_x, int hi_x, int lo_row, int hi_row) {
  /****
   * 1. check whether any part of the space is used
   * 2. check whether NP well rules are disobeyed
   * ****/
  int start_row = lo_row;
  int end_row = hi_row;
  int lx = lo_x;

  if (end_row >= int(all_rows_.size())) {
    return false;
  }

  bool is_avail = true;
  // 1. check the overlap rule
  for (int i = start_row; i <= end_row; ++i) {
    if (all_rows_[i].start > lx) {
      is_avail = false;
      break;
    }
  }

  // 2. check the NP well rule

  return is_avail;
}

void WellLegalizer::UseSpaceLeft(Block const &block) {
  /****
   * Mark the space used by this block by changing the start point of available space in each related row
   * ****/
  int start_row = StartRow(int(block.LLY()));
  int end_row = EndRow(int(block.URY()));

  /*if (end_row >= int(all_rows_.size())) {
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
    all_rows_[i].start = end_x;
    all_rows_[i].is_n = (i > pn_boundary_row);
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
  //          << "    Number of rows: " << all_rows_.size() << "\n"
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
      tmp_loc = std::max(tmp_loc, all_rows_[i].start);
    }

    // 2. legalize the location to respect well rules

    //    2.a check if the distance rule is satisfied
    pn_boundary_row = tmp_row + p_height;
    //    P well distance
    for (int i = tmp_row; i <= pn_boundary_row; ++i) {
      if (all_rows_[i].start == RegionLeft()) continue;
      if (!all_rows_[i].is_n) {
        if (tmp_loc > all_rows_[i].start && tmp_loc < all_rows_[i].start + pp_spacing) {
          tmp_loc = all_rows_[i].start + pp_spacing;
        }
      } else {
        if (tmp_loc > all_rows_[i].start && tmp_loc < all_rows_[i].start + np_spacing) {
          tmp_loc = all_rows_[i].start + np_spacing;
        }
      }
    }

    //    N well distance
    for (int i = pn_boundary_row + 1; i <= tmp_end_row; ++i) {
      if (all_rows_[i].start == RegionLeft()) continue;
      if (all_rows_[i].is_n) {
        if (tmp_loc > all_rows_[i].start && tmp_loc < all_rows_[i].start + nn_spacing) {
          tmp_loc = all_rows_[i].start + nn_spacing;
        }
      } else {
        if (tmp_loc > all_rows_[i].start && tmp_loc < all_rows_[i].start + np_spacing) {
          tmp_loc = all_rows_[i].start + np_spacing;
        }
      }
    }

    //    2.b check if the min-width rule is satisfied.
    //    P well min-width
    int shared_length = 0;
    for (int i = tmp_row; i <= pn_boundary_row; ++i) {
      if (all_rows_[i].start == RegionLeft()) continue;
      if (!(all_rows_[i].is_n) && all_rows_[i].start == tmp_loc) {
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
      if (all_rows_[i].start == RegionLeft()) continue;
      if (all_rows_[i].is_n && all_rows_[i].start == tmp_loc) {
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
      if (all_rows_[i].start == tmp_loc) {
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
    if (all_rows_[i].dist < n_max_plug_dist_) {
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

bool WellLegalizer::IsCurrentLocWellRuleClean(int p_row, Value2D<int> &loc, int width, int height) {
  /****
   * Returns whether the current location respects Well rules
   *
   * 1. check N/P well distance to its left hand side neighbors
   * ****/
  bool is_well_rule_clean = true;
  int bottom_row = StartRow(loc.y);
  int boundary_row = bottom_row + p_row;
  int top_row = EndRow(loc.y + height);

  assert(boundary_row > bottom_row);
  assert(top_row > boundary_row);

  bool tmp_row_well_clean = false;
  for (int i = bottom_row; i <= boundary_row; ++i) {
    if (all_rows_[i].start == RegionLeft()) continue;
    if (!all_rows_[i].is_n) {
      tmp_row_well_clean = (loc.x > all_rows_[i].start && loc.x < all_rows_[i].start + pp_spacing);
    } else {
      tmp_row_well_clean = (loc.x > all_rows_[i].start && loc.x < all_rows_[i].start + np_spacing);
    }
    if (!tmp_row_well_clean) {
      is_well_rule_clean = false;
      break;
    }
  }

  if (!is_well_rule_clean) {
    for (int i = boundary_row + 1; i <= top_row; ++i) {
      if (all_rows_[i].start == RegionLeft()) continue;
      if (all_rows_[i].is_n) {
        tmp_row_well_clean = (loc.x > all_rows_[i].start && loc.x < all_rows_[i].start + nn_spacing) ;
      } else {
        tmp_row_well_clean = (loc.x > all_rows_[i].start && loc.x < all_rows_[i].start + np_spacing) ;
      }
      if (!tmp_row_well_clean) {
        is_well_rule_clean = false;
        break;
      }
    }
  }

  return is_well_rule_clean;
}

bool WellLegalizer::IsCurrentLocLegalLeft(Value2D<int> &loc, int width, int height) {
  /****
   * Returns whether the current location is legal
   *
   * 1. if the space itself is illegal, then return false
   * 2. if the space covers placed blocks, then return false
   * 3. otherwise, return true
   * ****/

  bool is_current_loc_legal = true;
  int start_row = StartRow(loc.y);
  int end_row = EndRow(loc.y + height);

  // 1. check if the space itself is legal
  is_current_loc_legal = IsSpaceLegal(loc.x, loc.x + width, start_row, end_row);
  if (!is_current_loc_legal) {
    return false;
  }

  // 2. check if the space covers any placed blocks
  is_current_loc_legal = true;
  for (int i = start_row; i <= end_row; ++i) {
    if (block_contour_[i] > loc.x) {
      is_current_loc_legal = false;
      break;
    }
  }

  return is_current_loc_legal;
}

int WellLegalizer::WhiteSpaceBoundLeft(int lo_x, int hi_x, int lo_row, int hi_row) {

}

bool WellLegalizer::FindLocLeft(Value2D<int> &loc, int width, int height) {

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

  double init_x;
  double init_y;
  int height;
  int width;

  Value2D<int> res;
  bool is_current_loc_legal;
  bool is_current_well_rule_legal;
  bool is_legal_loc_found;

  for (auto &&pair: index_loc_list_) {
    auto &block = block_list[pair.num];

    if (block.IsFixed()) continue;

    init_x = int(std::round(block.LLX()));
    init_y = AlignedLocToRowLoc(block.LLY());
    height = int(block.Height());
    width = int(block.Width());

    res.x = init_x;
    res.y = init_y;

    is_current_loc_legal = IsCurrentLocLegalLeft(res, width, height);
    is_current_well_rule_legal = false;
    if (is_current_loc_legal) {
      int p_well_row = HeightToRow(block.Type()->well_->GetPNBoundary());
      is_current_well_rule_legal = IsCurrentLocWellRuleClean(p_well_row, res, width, height);
    }

    if (!is_current_loc_legal || !is_current_well_rule_legal) {
      is_legal_loc_found = FindLocLeft(res, width, height);
      if (!is_legal_loc_found) {
        is_successful = false;
        //std::cout << res.x << "  " << res.y << "  " << block.Num() << " left\n";
        //break;
      }
    }

    block.SetLoc(res.x, res.y);

    UseSpaceLeft(block);
  }

/*  for (auto &pair: index_loc_list_) {
    auto &block = block_list[pair.num];
    if (block.IsFixed()) {
      continue;
    }
    WellPlace(block);
    //std::cout << block.LLX() << "  " << block.LLY() << "\n";
  }*/

  return is_successful;
}

bool WellLegalizer::WellLegalizationRight() {

}

void WellLegalizer::StartPlacement() {
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "---------------------------------------\n"
              << "Start Well Legalization\n";
  }

  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  InitWellLegalizer();
  std::cout << "  Number of rows: " << all_rows_.size() << "\n"
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
    if (legalize_from_left_) {
      is_success = WellLegalizationLeft();
    } else {
      is_success = WellLegalizationRight();
    }
    legalize_from_left_ = !legalize_from_left_;
    ++k_left_;
    //GenMATLABTable("lg" + std::to_string(cur_iter_) + "_result.txt");
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
