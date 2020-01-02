//
// Created by yy492 on 1/2/20.
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

void PushPullLegalizer::PushBlock(Block &block) {

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
    PushBlock(block);
    //std::cout << block.LLX() << "  " << block.LLY() << "\n";
  }
}

void PushPullLegalizer::PullBlock(Block &block) {

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
}
