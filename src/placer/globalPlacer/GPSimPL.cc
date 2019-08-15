//
// Created by yihan on 8/4/2019.
//

#include "GPSimPL.h"
#include <cmath>
#include <random>

GPSimPL::GPSimPL(): Placer() {}

GPSimPL::GPSimPL(double aspectRatio, double fillingRate): Placer(aspectRatio, fillingRate) {}

int GPSimPL::TotBlockNum() {
  return GetCircuit()->TotBlockNum();
}

double GPSimPL::WidthEpsilon() {
  return circuit_->AveWidth()/100.0;
}

double GPSimPL::HeightEpsilon() {
  return circuit_->AveHeight()/100.0;
}

void GPSimPL::InitCGFlags() {
  HPWLX_new = 0;
  HPWLY_new = 0;
  HPWLX_old = 1e30;
  HPWLY_old = 1e30;
  HPWLx_converge = false;
  HPWLy_converge = false;
}

void GPSimPL::InterLockAuxInfo() {
  auto &&block_list = *BlockList();
  int size = block_list.size();
  aux_list.reserve(size);
  for (int i=0; i<size; i++) {
    aux_list.emplace_back(&(block_list[i]));
  }

  auto &&net_list = *NetList();
  for (auto &&net: net_list) {
    for (auto &&blk_pin: net.blk_pin_list) {
      auto block_aux = (SimPLBlockAux *)(blk_pin.GetBlock()->Aux());
      block_aux->InsertNet(&net);
    }
  }
  //ReportAuxInfo();
}

void GPSimPL::ReportAuxInfo() {
  auto &&block_list = *BlockList();
  for (auto &&block: block_list) {
    auto block_aux = (SimPLBlockAux *)(block.Aux());
    block_aux->ReportAux();
  }
}

void GPSimPL::BlockLocInit() {
  int length_x = Right() - Left();
  int length_y = Top() - Bottom();
  std::default_random_engine generator{0};
  std::uniform_real_distribution<double> distribution(0, 1);
  std::vector<Block> &block_list = *BlockList();
  for (auto &&block: block_list) {
    if (block.IsMovable()) {
      block.SetCenterX(Left() + length_x * distribution(generator));
      block.SetCenterY(Bottom() + length_y * distribution(generator));
    }
  }
}

void GPSimPL::CGInit() {
  // this init function allocate memory to Ax and Ay
  // the size of memory allocated for each row is the maximum memory which might be used
  Ax.reserve(TotBlockNum());
  Ay.reserve(TotBlockNum());
  bx.reserve(TotBlockNum());
  by.reserve(TotBlockNum());
  ax.reserve(TotBlockNum());
  ap.reserve(TotBlockNum());
  z.reserve(TotBlockNum());
  p.reserve(TotBlockNum());
  JP.reserve(TotBlockNum());

  for (int i=0; i < TotBlockNum(); i++) {
    // initialize bx, by
    bx.push_back(0);
    by.push_back(0);
    ax.push_back(0);
    ap.push_back(0);
    z.push_back(0);
    p.push_back(0);
    JP.push_back(0);
  }

  std::vector<WeightTuple> tmp_row;
  for (int i=0; i < TotBlockNum(); i++) {
    Ax.push_back(tmp_row);
    Ay.push_back(tmp_row);
  }

  // initialize diagonal elements
  std::vector<Block> &block_list = *BlockList();
  WeightTuple tmp_weight_tuple;
  tmp_weight_tuple.weight = 0;
  for (int i=0; i < TotBlockNum(); i++) {
    tmp_weight_tuple.pin = i;
    Ax[i].push_back(tmp_weight_tuple);
    Ay[i].push_back(tmp_weight_tuple);
    if (!block_list[i].IsMovable()) {
      Ax[i][0].weight = 1;
      bx[i] = block_list[i].LLX();
      Ay[i][0].weight = 1;
      by[i] = block_list[i].LLY();
    }
  }
}

