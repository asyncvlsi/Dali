//
// Created by Yihang Yang on 1/2/20.
//

#include "pushpulllegalizer.h"

#include <algorithm>

PushPullLegalizer::PushPullLegalizer()
    : Placer(), is_push_(true) {}

void PushPullLegalizer::InitLegalizer() {
  row_start_.resize(Top() - Bottom() + 1, Left());
  IndexLocPair<int> tmp_index_loc_pair(0, 0, 0);
  index_loc_list_.resize(circuit_->block_list.size(), tmp_index_loc_pair);
}

bool PushPullLegalizer::IsSpaceLegal(Block const &block) {
  /****
 * check if the block is out of the placement region
 * check whether any part of the space is used
 * ****/

  if (block.LLX() < Left() || block.URX() > Right()) {
    return false;
  }
  if (block.LLY() < Bottom() || block.URY() > Top()) {
    return false;
  }

  auto start_row = (unsigned int) (block.LLY() - Bottom());
  unsigned int end_row = start_row + block.Height() - 1;
  int lx = int(block.LLX());

  if (end_row >= row_start_.size()) {
    return false;
  }

  bool is_avail = true;
  for (unsigned int i = start_row; i <= end_row; ++i) {
    if (row_start_[i] > lx) {
      is_avail = false;
      break;
    }
  }

  return is_avail;
}

void PushPullLegalizer::UseSpace(Block const &block) {
  /****
   * Mark the space used by this block by changing the start point of available space in each related row
   * ****/
  auto start_row = (unsigned int) (block.LLY() - Bottom());
  unsigned int end_row = start_row + block.Height() - 1;
  int lx = int(block.LLX());

  if (end_row >= row_start_.size()) {
    //std::cout << "  ly:     " << int(block.LLY())       << "\n"
    //          << "  height: " << block.Height()   << "\n"
    //          << "  top:    " << Top()    << "\n"
    //          << "  bottom: " << Bottom() << "\n";
    Assert(false, "Cannot use space out of range");
  }

  int end_x = lx + int(std::round(block.Width()));

  for (unsigned int i = start_row; i <= end_row; ++i) {
    row_start_[i] = end_x;
  }
}

bool PushPullLegalizer::PushBlock(Block &block) {
  /****
   * For each row:
   *    Find the non-overlap location
   *    Move the location rightwards to respect overlap rules
   *
   * Cost function is displacement
   * ****/
  int init_x = int(block.LLX());
  int init_y = int(block.LLY());

  int height = int(block.Height());
  int start_row = 0;
  int end_row = Top() - Bottom() - height;
  //std::cout << "    Starting row: " << start_row << "\n"
  //          << "    Ending row:   " << end_row   << "\n"
  //          << "    Block Height: " << block.Height() << "\n"
  //          << "    Top of the placement region " << Top() << "\n"
  //          << "    Number of rows: " << row_start_.size() << "\n"
  //          << "    LX and LY: " << int(block.LLX()) << "  " << int(block.LLY()) << "\n";

  int best_row = 0;
  int best_loc = INT_MIN;
  int min_cost = INT_MAX;

  int tmp_cost = INT_MAX;
  int tmp_end_row = 0;
  int tmp_loc = INT_MIN;
  for (int tmp_row = start_row; tmp_row <= end_row; ++tmp_row) {
    // 1. find the non-overlap location
    tmp_end_row = tmp_row + height - 1;
    tmp_loc = Left();
    for (int i = tmp_row; i <= tmp_end_row; ++i) {
      tmp_loc = std::max(tmp_loc, row_start_[i]);
    }

    tmp_cost = std::abs(tmp_loc - init_x) + std::abs(tmp_row + Bottom() - init_y);
    if (tmp_cost < min_cost) {
      best_loc = tmp_loc;
      best_row = tmp_row;
      min_cost = tmp_cost;
    }

  }

  int res_x = best_loc;
  int res_y = best_row + Bottom();

  //std::cout << res.x << "  " << res.y << "  " << min_cost << "  " << block.Num() << "\n";

  if (min_cost < INT_MAX) {
    block.SetLoc(res_x, res_y);
    return true;
  }

  return false;
}

