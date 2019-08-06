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

void GPSimPL::InitHPWLFlags() {
  HPWLX_new = 0;
  HPWLY_new = 0;
  HPWLX_old = 1e30;
  HPWLY_old = 1e30;
  HPWLx_converge = false;
  HPWLy_converge = false;
}

void GPSimPL::UniformInit() {
  int length_x = Right() - Left();
  int length_y = Top() - Bottom();
  std::default_random_engine generator{0};
  std::uniform_real_distribution<double> distribution(0, 1);
  std::vector<Block> &block_list = *BlockList();
  for (auto &&node: block_list) {
    if (node.IsMovable()) {
      node.SetCenterX(Left() + length_x * distribution(generator));
      node.SetCenterY(Bottom() + length_y * distribution(generator));
    }
  }
}

void GPSimPL::CGInit() {
  // this init function allocate memory to Ax and Ay
  // the size of memory allocated for each row is the maximum memory which might be used
  Ax.clear();
  Ay.clear();
  kx.clear();
  ky.clear();
  bx.clear();
  by.clear();
  bx.reserve(TotBlockNum());
  by.reserve(TotBlockNum());
  kx.reserve(TotBlockNum());
  ky.reserve(TotBlockNum());

  ax.reserve(TotBlockNum());
  ap.reserve(TotBlockNum());
  z.reserve(TotBlockNum());
  p.reserve(TotBlockNum());
  JP.reserve(TotBlockNum());

  for (int i=0; i < TotBlockNum(); i++) {
    // initialize bx, by
    bx.push_back(0);
    by.push_back(0);
    // initialize kx, ky to track the length of Ax[i] and Ay[i]
    kx.push_back(0);
    ky.push_back(0);

    ax.push_back(0);
    ap.push_back(0);
    z.push_back(0);
    p.push_back(0);
    JP.push_back(0);
  }
  std::vector<weightTuple> tempArow;
  for (int i=0; i < TotBlockNum(); i++) {
    Ax.push_back(tempArow);
    Ay.push_back(tempArow);
  }
  weightTuple tempWT;
  tempWT.weight = 0;
  for (int i=0; i < TotBlockNum(); i++) {
    tempWT.pin = i;
    Ax[i].push_back(tempWT);
    Ay[i].push_back(tempWT);
  }

  int tempnodenum0, tempnodenum1;
  std::vector<Block> &block_list = *BlockList();
  std::vector<Net> &net_list = *NetList();
  for (auto &&net: net_list) {
    if (net.P()<=1) continue;
    for (size_t j=0; j<net.blk_pin_list.size(); j++) {
      tempnodenum0 = net.blk_pin_list[j].GetBlock()->Num();
      for (size_t k=j+1; k<net.blk_pin_list.size(); k++) {
        tempnodenum1 = net.blk_pin_list[k].GetBlock()->Num();
        if (tempnodenum0 == tempnodenum1) continue;
        if ((block_list[tempnodenum0].IsMovable())&&(block_list[tempnodenum1].IsMovable())) {
          // when both nodes are movable and are not the same, increase the row length by 1
          Ax[tempnodenum0].push_back(tempWT);
          Ax[tempnodenum1].push_back(tempWT);
          Ay[tempnodenum0].push_back(tempWT);
          Ay[tempnodenum1].push_back(tempWT);
        }
      }
    }
  }
}