void GPSimPL::BuildProblemCliqueX() {
  // before build a new Matrix, clean the information in existing matrix
  for (int i=0; i< TotBlockNum(); i++) {
    Ax[i][0].weight = 0;
    // make the diagonal elements 0
    bx[i] = 0;
    // make each element of b 0
  }

  WeightTuple tempWT;
  double weightx, invp, temppinlocx0, temppinlocx1, tempdiffoffset;
  int tempnodenum0, tempnodenum1;
  std::vector<Block> &block_list = *BlockList();
  std::vector<Net> &net_list = *NetList();
  for (auto &&net: net_list) {
    //std::cout << i << "\n";
    if (net.P() <= 1) continue;
    invp = net.InvP();
    for (size_t j=0; j<net.blk_pin_list.size(); j++) {
      tempnodenum0 = net.blk_pin_list[j].GetBlock()->Num();
      temppinlocx0 = block_list[tempnodenum0].LLX() + net.blk_pin_list[j].XOffset();
      for (size_t k=j+1; k<net.blk_pin_list.size(); k++) {
        tempnodenum1 = net.blk_pin_list[k].GetBlock()->Num();
        temppinlocx1 = block_list[tempnodenum1].LLX() + net.blk_pin_list[k].XOffset();
        if (tempnodenum0 == tempnodenum1) continue;
        if ((block_list[tempnodenum0].IsMovable() == 0)&&(block_list[tempnodenum1].IsMovable() == 0)) {
          continue;
        }
        weightx = invp/(fabs(temppinlocx0 - temppinlocx1) + WidthEpsilon());
        if ((block_list[tempnodenum0].IsMovable() == 0)&&(block_list[tempnodenum1].IsMovable() == 1)) {
          bx[tempnodenum1] += (temppinlocx0 - net.blk_pin_list[k].XOffset()) * weightx;
          Ax[tempnodenum1][0].weight += weightx;
        }
        else if ((block_list[tempnodenum0].IsMovable() == 1)&&(block_list[tempnodenum1].IsMovable() == 0)) {
          bx[tempnodenum0] += (temppinlocx1 - net.blk_pin_list[j].XOffset()) * weightx;
          Ax[tempnodenum0][0].weight += weightx;
        }
        else {
          //((blockList[tempnodenum0].isterminal() == 0)&&(blockList[tempnodenum1].isterminal() == 0))
          tempWT.pin = tempnodenum1;
          tempWT.weight = -weightx;
          tempWT.pin = tempnodenum0;
          tempWT.weight = -weightx;
          Ax[tempnodenum0][0].weight += weightx;
          Ax[tempnodenum1][0].weight += weightx;
          tempdiffoffset = (net.blk_pin_list[k].XOffset() - net.blk_pin_list[j].XOffset()) * weightx;
          bx[tempnodenum0] += tempdiffoffset;
          bx[tempnodenum1] -= tempdiffoffset;
        }
      }
    }
  }
}

void GPSimPL::BuildProblemCliqueY() {
  // before build a new Matrix, clean the information in existing matrix
  for (int i=0; i< TotBlockNum(); i++) {
    Ay[i][0].weight = 0;
    // make the diagonal elements 0
    by[i] = 0;
    // make each element of b 0
  }

  WeightTuple tempWT;
  double weighty, invp, temppinlocy0, temppinlocy1, tempdiffoffset;
  int tempnodenum0, tempnodenum1;
  std::vector<Block> &block_list = *BlockList();
  std::vector<Net> &net_list = *NetList();
  for (auto &&net: net_list) {
    //std::cout << i << "\n";
    if (net.P() <= 1) continue;
    invp = net.InvP();
    for (size_t j=0; j<net.blk_pin_list.size(); j++) {
      tempnodenum0 = net.blk_pin_list[j].GetBlock()->Num();
      temppinlocy0 = block_list[tempnodenum0].LLY() + net.blk_pin_list[j].YOffset();
      for (size_t k=j+1; k<net.blk_pin_list.size(); k++) {
        tempnodenum1 = net.blk_pin_list[k].GetBlock()->Num();
        temppinlocy1 = block_list[tempnodenum1].LLY() + net.blk_pin_list[k].YOffset();
        if (tempnodenum0 == tempnodenum1) continue;
        if ((block_list[tempnodenum0].IsMovable() == 0)&&(block_list[tempnodenum1].IsMovable() == 0)) {
          continue;
        }
        weighty = invp/((double)fabs(temppinlocy0 - temppinlocy1) + HeightEpsilon());
        if ((block_list[tempnodenum0].IsMovable() == 0)&&(block_list[tempnodenum1].IsMovable() == 1)) {
          by[tempnodenum1] += (temppinlocy0 - net.blk_pin_list[k].YOffset()) * weighty;
          Ay[tempnodenum1][0].weight += weighty;
        }
        else if ((block_list[tempnodenum0].IsMovable() == 1)&&(block_list[tempnodenum1].IsMovable() == 0)) {
          by[tempnodenum0] += (temppinlocy1 - net.blk_pin_list[j].YOffset()) * weighty;
          Ay[tempnodenum0][0].weight += weighty;
        }
        else {
          //((blockList[tempnodenum0].isterminal() == 0)&&(blockList[tempnodenum1].isterminal() == 0))
          tempWT.pin = tempnodenum1;
          tempWT.weight = -weighty;
          tempWT.pin = tempnodenum0;
          tempWT.weight = -weighty;
          Ay[tempnodenum0][0].weight += weighty;
          Ay[tempnodenum1][0].weight += weighty;
          tempdiffoffset = (net.blk_pin_list[k].YOffset() - net.blk_pin_list[j].YOffset()) * weighty;
          by[tempnodenum0] += tempdiffoffset;
          by[tempnodenum1] -= tempdiffoffset;
        }
      }
    }
  }
}

void GPSimPL::UpdateHPWLX() {
  HPWLX_new = HPWLX();
}

void GPSimPL::UpdateMaxMinX() {
  std::vector<Net> &net_list = *NetList();
  for (auto &&net: net_list) {
    net.UpdateMaxMinX();
  }
}

void GPSimPL::UpdateCGFlagsX() {
  UpdateHPWLX();
  if (HPWLX_new == 0) { // this is for 1 degree net, this happens in extremely rare cases
    HPWLx_converge = true;
  } else {
    HPWLx_converge = (std::fabs(1 - HPWLX_new / HPWLX_old) < HPWL_intra_linearSolver_precision);
    HPWLX_old = HPWLX_new;
  }
}