void PushPullLegalizer::PushLegalization() {
  std::vector<Block> &block_list = *BlockList();
  for (size_t i = 0; i < index_loc_list_.size(); ++i) {
    index_loc_list_[i].num = i;
    index_loc_list_[i].x = block_list[i].LLX();
    index_loc_list_[i].y = block_list[i].LLY();
  }
  std::sort(index_loc_list_.begin(), index_loc_list_.end());
  for (auto &pair: index_loc_list_) {
    auto &block = block_list[pair.num];
    if (block.IsFixed()) {
      continue;
    }
    bool is_cur_loc_legal = IsSpaceLegal(block);
    if (!is_cur_loc_legal) {
      bool loc_found = PushBlock(block);
      if (!loc_found) {
        Assert(false, "Cannot find legal location");
      }
    }
    UseSpace(block);
    //std::cout << block.LLX() << "  " << block.LLY() << "\n";
  }
}

double PushPullLegalizer::GetPartialHPWL(Block &block, int x, int y) {
  double max_x = x;
  double max_y = y;
  double min_x = x;
  double min_y = y;
  double tot_hpwl = 0;
  Net *net = nullptr;
  for (auto &&net_num: block.net_list) {
    net = &(circuit_->net_list[net_num]);
    for (auto &&blk_pin_pair: net->blk_pin_list) {
      if (blk_pin_pair.GetBlock()->GetPlaceStatus() == PLACED) {
        min_x = std::min(min_x, blk_pin_pair.AbsX());
        min_y = std::min(min_y, blk_pin_pair.AbsY());
        max_x = std::max(max_x, blk_pin_pair.AbsX());
        max_y = std::max(max_y, blk_pin_pair.AbsY());
      }
    }
    tot_hpwl += (max_x - min_x) + (max_y - min_y);
  }

  return tot_hpwl;
}

bool PushPullLegalizer::PullBlockLeft(Block &block) {
  /****
   * For each row:
   *    if the row is not used before, do not change the location of this block (this kind of block is termed starting_block)
   *    if all rows are used, find the location with the minimum partial HPWL
   *    the partial HPWL is calculated in the following way:
   *        only calculate the HPWL of placed cells
   * Cost function is displacement
   * ****/
  int init_x = int(block.LLX());
  int init_y = int(block.LLY());

  int height = int(block.Height());
  int start_row = 0;
  int end_row = Top() - Bottom() - height;
  //std::cout << "    Starting row: " << start_row << "\n"
  //          << "    Ending row:   " << end_row   << "\n"
  //          << "    Block Height: " << block.Height() << "\n"
  //          << "    Top of the placement region " << Top() << "\n"
  //          << "    Number of rows: " << row_start_.size() << "\n"
  //          << "    LX and LY: " << int(block.LLX()) << "  " << int(block.LLY()) << "\n";

  int best_row = 0;
  int best_loc = INT_MIN;
  double min_cost = DBL_MAX;
  double tmp_cost = DBL_MAX;
  int tmp_end_row = 0;
  int tmp_x = INT_MIN;
  int tmp_y = INT_MIN;

  bool is_starting_block = false;
  double tmp_hpwl = 0;
  double tmp_displacement = 0;
  for (int tmp_row = start_row; tmp_row <= end_row; ++tmp_row) {
    if (row_start_[tmp_row] == Left()) {
      is_starting_block = true;
      break;
    }

    tmp_end_row = tmp_row + height - 1;
    tmp_x = Left();
    for (int i = tmp_row; i <= tmp_end_row; ++i) {
      tmp_x = std::max(tmp_x, row_start_[i]);
    }

    tmp_y = tmp_row + Bottom();

    tmp_hpwl = GetPartialHPWL(block, tmp_x, tmp_y);
    tmp_displacement = std::abs(tmp_x - init_x) + std::abs(tmp_y - init_y);

    tmp_cost = tmp_displacement + tmp_hpwl;
    if (tmp_cost < min_cost) {
      best_loc = tmp_x;
      best_row = tmp_row;
      min_cost = tmp_cost;
    }
  }
  if (is_starting_block) {
    return true;
  }

  int res_x = best_loc;
  int res_y = best_row + Bottom();

  //std::cout << res.x << "  " << res.y << "  " << min_cost << "  " << block.Num() << "\n";

  if (min_cost < DBL_MAX) {
    block.SetLoc(res_x, res_y);
    return true;
  }

  return false;
}

void PushPullLegalizer::PullLegalizationFromLeft() {
  row_start_.assign(row_start_.size(), Left());
  std::vector<Block> &block_list = *BlockList();
  for (size_t i = 0; i < index_loc_list_.size(); ++i) {
    index_loc_list_[i].num = i;
    index_loc_list_[i].x = block_list[i].LLX();
    index_loc_list_[i].y = block_list[i].LLY();
    block_list[i].SetPlaceStatus(UNPLACED);
  }
  std::sort(index_loc_list_.begin(), index_loc_list_.end());
  for (auto &pair: index_loc_list_) {
    auto &block = block_list[pair.num];
    if (block.IsFixed()) {
      continue;
    }
    PullBlockLeft(block);
    //std::cout << block.LLX() << "  " << block.LLY() << "\n";
    UseSpace(block);
    block.SetPlaceStatus(PLACED);
  }
}

