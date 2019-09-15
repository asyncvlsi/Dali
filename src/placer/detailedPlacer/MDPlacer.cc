//
// Created by Yihang Yang on 9/11/2019.
//
/****
 * This detailed placer make use of Molecular dynamic legalization technique.
 * The advantage of this technique is that it can be used as a legalizer, while the disadvantage of this technique is that
 * the running time is usually much longer.
 * In this implementation, we choose a different approach compared with traditional molecular dynamic simulation:
 *  1. Every time we only calculate the force, velocity, and update the location for a single block.
 *  2. We will traverse the block_list to make sure all blocks will have an opportunity to move.
 *  3. We will use Conjugate gradient with momentum to optimize (1-r)HWPL + rOverLap.
 * ****/

#include "MDPlacer.h"

void MDPlacer::CreateBlkAuxList() {
  std::vector<Block> &block_list = *BlockList();
  blk_aux_list.reserve(block_list.size());
  for (auto &&block: block_list) {
    blk_aux_list.emplace_back(&block);
  }
}

void MDPlacer::UpdateLocMomentum(Block &blk) {
  /****
   * 1. Calculate the total force, use the total force to calculate the velocity_incre, delta_v = f * learning_rate
   * 2. Update the velocity, v_new = alpha * v_old + delta_v;
   * 3. Update the location of the block.
   * ****/
  Value2D force(0,0);
  std::vector<Block> &block_list = *BlockList();
  auto aux_info = (MDBlkAux *)blk.Aux();
  for (auto &&block: block_list) {
    if (&block == &blk) continue;
    force.Incre(aux_info->GetForce(&block));
  }
  Value2D velocity_incre = force * learning_rate;

  Value2D velocity = aux_info->Velocity() * momentum_term;
  velocity += velocity_incre;
  aux_info->SetVelocity(velocity);

  blk.IncreX(velocity.x);
  blk.IncreY(velocity.y);
}

void MDPlacer::StartPlacement() {
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "Start MDPlacer\n";
  }
  CreateBlkAuxList();
  std::vector<Block> &block_list = *BlockList();
  for (auto &&block: block_list) {
    UpdateLocMomentum(block);
  }
  if (globalVerboseLevel >= LOG_INFO) {
    std::cout << "MDPlacer Complete\n";
  }
}
