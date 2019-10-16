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
#include <algorithm>

MDPlacer::MDPlacer(): learning_rate_(0.1), momentum_term_(0.9), max_iteration_num_(100), bin_width_(0), bin_height_(0), bin_cnt_x_(0), bin_cnt_y_(0) {}

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
  bin_width_ = GetCircuit()->MinWidth();
  bin_height_ = GetCircuit()->MinHeight();
  bin_cnt_x_ = std::ceil((double)(Right() - Left()) / bin_width_);
  bin_cnt_y_ = std::ceil((double)(Top() - Bottom()) / bin_height_);
  
  //std::cout << "bin_width: " << bin_width_ << "\n";
  //std::cout << "bin_height: " << bin_height_ << "\n";
  //std::cout << "bin_cnt_x: " << bin_cnt_x_ << "\n";
  //std::cout << "bin_cnt_y: " << bin_cnt_y_ << "\n";
  
  std::vector<Bin> tmp_bin_column(bin_cnt_y_);
  bin_matrix.reserve(bin_cnt_x_);
  for (int i = 0; i < bin_cnt_x_; i++) {
    bin_matrix.push_back(tmp_bin_column);
  }

  /* for each grid bin, we need to initialize the attributes, including index, boundaries, area, and potential available white space
   * the adjacent bin list is created for the convenience of overfilled bin clustering */
  for (int i=0; i<(int)(bin_matrix.size()); i++) {
    for (int j = 0; j < (int)(bin_matrix[i].size()); j++) {
      bin_matrix[i][j].SetBottom(Bottom() + j * bin_height_);
      bin_matrix[i][j].SetTop(Bottom() + (j+1) * bin_height_);
      bin_matrix[i][j].SetLeft(Left() + i * bin_width_);
      bin_matrix[i][j].SetRight(Left() + (i+1) * bin_width_);
    }
  }

  for (auto &&bin_column: bin_matrix) {
    bin_column[bin_cnt_y_ - 1].SetTop(Top());
  }
  for (auto &&grid_bin: bin_matrix[bin_cnt_x_ - 1]) {
    grid_bin.SetRight(Right());
  }
}

BinIndex MDPlacer::LowLocToIndex(double llx, double lly) {
  BinIndex result;
  result.x = std::floor((llx - Left())/BinWidth());
  result.x = std::max(result.x, 0);
  result.y = std::floor((lly - Bottom())/BinHeight());
  result.x = std::max(result.x, 0);
  return result;
}

BinIndex MDPlacer::HighLocToIndex(double urx, double ury) {
  BinIndex result;
  result.x = std::ceil((urx - Left())/BinWidth());
  result.x = std::min(result.x, BinCountX());
  result.y = std::ceil((ury - Bottom())/BinHeight());
  result.x = std::min(result.x, BinCountY());
  return result;
}

void MDPlacer::UpdateIndex(BlockAux &blk_aux) {

}

void MDPlacer::UpdateBin(Block &blk) {
  std::vector<BinIndex> index_list;
  auto blk_aux = (MDBlkAux *)blk.Aux();
  BinIndex new_ll = LowLocToIndex(blk.LLX(), blk.LLY());
  BinIndex new_ur = HighLocToIndex(blk.URX(), blk.URY());
  BinIndex old_ll = blk_aux->ll_index;
  BinIndex old_ur = blk_aux->ur_index;

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
  auto blk_aux = (MDBlkAux *)blk.Aux();
  for (auto &&block: block_list) {
    if (&block == &blk) continue;
    force = blk_aux->GetForce(&block);
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
  Value2D velocity = blk_aux->Velocity() * momentum_term_;
  velocity += velocity_incre;
  blk_aux->SetVelocity(velocity);

  blk.IncreX(velocity.x, Right(), Left());
  blk.IncreY(velocity.y, Top(), Bottom());
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
      UpdateBin(block);
    }
  }
  if (globalVerboseLevel >= LOG_INFO) {
    std::cout << "MDPlacer Complete\n";
  }
  ReportHPWL(LOG_INFO);
}