bool PushPullLegalizer::PullBlockRight(Block &block) {
  /****
   * For each row:
   *    if the row is not used before, do not change the location of this block (this kind of block is termed starting_block)
   *    if all rows are used, find the location with the minimum partial HPWL
   *    the partial HPWL is calculated in the following way:
   *        only calculate the HPWL of placed cells
   * Cost function is displacement
   * ****/
  int init_x = int(block.URX());
  int init_y = int(block.LLY());

  int height = int(block.Height());
  int start_row = 0;
  int end_row = Top() - Bottom() - height;
  //std::cout << "    Starting row: " << start_row << "\n"
  //          << "    Ending row:   " << end_row   << "\n"
  //          << "    Block Height: " << block.Height() << "\n"
  //          << "    Top of the placement region " << Top() << "\n"
  //          << "    Number of rows: " << row_start_.size() << "\n"
  //          << "    LX and LY: " << int(block.LLX()) << "  " << int(block.LLY()) << "\n";

  int best_row = 0;
  int best_loc = INT_MIN;
  double min_cost = DBL_MAX;
  double tmp_cost = DBL_MAX;
  int tmp_end_row = 0;
  int tmp_x = INT_MIN;
  int tmp_y = INT_MIN;

  bool is_starting_block = false;
  double tmp_hpwl = 0;
  double tmp_displacement = 0;
  for (int tmp_row = start_row; tmp_row <= end_row; ++tmp_row) {
    if (row_start_[tmp_row] == Right()) {
      is_starting_block = true;
      break;
    }

    tmp_end_row = tmp_row + height - 1;
    tmp_x = Right();
    for (int i = tmp_row; i <= tmp_end_row; ++i) {
      tmp_x = std::max(tmp_x, row_start_[i]);
    }

    tmp_y = tmp_row + Bottom();

    tmp_hpwl = GetPartialHPWL(block, tmp_x, tmp_y);
    tmp_displacement = std::abs(tmp_x - init_x) + std::abs(tmp_y - init_y);

    tmp_cost = tmp_displacement + tmp_hpwl;
    if (tmp_cost < min_cost) {
      best_loc = tmp_x;
      best_row = tmp_row;
      min_cost = tmp_cost;
    }
  }
  if (is_starting_block) {
    return true;
  }

  int res_x = best_loc;
  int res_y = best_row + Bottom();

  //std::cout << res.x << "  " << res.y << "  " << min_cost << "  " << block.Num() << "\n";

  if (min_cost < DBL_MAX) {
    block.SetURX(res_x);
    block.SetLLY(res_y);
    return true;
  }

  return false;
}

void PushPullLegalizer::PullLegalizationFromRight() {
  row_start_.assign(row_start_.size(), Right());
  std::vector<Block> &block_list = *BlockList();
  for (size_t i = 0; i < index_loc_list_.size(); ++i) {
    index_loc_list_[i].num = i;
    index_loc_list_[i].x = block_list[i].URX();
    index_loc_list_[i].y = block_list[i].URY();
    block_list[i].SetPlaceStatus(UNPLACED);
  }
  std::sort(index_loc_list_.begin(),
            index_loc_list_.end(),
            [](const IndexLocPair<int> &lhs, const IndexLocPair<int> &rhs) {
              return (lhs.x > rhs.x) || (lhs.x == rhs.x && lhs.y > rhs.y);
            });
  for (auto &pair: index_loc_list_) {
    auto &block = block_list[pair.num];
    if (block.IsFixed()) {
      continue;
    }
    PullBlockRight(block);
    UseSpace(block);
    block.SetPlaceStatus(PLACED);
  }
}

void PushPullLegalizer::StartPlacement() {
  InitLegalizer();

  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "Start PushPull Legalization\n";
  }

  if (is_push_) {
    PushLegalization();
    ReportHPWL(LOG_CRITICAL);
  }
  PullLegalizationFromLeft();
  ReportHPWL(LOG_CRITICAL);
  PullLegalizationFromRight();

  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "\033[0;36m"
              << "PushPull Legalization complete!\n"
              << "\033[0m";
  }

  ReportHPWL(LOG_CRITICAL);

}