//
// Created by Yihang Yang on 1/2/20.
//

#include "pushpulllegalizer.h"

#include <cfloat>

#include <algorithm>

PushPullLegalizer::PushPullLegalizer()
    : Placer(), is_push_(true), is_pull_from_left_(true), cur_iter_(0), max_iter_(5) {}

void PushPullLegalizer::InitLegalizer() {
  row_start_.resize(Top() - Bottom() + 1, Left());
  IndexLocPair<int> tmp_index_loc_pair(0, 0, 0);
  index_loc_list_.resize(circuit_->block_list.size(), tmp_index_loc_pair);
}

bool PushPullLegalizer::IsSpaceLegal(Block &block) {
  /****
 * check if the block is out of the placement region
 * check whether any part of the space is used
 * ****/

  auto blk_left   = int(std::round(block.LLX()));
  auto blk_right  = int(std::round(block.URX()));
  auto blk_bottom = int(std::round(block.LLY()));
  auto blk_top    = int(std::round(block.URY()));


  // 1. check whether the x location of this block is out of the region
  if (blk_left < Left() || blk_right > Right()) {
    return false;
  }
  // 2. check whether the y location of this block is out of the region
  if (blk_bottom < Bottom() || blk_top > Top()) {
    return false;
  }

  // 3. check if any part of the current location is used
  auto start_row = (unsigned int) (blk_bottom - Bottom());
  unsigned int end_row = start_row + block.Height() - 1;
  bool is_avail = true;
  for (unsigned int i = start_row; i <= end_row; ++i) {
    if (row_start_[i] > blk_left) {
      is_avail = false;
      break;
    }
  }

  if (is_avail) {
    block.SetLoc(blk_left, blk_bottom);
  }
  return is_avail;
}

void PushPullLegalizer::UseSpace(Block const &block) {
  /****
   * Mark the space used by this block by changing the start point of available space in each related row
   * ****/
  auto start_row = (unsigned int) (block.LLY() - Bottom());
  unsigned int end_row = start_row + block.Height() - 1;
  if (end_row >= row_start_.size()) {
    //std::cout << "  ly:     " << int(block.LLY())       << "\n"
    //          << "  height: " << block.Height()   << "\n"
    //          << "  top:    " << Top()    << "\n"
    //          << "  bottom: " << Bottom() << "\n";
    Assert(false, "Cannot use space out of range");
  }

  int end_x = int(block.URX());
  for (unsigned int i = start_row; i <= end_row; ++i) {
    row_start_[i] = end_x;
  }
}

void PushPullLegalizer::FastShift(unsigned int failure_point) {
  
}