void GPSimPL::BuildProblemCliqueX() {
  // before build a new Matrix, clean the information in existing matrix
  for (int i=0; i< TotBlockNum(); i++) {
    Ax[i][0].weight = 0;
    // make the diagonal elements 0
    kx[i] = 0;
    // mark the length each row of matrix 0, although some data might still exists there
    bx[i] = 0;
    // make each element of b 0
  }

  weightTuple tempWT;
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
          kx[tempnodenum0]++;
          Ax[tempnodenum0][kx[tempnodenum0]] = tempWT;

          tempWT.pin = tempnodenum0;
          tempWT.weight = -weightx;
          kx[tempnodenum1]++;
          Ax[tempnodenum1][kx[tempnodenum1]] = tempWT;

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
    ky[i] = 0;
    // mark the length each row of matrix 0, although some data might still exists there
    by[i] = 0;
    // make each element of b 0
  }

  weightTuple tempWT;
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
          ky[tempnodenum0]++;
          Ay[tempnodenum0][ky[tempnodenum0]] = tempWT;

          tempWT.pin = tempnodenum0;
          tempWT.weight = -weighty;
          ky[tempnodenum1]++;
          Ay[tempnodenum1][ky[tempnodenum1]] = tempWT;

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
  HPWLX_new = 0;
  std::vector<Net> &net_list = *NetList();
  for (auto &&net: net_list) {
    HPWLX_new += net.HPWLX();
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

void GPSimPL::BuildProblemB2BX() {
  // before build a new Matrix, clean the information in existing matrix
  for (int i=0; i< TotBlockNum(); i++) {
    Ax[i][0].weight = 0;
    // make the diagonal elements 0
    kx[i] = 0;
    // mark the length each row of matrix 0, although some data might still exists there
    bx[i] = 0;
    // make each element of b 0
  }
  // update the x direction max and min node in each net
  weightTuple tempWT;
  double weight_x, inv_p, temp_pin_loc_x0, temp_pin_loc_x1, temp_diff_offset;
  int temp_node_num0, temp_node_num1;
  size_t max_pindex_x, min_pindex_x;
  std::vector<Block> &block_list = *BlockList();
  std::vector<Net> &net_list = *NetList();
  for (auto &&net: net_list) {
    if (net.P()<=1) {
      continue;
    }
    inv_p = net.InvP();
    max_pindex_x = net.MaxPinX();
    min_pindex_x = net.MinPinX();
    for (size_t i=0; i<net.blk_pin_list.size(); i++) {
      temp_node_num0 = net.blk_pin_list[i].GetBlock()->Num();
      temp_pin_loc_x0 = block_list[temp_node_num0].LLX() + net.blk_pin_list[i].XOffset();
      for (size_t k=i+1; k<net.blk_pin_list.size(); k++) {
        if ((i!=max_pindex_x)&&(i!=min_pindex_x)) {
          if ((k!=max_pindex_x)&&(k!=min_pindex_x)) continue;
        }
        temp_node_num1 = net.blk_pin_list[k].GetBlock()->Num();
        if (temp_node_num0 == temp_node_num1) continue;
        temp_pin_loc_x1 = block_list[temp_node_num1].LLX() + net.blk_pin_list[k].XOffset();
        weight_x = inv_p/(std::fabs(temp_pin_loc_x0 - temp_pin_loc_x1) + WidthEpsilon());
        if ((block_list[temp_node_num0].IsMovable() == 0)&&(block_list[temp_node_num1].IsMovable() == 0)) {
          continue;
        }
        else if ((block_list[temp_node_num0].IsMovable() == 0)&&(block_list[temp_node_num1].IsMovable() == 1)) {
          bx[temp_node_num1] += (temp_pin_loc_x0 - net.blk_pin_list[k].XOffset()) * weight_x;
          Ax[temp_node_num1][0].weight += weight_x;
        }
        else if ((block_list[temp_node_num0].IsMovable() == 1)&&(block_list[temp_node_num1].IsMovable() == 0)) {
          bx[temp_node_num0] += (temp_pin_loc_x1 - net.blk_pin_list[i].XOffset()) * weight_x;
          Ax[temp_node_num0][0].weight += weight_x;
        }
        else {
          //((blockList[temp_node_num0].IsMovable())&&(blockList[temp_node_num1].IsMovable()))
          tempWT.pin = temp_node_num1;
          tempWT.weight = -weight_x;
          kx[temp_node_num0]++;
          Ax[temp_node_num0][kx[temp_node_num0]] = tempWT;

          tempWT.pin = temp_node_num0;
          tempWT.weight = -weight_x;
          kx[temp_node_num1]++;
          Ax[temp_node_num1][kx[temp_node_num1]] = tempWT;

          Ax[temp_node_num0][0].weight += weight_x;
          Ax[temp_node_num1][0].weight += weight_x;
          temp_diff_offset = (net.blk_pin_list[k].XOffset() - net.blk_pin_list[i].XOffset()) * weight_x;
          bx[temp_node_num0] += temp_diff_offset;
          bx[temp_node_num1] -= temp_diff_offset;
        }
      }
    }
  }
  for (size_t i=0; i<Ax.size(); i++) { // this is for cells with tiny force applied on them
    if (Ax[i][0].weight < 1e-10) {
      Ax[i][0].weight = 1;
      bx[i] = block_list[i].LLX();
    }
  }
}

void GPSimPL::BuildProblemB2BXNoOffset() {
  // before build a new Matrix, clean the information in existing matrix
  for (int i=0; i< TotBlockNum(); i++) {
    Ax[i][0].weight = 0;
    // make the diagonal elements 0
    kx[i] = 0;
    // mark the length each row of matrix 0, although some data might still exists there
    bx[i] = 0;
    // make each element of b 0
  }
  weightTuple tempWT;
  double weightx, invp, temppinlocx0, temppinlocx1;
  int tempnodenum0, tempnodenum1;
  size_t maxpindex_x, minpindex_x;
  std::vector<Block> &block_list = *BlockList();
  std::vector<Net> &net_list = *NetList();
  for (auto &&net: net_list) {
    if (net.P()<=1) continue;
    invp = net.InvP();
    maxpindex_x = net.MaxPinX();
    minpindex_x = net.MinPinX();
    for (size_t i=0; i<net.blk_pin_list.size(); i++) {
      tempnodenum0 = net.blk_pin_list[i].GetBlock()->Num();
      temppinlocx0 = block_list[tempnodenum0].LLX();
      for (size_t k=i+1; k<net.blk_pin_list.size(); k++) {
        if ((i!=maxpindex_x)&&(i!=minpindex_x)) {
          if ((k!=maxpindex_x)&&(k!=minpindex_x)) continue;
        }
        tempnodenum1 = net.blk_pin_list[k].GetBlock()->Num();
        if (tempnodenum0 == tempnodenum1) continue;
        temppinlocx1 = block_list[tempnodenum1].LLX();
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
          kx[tempnodenum0]++;
          /*if (kx[tempnodenum0] == Ax[tempnodenum0].size()) {
            std::cout << tempnodenum0 << " " << kx[tempnodenum0] << " Overflowx\n";
          }*/
          Ax[tempnodenum0][kx[tempnodenum0]] = tempWT;

          tempWT.pin = tempnodenum0;
          tempWT.weight = -weightx;
          kx[tempnodenum1]++;
          /*if (kx[tempnodenum1] == Ax[tempnodenum1].size()) {
            std::cout << tempnodenum1 << " " << kx[tempnodenum1] << " Overflowx\n";
          }*/
          Ax[tempnodenum1][kx[tempnodenum1]] = tempWT;

          Ax[tempnodenum0][0].weight += weightx;
          Ax[tempnodenum1][0].weight += weightx;
        }
      }
    }
  }
}