void GPSimPL::BuildProblemB2BX() {
  std::vector<Block> &block_list = *BlockList();
  std::vector<Net> &net_list = *NetList();
  // 1. update the x direction max and min node in each net
  UpdateMaxMinX();
  // 2. before build a new Matrix, "clean" the information in existing matrix, and reserve space for this row
  for (int i=0; i< TotBlockNum(); i++) {
    if (block_list[i].IsMovable()) {
      Ax[i].resize(1);
      Ax[i][0].weight = 0;
      // initialize the diagonal value of movable blocks to 0
      bx[i] = 0;
      // initialize the corresponding b value of this movable blocks to 0
      auto block_aux = (SimPLBlockAux *)(block_list[i].Aux());
      Ax[i].reserve(block_aux->B2BRowSizeX());
      // reserve space for this row
    }
  }
  WeightTuple tmp_weight_tuple;
  double weight_x, inv_p, pin_loc_x0, pin_loc_x1, offset_diff;
  int block_num0, block_num1;
  size_t max_pin_x, min_pin_x;
  for (auto &&net: net_list) {
    if (net.P() <= 1) continue;
    inv_p = net.InvP();
    max_pin_x = net.MaxBlkPinNumX();
    min_pin_x = net.MinBlkPinNumX();
    for (size_t i=0; i<net.blk_pin_list.size(); i++) {
      block_num0 = net.blk_pin_list[i].GetBlock()->Num();
      pin_loc_x0 = block_list[block_num0].LLX() + net.blk_pin_list[i].XOffset();
      for (size_t k=i+1; k<net.blk_pin_list.size(); k++) {
        if ((i!=max_pin_x)&&(i!=min_pin_x)) {
          if ((k!=max_pin_x)&&(k!=min_pin_x)) continue;
          // if neither of these two pins are boundary pins, there is no nets between them
        }
        block_num1 = net.blk_pin_list[k].GetBlock()->Num();
        if (block_num0 == block_num1) continue;
        pin_loc_x1 = block_list[block_num1].LLX() + net.blk_pin_list[k].XOffset();
        weight_x = inv_p/(std::fabs(pin_loc_x0 - pin_loc_x1) + WidthEpsilon());
        if ((block_list[block_num0].IsMovable() == 0)&&(block_list[block_num1].IsMovable() == 0)) {
          continue;
        }
        else if ((block_list[block_num0].IsMovable() == 0)&&(block_list[block_num1].IsMovable() == 1)) {
          bx[block_num1] += (pin_loc_x0 - net.blk_pin_list[k].XOffset()) * weight_x;
          Ax[block_num1][0].weight += weight_x;
        }
        else if ((block_list[block_num0].IsMovable() == 1)&&(block_list[block_num1].IsMovable() == 0)) {
          bx[block_num0] += (pin_loc_x1 - net.blk_pin_list[i].XOffset()) * weight_x;
          Ax[block_num0][0].weight += weight_x;
        }
        else {
          tmp_weight_tuple.pin = block_num1;
          tmp_weight_tuple.weight = -weight_x;
          Ax[block_num0].push_back(tmp_weight_tuple);

          tmp_weight_tuple.pin = block_num0;
          tmp_weight_tuple.weight = -weight_x;
          Ax[block_num1].push_back(tmp_weight_tuple);

          Ax[block_num0][0].weight += weight_x;
          Ax[block_num1][0].weight += weight_x;
          offset_diff = (net.blk_pin_list[k].XOffset() - net.blk_pin_list[i].XOffset()) * weight_x;
          bx[block_num0] += offset_diff;
          bx[block_num1] -= offset_diff;
        }
      }
    }
  }
  for (size_t i=0; i<Ax.size(); i++) { // this is for cells with tiny force applied on them
    if ((Ax[i][0].weight < 1e-10) && (block_list[i].IsMovable())) {
      Ax[i][0].weight = 1;
      bx[i] = block_list[i].LLX();
    }
  }
}

void GPSimPL::UpdateMaxMinCtoCX() {
  HPWLX_new = 0;
  std::vector<Net> &net_list = *NetList();
  for (auto &&net: net_list) {
    HPWLX_new += net.HPWLCtoCX();
  }
  //std::cout << "HPWLX_old: " << HPWLX_old << "\n";
  //std::cout << "HPWLX_new: " << HPWLX_new << "\n";
  //std::cout << 1 - HPWLX_new/HPWLX_old << "\n";
  if (HPWLX_new == 0) { // this is for 1 degree net, this happens in extremely rare cases
    HPWLx_converge = true;
  } else {
    HPWLx_converge = (std::fabs(1 - HPWLX_new / HPWLX_old) < HPWL_intra_linearSolver_precision);
    HPWLX_old = HPWLX_new;
  }
}