bool PushPullLegalizer::PushBlock(Block &block) {
  /****
   * For each row:
   *    Find the non-overlap location
   *    Move the location rightwards to respect overlap rules
   *
   * Cost function is displacement
   * ****/
  auto init_x = int(std::round(block.LLX()));
  auto init_y = int(std::round(block.LLY()));
  auto height = int(block.Height());
  auto right_most  = right_ - int(block.Width());
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

  int tmp_cost;
  int tmp_end_row = 0;
  int tmp_loc;

  int min_row = std::max(0, init_y - bottom_ - 2 * height);
  int max_row = std::min(end_row, init_y - bottom_ + 2 * height);

  bool all_row_fail = true;

  while (all_row_fail) {
    int old_min_row = min_row;
    int old_max_row = max_row;
    min_row = std::max(0, min_row - 2 * height);
    max_row = std::min(end_row, max_row + 2 * height);

    for (int tmp_row = min_row; tmp_row < max_row; ++tmp_row) {
      if (tmp_row >= old_min_row && tmp_row < old_max_row) continue;
      // 1. find the non-overlap location
      tmp_end_row = tmp_row + height - 1;
      tmp_loc = Left();
      for (int i = tmp_row; i <= tmp_end_row; ++i) {
        tmp_loc = std::max(tmp_loc, row_start_[i]);
      }
      if (tmp_loc > right_most) continue;
      all_row_fail = false;
      tmp_cost = std::abs(tmp_loc - init_x) + std::abs(tmp_row + Bottom() - init_y);
      if (tmp_cost < min_cost) {
        best_loc = tmp_loc;
        best_row = tmp_row;
        min_cost = tmp_cost;
      }

    }

    if (min_row <= 0 && max_row >= end_row) break;
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

bool PushPullLegalizer::PushLegalizationFromLeft() {
  std::vector<Block> &block_list = *BlockList();
  for (size_t i = 0; i < index_loc_list_.size(); ++i) {
    index_loc_list_[i].num = i;
    index_loc_list_[i].x = block_list[i].LLX();
    index_loc_list_[i].y = block_list[i].LLY();
  }
  std::sort(index_loc_list_.begin(), index_loc_list_.end());
  unsigned int sz = index_loc_list_.size();
  unsigned int block_num;
  for (unsigned int i = 0; i < sz; ++i) {
    block_num = index_loc_list_[i].num;
    auto &block = block_list[block_num];
    if (block.IsFixed()) {
      continue;
    }
    bool is_cur_loc_legal = IsSpaceLegal(block);
    if (!is_cur_loc_legal) {
      bool loc_found = PushBlock(block);
      if (!loc_found) {
        GenMATLABTable("lg_result.txt");
        if (globalVerboseLevel >= LOG_CRITICAL) {
          std::cout << "  WARNING:  " << cur_iter_ << "-iteration push legalization\n"
                    << "        this may disturb the global placement, and may impact the placement quality\n";
        }
        FastShift(i);
        return false;
      }
    }
    UseSpace(block);
    //std::cout << block.LLX() << "  " << block.LLY() << "\n";
  }
  return true;
}

bool PushPullLegalizer::PushLegalizationFromRight() {
  std::vector<Block> &block_list = *BlockList();
  for (size_t i = 0; i < index_loc_list_.size(); ++i) {
    index_loc_list_[i].num = i;
    index_loc_list_[i].x = block_list[i].LLX();
    index_loc_list_[i].y = block_list[i].LLY();
  }
  std::sort(index_loc_list_.begin(), index_loc_list_.end());
  unsigned int sz = index_loc_list_.size();
  unsigned int block_num;
  for (unsigned int i = 0; i < sz; ++i) {
    block_num = index_loc_list_[i].num;
    auto &block = block_list[block_num];
    if (block.IsFixed()) {
      continue;
    }
    bool is_cur_loc_legal = IsSpaceLegal(block);
    if (!is_cur_loc_legal) {
      bool loc_found = PushBlock(block);
      if (!loc_found) {
        GenMATLABTable("lg_result.txt");
        if (globalVerboseLevel >= LOG_CRITICAL) {
          std::cout << "  WARNING:  " << cur_iter_ << "-iteration push legalization\n"
                    << "        this may disturb the global placement, and may impact the placement quality\n";
        }
        FastShift(i);
        return false;
      }
    }
    UseSpace(block);
    //std::cout << block.LLX() << "  " << block.LLY() << "\n";
  }
  return true;
}

double PushPullLegalizer::EstimatedHPWL(Block &block, int x, int y) {
  double max_x = x;
  double max_y = y;
  double min_x = x;
  double min_y = y;
  double tot_hpwl = 0;
  for (auto &net_num: block.net_list) {
    for (auto &blk_pin_pair: circuit_->net_list[net_num].blk_pin_list) {
      if (blk_pin_pair.GetBlock() != &block) {
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
   *    if all rows are used, find the location with the minimum HPWL
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
  auto min_cost = DBL_MAX;
  double tmp_cost;
  int tmp_end_row = 0;
  int tmp_x;
  int tmp_y;

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

    tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);
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
    bool is_cur_loc_legal = IsSpaceLegal(block);
    if (!is_cur_loc_legal) {
      bool loc_found = PullBlockLeft(block);
      if (!loc_found) {
        Assert(false, "Cannot find legal location");
      }
    }
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
  double tmp_cost;
  int tmp_end_row = 0;
  int tmp_x;
  int tmp_y;

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

    tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);
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
    bool is_cur_loc_legal = IsSpaceLegal(block);
    if (!is_cur_loc_legal) {
      bool loc_found = PullBlockRight(block);
      if (!loc_found) {
        Assert(false, "Cannot find legal location");
      }
    }
    UseSpace(block);
    block.SetPlaceStatus(PLACED);
  }
}

void PushPullLegalizer::StartPlacement() {
  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();

  InitLegalizer();

  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "Start PushPull Legalization\n";
  }

  bool is_successful = false;
  /*if (is_push_) {
    if (globalVerboseLevel >= LOG_CRITICAL) {
      std::cout << "Push...\n";
    }
    for (cur_iter_ = 0; cur_iter_ < max_iter_; ++cur_iter_) {
      if (is_pull_from_left_) {
        is_successful = PushLegalizationFromLeft();
      } else {
        is_successful = PushLegalizationFromRight();
      }
      if (is_successful) {
        break;
      }
    }
    ReportHPWL(LOG_CRITICAL);
  }*/
  if (!is_successful) {
    ClosePackLegalization();
  }
  /*if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "Pull...\n";
  }
  PullLegalizationFromLeft();
  ReportHPWL(LOG_CRITICAL);*/
  //PullLegalizationFromRight();
  //ReportHPWL(LOG_CRITICAL);

  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "\033[0;36m"
              << "PushPull Legalization complete!\n"
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

}

bool PushPullLegalizer::ClosePackLegalization() {
  row_start_.assign(row_start_.size(), Left());
  std::vector<Block> &block_list = *BlockList();
  for (size_t i = 0; i < index_loc_list_.size(); ++i) {
    index_loc_list_[i].num = i;
    index_loc_list_[i].x = block_list[i].LLX();
    index_loc_list_[i].y = block_list[i].LLY();
    block_list[i].SetPlaceStatus(UNPLACED);
  }
  std::sort(index_loc_list_.begin(), index_loc_list_.end());

  int init_x;
  int init_y;
  int height;
  int width;
  int start_row;
  int end_row;
  int best_row;
  int best_loc;
  double min_cost;
  double tmp_cost;
  int tmp_end_row;
  int tmp_x;
  int tmp_y;

  for (auto &pair: index_loc_list_) {
    auto &block = block_list[pair.num];

    init_x = int(block.LLX());
    init_y = int(block.LLY());

    height = int(block.Height());
    width = int(block.Width());
    start_row = std::max(0, init_y - Bottom() - 2 * height);
    end_row = std::min(Top() - Bottom() - height, init_y - Bottom() + 3 * height);

    best_row = 0;
    best_loc = INT_MIN;
    min_cost = DBL_MAX;

    for (int tmp_row = start_row; tmp_row <= end_row; ++tmp_row) {
      tmp_end_row = tmp_row + height - 1;
      tmp_x = std::max(Left(), init_x - 1*width);
      for (int i = tmp_row; i <= tmp_end_row; ++i) {
        tmp_x = std::max(tmp_x, row_start_[i]);
      }

      tmp_y = tmp_row + Bottom();
      //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

      tmp_cost = std::abs(tmp_x - init_x) + std::abs(tmp_y - init_y);
      if (tmp_cost < min_cost) {
        best_loc = tmp_x;
        best_row = tmp_row;
        min_cost = tmp_cost;
      }
    }

    int res_x = best_loc;
    int res_y = best_row + Bottom();

    //std::cout << res_x << "  " << res_y << "  " << min_cost << "  " << block.Num() << "\n";

    block.SetLoc(res_x, res_y);

    UseSpace(block);
    block.SetPlaceStatus(PLACED);
  }


  return true;
}
