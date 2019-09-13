//
// Created by yihan on 9/11/2019.
//

#include "MDPlacer.h"

void MDPlacer::CreateBlkAuxList() {
  std::vector<Block> &block_list = *BlockList();
  blk_aux_list.reserve(block_list.size());
  for (auto &&block: block_list) {
    blk_aux_list.emplace_back(&block);
  }
}

void MDPlacer::UpdateForce() {

}

void MDPlacer::UpdateVelocity() {

}

void MDPlacer::UpdateLoc() {

}

void MDPlacer::StartPlacement() {

}
