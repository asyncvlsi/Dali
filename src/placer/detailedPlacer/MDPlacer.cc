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
 *  3. We will use gradient descent with momentum to optimize (1-r)HWPL + rOverLap.
 * ****/

#include "MDPlacer.h"

void MDPlacer::CreateBlkAuxList() {
  /****
   * 1. Create blk_aux_list and link auxiliary information with blocks;
   * 2. For each blk, find the nets it connects to, and put this list to its associated blk_aux.
   * ****/
  std::vector<Block> &block_list = *BlockList();
  blk_aux_list.reserve(block_list.size());
  for (auto &&block: block_list) {
    blk_aux_list.emplace_back(&block);
  }

  std::vector<Net> &net_list = *NetList();
  for (auto &&net: net_list) {
    for (auto &&blk_pin: net.blk_pin_list) {
      blk_aux_list[blk_pin.BlockNum()].net_list.push_back(&net);
    }
  }

  /*for (auto &&blk_aux: blk_aux_list) {
    blk_aux.ReportNet();
  }*/
}

void MDPlacer::InitGridBin() {

}

void MDPlacer::UpdateVelocityLoc(Block &blk) {
  /****
   * 1. Calculate the total force, use the total force to calculate the velocity_incre, delta_v = f * learning_rate
   * 2. Update the velocity, v_new = alpha * v_old + delta_v;
   * 3. Update the location of the block.
   * ****/
  Value2D tot_force(0, 0);
  Value2D force(0, 0);
  std::vector<Block> &block_list = *BlockList();
  auto aux_info = (MDBlkAux *)blk.Aux();
  for (auto &&block: block_list) {
    if (&block == &blk) continue;
    force = aux_info->GetForce(&block);
    tot_force.Incre(force);
  }
  tot_force *= 1.0/blk.Area();

  int blk_num = blk.Num();
  force.Init();
  for (auto &&net_ptr: blk_aux_list[blk_num].net_list) {
    net_ptr->UpdateMaxMin();
    if (blk_num == net_ptr->MaxBlkPinNumX()) {
      force.x -= 1;
    }
    if (blk_num == net_ptr->MinBlkPinNumX()) {
      force.x += 1;
    }
    if (blk_num == net_ptr->MaxBlkPinNumY()) {
      force.y -= 1;
    }
    if (blk_num == net_ptr->MinBlkPinNumY()) {
      force.y += 1;
    }
  }
  tot_force.Incre(force);

  Value2D velocity_incre = tot_force * learning_rate_;
  Value2D velocity = aux_info->Velocity() * momentum_term_;
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
  InitGridBin();
  for (int i=0; i<max_iteration_num_; ++i) {
    ReportHPWL(LOG_INFO);
    for (auto &&block: block_list) {
      UpdateVelocityLoc(block);
    }
  }
  if (globalVerboseLevel >= LOG_INFO) {
    std::cout << "MDPlacer Complete\n";
  }
  ReportHPWL(LOG_INFO);
}