void GPSimPL::BuildProblemB2BXNoOffset() {
  // before build a new Matrix, clean the information in existing matrix
  std::vector<Block> &block_list = *BlockList();
  std::vector<Net> &net_list = *NetList();
  // before build a new Matrix, clean the information in existing matrix
  for (int i=0; i< TotBlockNum(); i++) {
    if (block_list[i].IsMovable()) {
      Ax[i][0].weight = 0;
      // make the diagonal elements 0
      bx[i] = 0;
      // make each element of b 0
    } else {
      Ax[i][0].weight = 1;
      bx[i] = block_list[i].LLX();
    }
  }
  WeightTuple tempWT;
  double weightx, invp, temppinlocx0, temppinlocx1;
  int tempnodenum0, tempnodenum1;
  size_t maxpindex_x, minpindex_x;
  for (auto &&net: net_list) {
    if (net.P()<=1) {
      continue;
    }
    invp = net.InvP();
    maxpindex_x = net.MaxPinCtoCX();
    minpindex_x = net.MinPinCtoCX();
    for (size_t i=0; i<net.blk_pin_list.size(); i++) {
      tempnodenum0 = net.blk_pin_list[i].GetBlock()->Num();
      temppinlocx0 = block_list[tempnodenum0].X();
      for (size_t k=i+1; k<net.blk_pin_list.size(); k++) {
        if ((i!=maxpindex_x)&&(i!=minpindex_x)) {
          if ((k!=maxpindex_x)&&(k!=minpindex_x)) continue;
        }
        tempnodenum1 = net.blk_pin_list[k].GetBlock()->Num();
        if (tempnodenum0 == tempnodenum1) continue;
        temppinlocx1 = block_list[tempnodenum1].X();
        weightx = invp/((double)fabs(temppinlocx0 - temppinlocx1) + WidthEpsilon());
        if ((block_list[tempnodenum0].IsMovable() == 0)&&(block_list[tempnodenum1].IsMovable() == 0)) {
          continue;
        }
        else if ((block_list[tempnodenum0].IsMovable() == 0)&&(block_list[tempnodenum1].IsMovable() == 1)) {
          bx[tempnodenum1] += temppinlocx0 * weightx;
          Ax[tempnodenum1][0].weight += weightx;
        }
        else if ((block_list[tempnodenum0].IsMovable() == 1)&&(block_list[tempnodenum1].IsMovable() == 0)) {
          bx[tempnodenum0] += temppinlocx1 * weightx;
          Ax[tempnodenum0][0].weight += weightx;
        }
        else {
          //((blockList[tempnodenum0].isterminal() == 0)&&(blockList[tempnodenum1].isterminal() == 0))
          tempWT.pin = tempnodenum1;
          tempWT.weight = -weightx;
          tempWT.pin = tempnodenum0;
          tempWT.weight = -weightx;
          Ax[tempnodenum0][0].weight += weightx;
          Ax[tempnodenum1][0].weight += weightx;
        }
      }
    }
  }
  for (size_t i=0; i<Ax.size(); i++) { // this is for cells with tiny force applied on them
    if ((Ax[i][0].weight < 1e-10) && (block_list[i].IsMovable())) {
      Ax[i][0].weight = 1;
      bx[i] = block_list[i].X();
    }
  }
}

void GPSimPL::UpdateHPWLY() {
  // update the y direction max and min node in each net
  HPWLY_new = HPWLY();
}

void GPSimPL::UpdateMaxMinY() {
  std::vector<Net> &net_list = *NetList();
  for (auto &&net: net_list) {
    net.UpdateMaxMinY();
  }
}

void GPSimPL::UpdateCGFlagsY() {
  UpdateHPWLY();
  if (HPWLY_new == 0) { // this is for 1 degree net, this happens in extremely rare cases
    HPWLy_converge = true;
  } else {
    HPWLy_converge = (std::fabs(1 - HPWLY_new / HPWLY_old) < HPWL_intra_linearSolver_precision);
    HPWLY_old = HPWLY_new;
  }
}

void GPSimPL::BuildProblemB2BY() {
  std::vector<Block> &block_list = *BlockList();
  std::vector<Net> &net_list = *NetList();
  // 1. update the x direction max and min node in each net
  UpdateMaxMinY();
  // 2. before build a new Matrix, "clean" the information in existing matrix, and reserve space for this row
  for (int i=0; i< TotBlockNum(); i++) {
    if (block_list[i].IsMovable()) {
      Ay[i].resize(1);
      Ay[i][0].weight = 0;
      // initialize the diagonal value of movable blocks to 0
      by[i] = 0;
      // initialize the corresponding b value of this movable blocks to 0
      auto block_aux = (SimPLBlockAux *)(block_list[i].Aux());
      Ay[i].reserve(block_aux->B2BRowSizeY());
      // reserve space for this row
    }
  }
  WeightTuple tmp_weight_tuple;
  double weighty, inv_p, pin_loc_y0, pin_loc_y1, offset_diff;
  int block_num0, block_num1;
  size_t max_pin_y, min_pin_y;
  for (auto &&net: net_list) {
    if (net.P()<=1) {
      continue;
    }
    inv_p = net.InvP();
    net.UpdateMaxMinY();
    max_pin_y = net.MaxBlkPinNumY();
    min_pin_y = net.MinBlkPinNumY();
    for (size_t i=0; i<net.blk_pin_list.size(); i++) {
      block_num0 = net.blk_pin_list[i].GetBlock()->Num();
      pin_loc_y0 = block_list[block_num0].LLY() + net.blk_pin_list[i].YOffset();
      for (size_t k=i+1; k<net.blk_pin_list.size(); k++) {
        if ((i!=max_pin_y)&&(i!=min_pin_y)) {
          if ((k!=max_pin_y)&&(k!=min_pin_y)) continue;
        }
        block_num1 = net.blk_pin_list[k].GetBlock()->Num();
        if (block_num0 == block_num1) continue;
        pin_loc_y1 = block_list[block_num1].LLY() + net.blk_pin_list[i].YOffset();
        weighty = inv_p/((double)fabs(pin_loc_y0 - pin_loc_y1) + HeightEpsilon());
        if ((block_list[block_num0].IsMovable() == 0)&&(block_list[block_num1].IsMovable() == 0)) {
          continue;
        }
        else if ((block_list[block_num0].IsMovable() == 0)&&(block_list[block_num1].IsMovable() == 1)) {
          by[block_num1] += (pin_loc_y0 - net.blk_pin_list[k].YOffset()) * weighty;
          Ay[block_num1][0].weight += weighty;
        }
        else if ((block_list[block_num0].IsMovable() == 1)&&(block_list[block_num1].IsMovable() == 0)) {
          by[block_num0] += (pin_loc_y1 - net.blk_pin_list[i].YOffset()) * weighty;
          Ay[block_num0][0].weight += weighty;
        }
        else {
          tmp_weight_tuple.pin = block_num1;
          tmp_weight_tuple.weight = -weighty;
          Ay[block_num0].push_back(tmp_weight_tuple);

          tmp_weight_tuple.pin = block_num0;
          tmp_weight_tuple.weight = -weighty;
          Ay[block_num1].push_back(tmp_weight_tuple);

          Ay[block_num0][0].weight += weighty;
          Ay[block_num1][0].weight += weighty;
          offset_diff = (net.blk_pin_list[k].YOffset() - net.blk_pin_list[i].YOffset()) * weighty;
          by[block_num0] += offset_diff;
          by[block_num1] -= offset_diff;
        }
      }
    }
  }
  for (size_t i=0; i<Ay.size(); i++) { // // this is for cells with tiny force applied on them
    if ((Ay[i][0].weight < 1e-10) && (block_list[i].IsMovable())) {
      Ay[i][0].weight = 1;
      by[i] = block_list[i].LLY();
    }
  }
}

