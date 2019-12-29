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
  std::cout << n_max_plug_dist_ << "  " << p_max_plug_dist_ << "\n";

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

void WellLegalizer::UseSpace(Block &block) {

}

void WellLegalizer::FindLocation(Block &block) {

}

void WellLegalizer::WellPlace(Block &block) {
  /****
   * 1. if there is no blocks on the left hand side of this block, switch the type to plugged
   * ****/
  int start_row = int(block.LLY()-Bottom());
  int end_row = int(block.URY()-Bottom());
  bool no_left_blocks = true;
  for (int i=start_row; i<=end_row; ++i) {
    if (all_rows_[i].dist < n_max_plug_dist_) {
      no_left_blocks = false;
      break;
    }
  }
  if (no_left_blocks) {
    SwitchToPlugType(block);
  } else {
    FindLocation(block);
  }
  UseSpace(block);
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
  }

  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "\033[0;36m"
              << "Well Legalization complete!\n"
              << "\033[0m";
  }

}
