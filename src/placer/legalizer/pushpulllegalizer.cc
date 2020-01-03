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
 * check whether any part of the space is used
 * ****/
  auto start_row = (unsigned int)(block.LLY() - Bottom());
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
  auto start_row = (unsigned int)(block.LLY() - Bottom());
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
   *    Move the location rightwards to respect well rules if necessary
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
    //bool is_cur_loc_legal = IsSpaceLegal(block);
    bool is_cur_loc_legal = false;
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

bool PushPullLegalizer::PullBlock(Block &block) {

}

void PushPullLegalizer::PullLegalization() {
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
    PullBlock(block);
    //std::cout << block.LLX() << "  " << block.LLY() << "\n";
  }
}

void PushPullLegalizer::StartPlacement() {
  InitLegalizer();

  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "Start PushPull Legalization\n";
  }

  if (is_push_) {
    PushLegalization();
  }
  PullLegalization();

  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "\033[0;36m"
              << "PushPull Legalization complete!\n"
              << "\033[0m";
  }

  ReportHPWL(LOG_CRITICAL);

}