void GPSimPL::UpdateMaxMinCtoCY() {
  // update the y direction max and min node in each net
  HPWLY_new = 0;
  std::vector<Net> &net_list = *NetList();
  for (auto &&net: net_list) {
    HPWLY_new += net.HPWLCtoCY();
  }
  //std::cout << "HPWLY_old: " << HPWLY_old << "\n";
  //std::cout << "HPWLY_new: " << HPWLY_new << "\n";
  //std::cout << 1 - HPWLY_new/HPWLY_old << "\n";
  if (HPWLY_new == 0) { // this is for 1 degree net, this happens in extremely rare cases
    HPWLy_converge = true;
  } else {
    HPWLy_converge = (std::fabs(1 - HPWLY_new / HPWLY_old) < HPWL_intra_linearSolver_precision);
    HPWLY_old = HPWLY_new;
  }
}

void GPSimPL::BuildProblemB2BYNoOffset() {
  // before build a new Matrix, clean the information in existing matrix
  std::vector<Block> &block_list = *BlockList();
  std::vector<Net> &net_list = *NetList();
  // before build a new Matrix, clean the information in existing matrix
  for (int i=0; i< TotBlockNum(); i++) {
    if (block_list[i].IsMovable()) {
      Ay[i][0].weight = 0;
      // make the diagonal elements 0
      // mark the length each row of matrix 0, although some data might still exists there
      by[i] = 0;
      // make each element of b 0
    } else {
      Ay[i][0].weight = 1;
      by[i] = block_list[i].Y();
    }
  }
  WeightTuple tempWT;
  double weighty, invp, temppinlocy0, temppinlocy1;
  int tempnodenum0, tempnodenum1;
  size_t maxpindex_y, minpindex_y;
  for (auto &&net: net_list) {
    if (net.P()<=1) {
      continue;
    }
    invp = net.InvP();
    maxpindex_y = net.MaxPinCtoCY();
    minpindex_y = net.MinPinCtoCY();
    for (size_t i=0; i<net.blk_pin_list.size(); i++) {
      tempnodenum0 = net.blk_pin_list[i].GetBlock()->Num();
      temppinlocy0 = block_list[tempnodenum0].Y();
      for (size_t k=i+1; k<net.blk_pin_list.size(); k++) {
        if ((i!=maxpindex_y)&&(i!=minpindex_y)) {
          if ((k!=maxpindex_y)&&(k!=minpindex_y)) continue;
        }
        tempnodenum1 = net.blk_pin_list[k].GetBlock()->Num();
        if (tempnodenum0 == tempnodenum1) continue;
        temppinlocy1 = block_list[tempnodenum1].Y();
        weighty = invp/((double)fabs(temppinlocy0 - temppinlocy1) + HeightEpsilon());
        if ((block_list[tempnodenum0].IsMovable() == 0)&&(block_list[tempnodenum1].IsMovable() == 0)) {
          continue;
        }
        else if ((block_list[tempnodenum0].IsMovable() == 0)&&(block_list[tempnodenum1].IsMovable() == 1)) {
          by[tempnodenum1] += temppinlocy0 * weighty;
          Ay[tempnodenum1][0].weight += weighty;
        }
        else if ((block_list[tempnodenum0].IsMovable() == 1)&&(block_list[tempnodenum1].IsMovable() == 0)) {
          by[tempnodenum0] += temppinlocy1 * weighty;
          Ay[tempnodenum0][0].weight += weighty;
        }
        else {
          //((blockList[tempnodenum0].isterminal() == 0)&&(blockList[tempnodenum1].isterminal() == 0))
          tempWT.pin = tempnodenum1;
          tempWT.weight = -weighty;
          tempWT.pin = tempnodenum0;
          tempWT.weight = -weighty;
          Ay[tempnodenum0][0].weight += weighty;
          Ay[tempnodenum1][0].weight += weighty;
        }
      }
    }
  }
  for (size_t i=0; i<Ay.size(); i++) { // // this is for cells with tiny force applied on them
    if ((Ay[i][0].weight < 1e-10) && (block_list[i].IsMovable())) {
      Ay[i][0].weight = 1;
      by[i] = block_list[i].Y();
    }
  }
}