void GPSimPL::UpdateHPWLY() {
  // update the y direction max and min node in each net
  HPWLY_new = HPWLY();
}

void GPSimPL::UpdateMaxMinY() {
  // update the y direction max and min node in each net
  HPWLY_new = 0;
  std::vector<Net> &net_list = *NetList();
  for (auto &&net: net_list) {
    HPWLY_new += net.HPWLY();
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

void GPSimPL::BuildProblemB2BY() {
  // before build a new Matrix, clean the information in existing matrix
  for (int i=0; i< TotBlockNum(); i++) {
    Ay[i][0].weight = 0;
    // make the diagonal elements 0
    ky[i] = 0;
    // mark the length each row of matrix 0, although some data might still exists there
    by[i] = 0;
    // make each element of b 0
  }
  weightTuple tempWT;
  double weighty, inv_p, temp_pin_loc_y0, temp_pin_loc_y1, temp_diff_offset;
  int temp_node_num0, temp_node_num1;
  size_t max_pindex_y, min_pindex_y;
  std::vector<Block> &block_list = *BlockList();
  std::vector<Net> &net_list = *NetList();
  for (auto &&net: net_list) {
    if (net.P()<=1) {
      continue;
    }
    inv_p = net.InvP();
    max_pindex_y = net.MaxPinY();
    min_pindex_y = net.MinPinY();
    for (size_t i=0; i<net.blk_pin_list.size(); i++) {
      temp_node_num0 = net.blk_pin_list[i].GetBlock()->Num();
      temp_pin_loc_y0 = block_list[temp_node_num0].LLY() + net.blk_pin_list[i].YOffset();
      for (size_t k=i+1; k<net.blk_pin_list.size(); k++) {
        if ((i!=max_pindex_y)&&(i!=min_pindex_y)) {
          if ((k!=max_pindex_y)&&(k!=min_pindex_y)) continue;
        }
        temp_node_num1 = net.blk_pin_list[k].GetBlock()->Num();
        if (temp_node_num0 == temp_node_num1) continue;
        temp_pin_loc_y1 = block_list[temp_node_num1].LLY() + net.blk_pin_list[i].YOffset();
        weighty = inv_p/((double)fabs(temp_pin_loc_y0 - temp_pin_loc_y1) + HeightEpsilon());
        if ((block_list[temp_node_num0].IsMovable() == 0)&&(block_list[temp_node_num1].IsMovable() == 0)) {
          continue;
        }
        else if ((block_list[temp_node_num0].IsMovable() == 0)&&(block_list[temp_node_num1].IsMovable() == 1)) {
          by[temp_node_num1] += (temp_pin_loc_y0 - net.blk_pin_list[k].YOffset()) * weighty;
          Ay[temp_node_num1][0].weight += weighty;
        }
        else if ((block_list[temp_node_num0].IsMovable() == 1)&&(block_list[temp_node_num1].IsMovable() == 0)) {
          by[temp_node_num0] += (temp_pin_loc_y1 - net.blk_pin_list[i].YOffset()) * weighty;
          Ay[temp_node_num0][0].weight += weighty;
        }
        else {
          //((blockList[temp_node_num0].IsMovable())&&(blockList[temp_node_num1].IsMovable()))
          tempWT.pin = temp_node_num1;
          tempWT.weight = -weighty;
          ky[temp_node_num0]++;
          Ay[temp_node_num0][ky[temp_node_num0]] = tempWT;

          tempWT.pin = temp_node_num0;
          tempWT.weight = -weighty;
          ky[temp_node_num1]++;
          Ay[temp_node_num1][ky[temp_node_num1]] = tempWT;

          Ay[temp_node_num0][0].weight += weighty;
          Ay[temp_node_num1][0].weight += weighty;
          temp_diff_offset = (net.blk_pin_list[k].YOffset() - net.blk_pin_list[i].YOffset()) * weighty;
          by[temp_node_num0] += temp_diff_offset;
          by[temp_node_num1] -= temp_diff_offset;
        }
      }
    }
  }
  for (size_t i=0; i<Ay.size(); i++) { // // this is for cells with tiny force applied on them
    if (Ay[i][0].weight < 1e-10) {
      Ay[i][0].weight = 1;
      by[i] = block_list[i].LLY();
    }
  }
}

void GPSimPL::BuildProblemB2BYNoOffset() {
  // before build a new Matrix, clean the information in existing matrix
  for (int i=0; i< TotBlockNum(); i++) {
    Ay[i][0].weight = 0;
    // make the diagonal elements 0
    ky[i] = 0;
    // mark the length each row of matrix 0, although some data might still exists there
    by[i] = 0;
    // make each element of b 0
  }
  weightTuple tempWT;
  double weighty, invp, temppinlocy0, temppinlocy1;
  int tempnodenum0, tempnodenum1;
  size_t maxpindex_y, minpindex_y;
  std::vector<Block> &block_list = *BlockList();
  std::vector<Net> &net_list = *NetList();
  for (auto &&net: net_list) {
    if (net.P()<=1) continue;
    invp = net.InvP();
    maxpindex_y = net.MaxPinY();
    minpindex_y = net.MinPinY();
    for (size_t i=0; i<net.blk_pin_list.size(); i++) {
      tempnodenum0 = net.blk_pin_list[i].GetBlock()->Num();
      temppinlocy0 = block_list[tempnodenum0].LLY();
      for (size_t k=i+1; k<net.blk_pin_list.size(); k++) {
        if ((i!=maxpindex_y)&&(i!=minpindex_y)) {
          if ((k!=maxpindex_y)&&(k!=minpindex_y)) continue;
        }
        tempnodenum1 = net.blk_pin_list[k].GetBlock()->Num();
        if (tempnodenum0 == tempnodenum1) continue;
        temppinlocy1 = block_list[tempnodenum1].LLY();
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
          ky[tempnodenum0]++;
          /*if (ky[tempnodenum0] == Ay[tempnodenum0].size()) {
            std::cout << tempnodenum0 << " " << ky[tempnodenum0] << " Overflowy\n";
          }*/
          Ay[tempnodenum0][ky[tempnodenum0]] = tempWT;

          tempWT.pin = tempnodenum0;
          tempWT.weight = -weighty;
          ky[tempnodenum1]++;
          /*if (ky[tempnodenum1] == Ay[tempnodenum1].size()) {
            std::cout << tempnodenum1 << " " << ky[tempnodenum1] << " Overflowy\n";
          }*/
          Ay[tempnodenum1][ky[tempnodenum1]] = tempWT;

          Ay[tempnodenum0][0].weight += weighty;
          Ay[tempnodenum1][0].weight += weighty;
        }
      }
    }
  }
}

void GPSimPL::CGSolver(std::string const &dimension, std::vector<std::vector<weightTuple> > &A, std::vector<double> &b, std::vector<size_t> &k) {
  double epsilon = 1e-15, alpha, beta, rsold, rsnew, pAp, solution_distance;
  for (int i=0; i< TotBlockNum(); i++) {
    // initialize the Jacobi pre-conditioner
    if (A[i][0].weight > epsilon) {
      // JP[i] is the reverse of diagonal value
      JP[i] = 1/A[i][0].weight;
    }
    else {
      // if the diagonal is too small, set JP[i] to 1 to prevent diverge
      JP[i] = 1;
    }
  }
  std::vector<Block> &block_list = *BlockList();
  for (int i=0; i< TotBlockNum(); i++) {
    // calculate Ax
    ax[i] = 0;
    if (dimension=="x") {
      for (size_t j=0; j<=k[i]; j++) {
        ax[i] += A[i][j].weight * block_list[A[i][j].pin].X();
      }
    }
    else {
      for (size_t j=0; j<=k[i]; j++) {
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
      for (size_t j=0; j<=k[i]; j++) {
        ap[i] += A[i][j].weight * p[A[i][j].pin];
      }
      pAp += p[i]*ap[i];
    }
    alpha = rsold/pAp;
    rsnew = 0;
    if (dimension=="x") {
      for (int i=0; i< TotBlockNum(); i++) {
        block_list[i].IncreX(alpha * p[i]);
      }
    }
    else {
      for (int i=0; i< TotBlockNum(); i++) {
        block_list[i].IncreY(alpha * p[i]);
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
      if (block_list[i].LLX() < Left()) {
        block_list[i].SetCenterX(Left() + block_list[i].Width()/2.0 + 1);
        //std::cout << i << "\n";
      }
      else if (block_list[i].URX() > Right()) {
        block_list[i].SetCenterX(Right() - block_list[i].Width()/2.0 - 1);
        //std::cout << i << "\n";
      }
      else {
        continue;
      }
    }
  }
  else{
    for (int i=0; i< TotBlockNum(); i++) {
      if (block_list[i].LLY() < Bottom()) {
        block_list[i].SetCenterY(Bottom() + block_list[i].Height()/2.0 + 1);
        //std::cout << i << "\n";
      }
      else if (block_list[i].URY() > Top()) {
        block_list[i].SetCenterY(Top() - block_list[i].Height()/2.0 - 1);
        //std::cout << i << "\n";
      }
      else {
        continue;
      }
    }
  }
}

void GPSimPL::CGSolverX() {
  CGSolver("x", Ax, bx, kx);
}

void GPSimPL::CGsolverY() {
  CGSolver("y", Ay, by, ky);
}

void GPSimPL::CGClose() {
  Ax.clear();
  Ay.clear();
  bx.clear();
  by.clear();
  kx.clear();
  ky.clear();
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
  /*
  std::vector<Net> &net_list = *NetList();
  block_al_t *block_ptr0, *block_ptr1;
  for (auto &&net: netList) {
    for (int i=0; i<net.blk_pin_list.size(); i++) {
      block_ptr0 = (block_al_t *)net.blk_pin_list[i].GetBlock();
      for (int j=i+1; j<net.blk_pin_list.size(); j++) {
        block_ptr1 = (block_al_t *)net.blk_pin_list[j].GetBlock();
        ost << "line([" << block_ptr0->LLX() + net.blk_pin_list[i].XOffset() << "," << block_ptr1->LLX() + net.blk_pin_list[j].XOffset()
               << "],[" << block_ptr0->LLY() + net.blk_pin_list[i].YOffset() << "," << block_ptr1->LLY() + net.blk_pin_list[j].YOffset() << "],'lineWidth', 0.5)\n";
      }
    }
  }
   */
  ost.close();
}

void GPSimPL::StartPlacement() {
  CGInit();
  UniformInit();
  UpdateMaxMinX();
  UpdateMaxMinY();
  HPWLx_converge = false;
  HPWLy_converge = false;
  HPWLX_old = 1e30;
  HPWLY_old = 1e30;
  for (int i=0; i<50; i++) {
    if (!HPWLx_converge) {
      BuildProblemB2BX();
      CGSolverX();
      UpdateMaxMinX();
    }
    if (!HPWLy_converge) {
      BuildProblemB2BY();
      CGsolverY();
      UpdateMaxMinY();
    }
    if (HPWLx_converge && HPWLy_converge)  {
      std::cout << i << " iterations in cg\n";
      break;
    }
  }
  CGClose();
  std::cout << "Initial Placement Complete\n";
  ReportHPWL();
}
