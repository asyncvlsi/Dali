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

Value2D MDPlacer::UpdateForce(Block &blk) {
  Value2D force(0,0);
  std::vector<Block> &block_list = *BlockList();
  auto aux_info = (MDBlkAux *)blk.Aux();
  for (auto &&block: block_list) {
    if (&block == &blk) continue;
    force += aux_info->GetForce(&block);
  }

  return force;
}

Value2D MDPlacer::UpdateVelocity(Value2D &force) {
  Value2D velocity(0,0);

  return velocity;
}

Value2D MDPlacer::UpdateLoc(Value2D &velocity) {
  Value2D displacement(0,0);

  return displacement;
}

void MDPlacer::StartPlacement() {
  CreateBlkAuxList();
  std::vector<Block> &block_list = *BlockList();
  Value2D f, v;
  Value2D loc_displacement;
  for (auto &&block: block_list) {
    f = UpdateForce(block);
    v = UpdateVelocity(f);
    loc_displacement = UpdateLoc(v);
    //block.UpdateLoc(v);
  }
}