void GPSimPL::build_problem_b2b_x(SpMat &eigen_A, Eigen::VectorXd &b) {
  std::vector<Block> &block_list = *BlockList();
  std::vector<Net> &net_list = *NetList();
  std::vector<T> coefficients;
  coefficients.reserve(10000000);
  for (int i=0; i<b.size(); i++) {
    b[i] = 0;
  }
  double weightX, invP, tmpPinLocX0, tmpPinLocX1, tmpDiffOffset;
  size_t tmpNodeNum0, tmpNodeNum1, maxPinIndex_x, minPinIndex_x;
  for (auto &&net: net_list) {
    if (net.P()==1) continue;
    invP = net.InvP();
    maxPinIndex_x = net.MaxBlkPinNumX();
    minPinIndex_x = net.MinBlkPinNumX();
    for (size_t i=0; i<net.blk_pin_list.size(); i++) {
      tmpNodeNum0 = net.blk_pin_list[i].GetBlock()->Num();
      tmpPinLocX0 = block_list[tmpNodeNum0].LLX();
      for (size_t j=i+1; j<net.blk_pin_list.size(); j++) {
        if ((i!=maxPinIndex_x)&&(i!=minPinIndex_x)) {
          if ((j!=maxPinIndex_x)&&(j!=minPinIndex_x)) continue;
        }
        tmpNodeNum1 = net.blk_pin_list[j].GetBlock()->Num();
        if (tmpNodeNum0 == tmpNodeNum1) continue;
        tmpPinLocX1 = block_list[tmpNodeNum1].LLX();
        weightX = invP/(fabs(tmpPinLocX0 - tmpPinLocX1) + WidthEpsilon());
        if (!block_list[tmpNodeNum0].IsMovable() && block_list[tmpNodeNum1].IsMovable()) {
          b[tmpNodeNum1] += (tmpPinLocX0 - net.blk_pin_list[j].XOffset()) * weightX;
          coefficients.emplace_back(T(tmpNodeNum1,tmpNodeNum1,weightX));
        } else if (block_list[tmpNodeNum0].IsMovable() && !block_list[tmpNodeNum1].IsMovable()) {
          b[tmpNodeNum0] += (tmpPinLocX1 - net.blk_pin_list[i].XOffset()) * weightX;
          coefficients.emplace_back(T(tmpNodeNum0,tmpNodeNum0,weightX));
        } else if (block_list[tmpNodeNum0].IsMovable() && block_list[tmpNodeNum1].IsMovable()){
          coefficients.emplace_back(T(tmpNodeNum0,tmpNodeNum0,weightX));
          coefficients.emplace_back(T(tmpNodeNum1,tmpNodeNum1,weightX));
          coefficients.emplace_back(T(tmpNodeNum0,tmpNodeNum1,-weightX));
          coefficients.emplace_back(T(tmpNodeNum1,tmpNodeNum0,-weightX));
          tmpDiffOffset = (net.blk_pin_list[j].XOffset() - net.blk_pin_list[i].XOffset()) * weightX;
          b[tmpNodeNum0] += tmpDiffOffset;
          b[tmpNodeNum1] -= tmpDiffOffset;
        } else {
          continue;
        }
      }
    }
  }
  for (size_t i=0; i<block_list.size(); ++i) {
    if (!block_list[i].IsMovable()) {
      coefficients.emplace_back(T(i,i,1));
      b[i] = block_list[i].LLX();
    }
  }
  eigen_A.setFromTriplets(coefficients.begin(), coefficients.end());
}

