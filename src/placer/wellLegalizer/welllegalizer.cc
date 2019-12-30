//
// Created by Yihang Yang on 12/22/19.
//

#include <algorithm>
#include "welllegalizer.h"

void WellLegalizer::InitWellLegalizer() {
  Assert(circuit_ != nullptr, "Well Legalization fail: no input circuit!");
  Tech *tech_parm = circuit_->GetTech();
  Assert(tech_parm != nullptr, "Well Legalization fail: no technology parameters found!");

  WellLayer *n_well, *p_well;
  n_well = tech_parm->GetNWell();
  p_well = tech_parm->GetPWell();
  Assert(n_well != nullptr, "Well Legalization fail: no N well parameters found!");
  Assert(p_well != nullptr, "Well Legalization fail: no P well parameters found!");

  n_max_plug_dist_ = std::ceil(n_well->MaxPlugDist()/circuit_->GetGridValueX());
  p_max_plug_dist_ = std::ceil(p_well->MaxPlugDist()/circuit_->GetGridValueX());

  Row tmp_row(Left(), INT_MAX, false);
  all_rows_.resize(Top()-Bottom()+1, tmp_row);
  IndexLocPair<int> tmp_index_loc_pair(0,0,0);
  index_loc_list_.resize(circuit_->block_list.size(), tmp_index_loc_pair);
}

void WellLegalizer::SwitchToPlugType(Block &block) {
  auto well = block.Type()->GetWell();
  Assert(well != nullptr, "Block does not have a well, cannot switch to the plugged version: " + *block.Name());
  if (well->IsUnplug()) {
    auto type = block.Type();
    auto cluster = well->GetCluster();
    auto plugged_well = cluster->GetPlug();
    Assert(cluster != nullptr || plugged_well == nullptr, "There is not plugged version of this BlockType: " + *type->Name());
    block.SetType(plugged_well->Type());
  }
}

bool WellLegalizer::IsSpaceAvail(int lx, int ly, int width, int height) {
  unsigned int start_row = ly - Bottom();
  unsigned int end_row = start_row + height;

  if (end_row >= all_rows_.size()) return false;

  bool is_avail = true;
  for (unsigned int i=start_row; i<=end_row; ++i) {
    if (all_rows_[i].start > lx) {
      is_avail = false;
      break;
    }
  }

  return is_avail;
}

void WellLegalizer::UseSpace(int lx, int ly, int width, int height) {
  unsigned int start_row = ly - Bottom();
  unsigned int end_row = start_row + height;

  if (end_row >= all_rows_.size()) {
    //std::cout << "  ly:     " << ly       << "\n"
    //          << "  height: " << height   << "\n"
    //          << "  top:    " << Top()    << "\n"
    //          << "  bottom: " << Bottom() << "\n";
    Assert(false, "Cannot use space out of range");
  }

  int end_x = lx + width;

  for (unsigned int i=start_row; i<=end_row; ++i) {
    all_rows_[i].start = end_x;
  }
}

void WellLegalizer::FindLocation(Block &block, int &lx, int &ly) {
  unsigned int start_row = ly - Bottom();
  unsigned int end_row = start_row + block.Height();
  if (end_row >= all_rows_.size()) {
    ly = Top() - block.Height();
    //std::cout << "  ly:     " << ly       << "\n"
    //          << "  height: " << block.Height()   << "\n"
    //          << "  top:    " << Top()    << "\n"
    //          << "  bottom: " << Bottom() << "\n";
  }
}

void WellLegalizer::WellPlace(Block &block) {
  /****
   * 1. if there is no blocks on the left hand side of this block, switch the type to plugged
   * 2. if the current location is legal, use that space
   * 3.   else find a new location
   * ****/
  int start_row = int(block.LLY()-Bottom());
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
  }
  int lx = int(block.LLX());
  int ly = int(block.LLY());
  int width = block.Width();
  int height = block.Height();
  bool is_cur_loc_legal = IsSpaceAvail(lx, ly, width, height);
  if (!is_cur_loc_legal) {
    FindLocation(block, lx, ly);
    block.SetLoc(lx, ly);
  }
  UseSpace(lx, ly, width, height);
}

void WellLegalizer::StartPlacement() {
  InitWellLegalizer();
  std::cout << "Number of rows: " << all_rows_.size() << "\n";
  std::cout << "Number of blocks: " << index_loc_list_.size() << "\n";

  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "Start Well Legalization\n";
  }
  std::vector<Block> &block_list = *BlockList();
  for (size_t i=0; i<index_loc_list_.size(); ++i) {
    index_loc_list_[i].num = i;
    index_loc_list_[i].x = block_list[i].LLX();
    index_loc_list_[i].y = block_list[i].LLY();
  }
  std::sort(index_loc_list_.begin(), index_loc_list_.end());
  for (auto &pair: index_loc_list_) {
    auto &block = block_list[pair.num];
    if (block.IsFixed()) continue;
    WellPlace(block);
    //std::cout << block.LLX() << "  " << block.LLY() << "\n";
  }

  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "\033[0;36m"
              << "Well Legalization complete!\n"
              << "\033[0m";
  }

}