void GPSimPL::build_problem_b2b_y(SpMat &eigen_A, Eigen::VectorXd &b) {
  std::vector<Block> &block_list = *BlockList();
  std::vector<Net> &net_list = *NetList();
  std::vector<T> coefficients;
  coefficients.reserve(10000000);
  for (int i=0; i<b.size(); i++) {
    b[i] = 0;
  }
  double weightY, invP, tmpPinLocY0, tmpPinLocY1, tmpDiffOffset;
  size_t tmpNodeNum0, tmpNodeNum1, maxPinIndex_y, minPinIndex_y;
  for (auto &&net: net_list) {
    if (net.P()==1) continue;
    invP = net.InvP();
    maxPinIndex_y = net.MaxBlkPinNumY();
    minPinIndex_y = net.MinBlkPinNumY();
    for (size_t i=0; i<net.blk_pin_list.size(); i++) {
      tmpNodeNum0 = net.blk_pin_list[i].GetBlock()->Num();
      tmpPinLocY0 = block_list[tmpNodeNum0].LLY();
      for (size_t j=i+1; j<net.blk_pin_list.size(); j++) {
        if ((i!=maxPinIndex_y)&&(i!=minPinIndex_y)) {
          if ((j!=maxPinIndex_y)&&(j!=minPinIndex_y)) continue;
        }
        tmpNodeNum1 = net.blk_pin_list[j].GetBlock()->Num();
        if (tmpNodeNum0 == tmpNodeNum1) continue;
        tmpPinLocY1 = block_list[tmpNodeNum1].LLY();
        weightY = invP/((double)fabs(tmpPinLocY0 - tmpPinLocY1) + HeightEpsilon());
        if (!block_list[tmpNodeNum0].IsMovable() && block_list[tmpNodeNum1].IsMovable()) {
          b[tmpNodeNum1] += (tmpPinLocY0 - net.blk_pin_list[j].YOffset()) * weightY;
          coefficients.emplace_back(T(tmpNodeNum1,tmpNodeNum1,weightY));
        } else if (block_list[tmpNodeNum0].IsMovable() && !block_list[tmpNodeNum1].IsMovable()) {
          b[tmpNodeNum0] += (tmpPinLocY1 - net.blk_pin_list[i].YOffset()) * weightY;
          coefficients.emplace_back(T(tmpNodeNum0,tmpNodeNum0,weightY));
        } else if (block_list[tmpNodeNum0].IsMovable() && block_list[tmpNodeNum1].IsMovable()) {
          coefficients.emplace_back(T(tmpNodeNum0,tmpNodeNum0,weightY));
          coefficients.emplace_back(T(tmpNodeNum1,tmpNodeNum1,weightY));
          coefficients.emplace_back(T(tmpNodeNum0,tmpNodeNum1,-weightY));
          coefficients.emplace_back(T(tmpNodeNum1,tmpNodeNum0,-weightY));
          tmpDiffOffset = (net.blk_pin_list[j].YOffset() - net.blk_pin_list[i].YOffset()) * weightY;
          b[tmpNodeNum0] += tmpDiffOffset;
          b[tmpNodeNum1] -= tmpDiffOffset;
        } else {
          continue;
        }
      }
    }
  }
  for (size_t i=0; i<block_list.size(); ++i) {
    if (!block_list[i].IsMovable()) {
      coefficients.emplace_back(T(i,i,1));
      b[i] = block_list[i].LLY();
    }
  }
  eigen_A.setFromTriplets(coefficients.begin(), coefficients.end());
}

void GPSimPL::eigen_cg_solver() {
  std::cout << "Total number of movable cells: " << GetCircuit()->TotMovableBlockNum() << "\n";
  std::cout << "Total number of cells: " << TotBlockNum() << "\n";
  std::vector<Block> &block_list = *BlockList();
  int cellNum = TotBlockNum();

  Eigen::ConjugateGradient <SpMat> cgx;
  cgx.setMaxIterations(cg_iteration_max_num);
  cgx.setTolerance(cg_precision);
  HPWLx_converge = false;
  HPWLX_old = 1e30;
  Eigen::VectorXd x(cellNum), eigen_bx(cellNum);
  SpMat eigen_Ax(cellNum, cellNum);
  for (size_t i=0; i<block_list.size(); ++i) {
    x[i] = block_list[i].LLX();
  }

  UpdateMaxMinX();
  for (int i=0; i<50; ++i) {
    build_problem_b2b_x(eigen_Ax, eigen_bx);
    cgx.compute(eigen_Ax);
    x = cgx.solveWithGuess(eigen_bx, x);
    //std::cout << "Here is the vector x:\n" << x << std::endl;
    std::cout << "\t#iterations:     " << cgx.iterations() << std::endl;
    std::cout << "\testimated error: " << cgx.error() << std::endl;
    for (int num=0; num<x.size(); ++num) {
      if (block_list[num].IsMovable()) {
        block_list[num].SetLLX(x[num]);
      }
    }
    UpdateCGFlagsX();
    if (HPWLx_converge) {
      std::cout << "iterations x:     " << i << "\n";
      break;
    }
  }

  Eigen::ConjugateGradient <SpMat> cgy;
  cgy.setMaxIterations(cg_iteration_max_num);
  cgy.setTolerance(cg_precision);
  // Assembly:
  HPWLy_converge = false;
  HPWLY_old = 1e30;
  Eigen::VectorXd y(cellNum), eigen_by(cellNum); // the solution and the right hand side-vector resulting from the constraints
  SpMat eigen_Ay(cellNum, cellNum); // sparse matrix
  for (size_t i=0; i<block_list.size(); ++i) {
    y[i] = block_list[i].LLY();
  }

  UpdateMaxMinY();
  for (int i=0; i<50; ++i) {
    build_problem_b2b_y(eigen_Ay, eigen_by); // fill A and b
    // Solving:
    cgy.compute(eigen_Ay);
    y = cgy.solveWithGuess(eigen_by, y);
    std::cout << "\t#iterations:     " << cgy.iterations() << std::endl;
    std::cout << "\testimated error: " << cgy.error() << std::endl;
    for (int num=0; num<y.size(); ++num) {
      if (block_list[num].IsMovable()) {
        block_list[num].SetLLX(y[num]);
      }
    }
    UpdateCGFlagsY();
    if (HPWLy_converge) {
      std::cout << "iterations y:     " << i << "\n";
      break;
    }
  }
}

void GPSimPL::CGSolver(std::string const &dimension, std::vector<std::vector<WeightTuple> > &A, std::vector<double> &b) {
  double epsilon = 1e-10, alpha, beta, rsold, rsnew, pAp, solution_distance;
  std::vector<Block> &block_list = *BlockList();
  for (int i=0; i< TotBlockNum(); i++) {
    // initialize the Jacobi pre-conditioner
    if (A[i][0].weight > epsilon) {
      // JP[i] is the reverse of diagonal value
      JP[i] = 1/A[i][0].weight;
    }
    else {
      // if the diagonal is too small, set JP[i] to 1 to prevent diverge
      JP[i] = 1;
      if (dimension == "x") {
        bx[i] = block_list[i].LLX();
      } else {
        by[i] = block_list[i].LLY();
      }
    }
  }
  for (int i=0; i< TotBlockNum(); i++) {
    // calculate Ax
    ax[i] = 0;
    if (dimension=="x") {
      for (size_t j=0; j<=A[i].size(); j++) {
        ax[i] += A[i][j].weight * block_list[A[i][j].pin].X();
      }
    }
    else {
      for (size_t j=0; j<=A[i].size(); j++) {
        ax[i] += A[i][j].weight * block_list[A[i][j].pin].Y();
      }
    }
  }
  rsold = 0;
  for (int i=0; i< TotBlockNum(); i++) {
    // calculate z and p and rsold
    z[i] = JP[i]*(b[i] - ax[i]);
    p[i] = z[i];
    rsold += z[i]*z[i]/JP[i];
  }

  for (int t=0; t<100; t++) {
    pAp = 0;
    for (int i=0; i< TotBlockNum(); i++) {
      ap[i] = 0;
      for (size_t j=0; j<=A[i].size(); j++) {
        ap[i] += A[i][j].weight * p[A[i][j].pin];
      }
      pAp += p[i]*ap[i];
    }
    alpha = rsold/pAp;
    rsnew = 0;
    if (dimension=="x") {
      for (int i=0; i< TotBlockNum(); i++) {
        if (block_list[i].IsMovable()) {
          block_list[i].IncreX(alpha * p[i]);
        }
      }
    }
    else {
      for (int i=0; i< TotBlockNum(); i++) {
        if (block_list[i].IsMovable()) {
          block_list[i].IncreY(alpha * p[i]);
        }
      }
    }
    solution_distance = 0;
    for (int i=0; i< TotBlockNum(); i++) {
      z[i] -= JP[i]*alpha * ap[i];
      solution_distance += z[i]*z[i]/JP[i]/JP[i];
      rsnew += z[i]*z[i]/JP[i];
    }
    //std::cout << "solution_distance: " << solution_distance/TotBlockNum() << "\n";
    if (solution_distance/ TotBlockNum() < cg_precision) break;
    beta = rsnew/rsold;
    for (int i=0; i< TotBlockNum(); i++) {
      p[i] = z[i] + beta*p[i];
    }
    rsold = rsnew;
  }

  /* if the cell is out of boundary, just shift it back to the placement region */
  if (dimension == "x") {
    for (int i=0; i< TotBlockNum(); i++) {
      if (block_list[i].IsMovable()) {
        if (block_list[i].LLX() < Left()) {
          block_list[i].SetCenterX(Left() + block_list[i].Width() / 2.0 + 1);
          //std::cout << i << "\n";
        } else if (block_list[i].URX() > Right()) {
          block_list[i].SetCenterX(Right() - block_list[i].Width() / 2.0 - 1);
          //std::cout << i << "\n";
        } else {
          continue;
        }
      }
    }
  } else{
    for (int i=0; i< TotBlockNum(); i++) {
      if (block_list[i].IsMovable()) {
        if (block_list[i].LLY() < Bottom()) {
          block_list[i].SetCenterY(Bottom() + block_list[i].Height() / 2.0 + 1);
          //std::cout << i << "\n";
        } else if (block_list[i].URY() > Top()) {
          block_list[i].SetCenterY(Top() - block_list[i].Height() / 2.0 - 1);
          //std::cout << i << "\n";
        } else {
          continue;
        }
      }
    }
  }
}

void GPSimPL::CGSolverX() {
  CGSolver("x", Ax, bx);
}

void GPSimPL::CGSolverY() {
  CGSolver("y", Ay, by);
}

void GPSimPL::CGClose() {
  Ax.clear();
  Ay.clear();
  bx.clear();
  by.clear();
  ax.clear();
  ap.clear();
  z.clear();
  p.clear();
  JP.clear();
}

void GPSimPL::DrawBlockNetList(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open input file " + name_of_file);
  ost << Left() << " " << Bottom() << " " << Right() - Left() << " " << Top() - Bottom() << "\n";
  std::vector<Block> &block_list = *BlockList();
  for (auto &&block: block_list) {
    ost << block.LLX() << " " << block.LLY() << " " << block.Width() << " " << block.Height() << "\n";
  }
  ost.close();
}

void GPSimPL::StartPlacement() {
  BlockLocInit();
  eigen_cg_solver();
  std::cout << "Initial Placement Complete\n";
  ReportHPWL();

  //ReportHPWLCtoC();
}
