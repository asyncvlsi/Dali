//
// Created by Yihang Yang on 9/3/19.
//

#include "DPLinear.h"

DPLinear::DPLinear(): GPSimPL() {}

void DPLinear::UpdateScaffoldNetList() {
  scaffold_net_list.clear();
  std::vector<Block> &block_list = *BlockList();
  for (size_t i=0; i<block_list.size(); ++i) {
    Block &block0 = block_list[i];
    for (size_t j=i+1; j<block_list.size(); ++j) {
      Block &block1 = block_list[j];
      if (block0.IsOverlap(block1)) {
        scaffold_net_list.emplace_back(block0, block1);
      }
    }
  }
}

void DPLinear::BuildProblemDPLinear(bool is_x_direction, Eigen::VectorXd &b) {
  /****
   * In order to build the problem, there are two steps, what we are trying to minimizing here is:
   * (1-r)HPWL + rD
   * where r changes from 0 to 1,
   * the first term is the traditional HPWL, the second term is a density penalty term.
   * Two steps to build the problem:
   * 1. build the problem for the first part, remember that now each memtrix element and vector element should times (1-r);
   * 2. build the problem for the second part, and each coefficient should times r.
   * ****/
  std::vector<Block> &block_list = *BlockList();
  std::vector<Net> &net_list = *NetList();
  size_t coefficients_capacity = coefficients.capacity();
  coefficients.resize(0);
  for (long int i = 0; i < b.size(); i++) {
    b[i] = 0;
  }
  double weight, inv_p, pin_loc0, pin_loc1, offset_diff;
  size_t blk_num0, blk_num1, max_pin_index, min_pin_index;
  bool is_movable0, is_movable1;
  if (is_x_direction) {
    for (auto &&net: net_list) {
      if (net.P() <= 1) continue;
      inv_p = net.InvP();
      net.UpdateMaxMinX();
      max_pin_index = net.MaxBlkPinNumX();
      min_pin_index = net.MinBlkPinNumX();
      for (size_t i = 0; i < net.blk_pin_list.size(); i++) {
        blk_num0 = net.blk_pin_list[i].BlockNum();
        pin_loc0 = block_list[blk_num0].LLX() + net.blk_pin_list[i].XOffset();
        is_movable0 = net.blk_pin_list[i].GetBlock()->IsMovable();
        for (size_t j = i + 1; j < net.blk_pin_list.size(); j++) {
          if ((i != max_pin_index) && (i != min_pin_index)) {
            if ((j != max_pin_index) && (j != min_pin_index)) continue;
          }
          blk_num1 = net.blk_pin_list[j].BlockNum();
          if (blk_num0 == blk_num1) continue;
          pin_loc1 = block_list[blk_num1].LLX() + net.blk_pin_list[j].XOffset();
          is_movable1 = net.blk_pin_list[j].GetBlock()->IsMovable();
          weight = inv_p / (std::fabs(pin_loc0 - pin_loc1) + WidthEpsilon()) * (1-r);
          if (!is_movable0 && is_movable1) {
            b[blk_num1] += (pin_loc0 - net.blk_pin_list[j].XOffset()) * weight;
            coefficients.emplace_back(T(blk_num1, blk_num1, weight));
          } else if (is_movable0 && !is_movable1) {
            b[blk_num0] += (pin_loc1 - net.blk_pin_list[i].XOffset()) * weight ;
            coefficients.emplace_back(T(blk_num0, blk_num0, weight));
          } else if (is_movable0 && is_movable1) {
            coefficients.emplace_back(T(blk_num0, blk_num0, weight));
            coefficients.emplace_back(T(blk_num1, blk_num1, weight));
            coefficients.emplace_back(T(blk_num0, blk_num1, -weight));
            coefficients.emplace_back(T(blk_num1, blk_num0, -weight));
            offset_diff = (net.blk_pin_list[j].XOffset() - net.blk_pin_list[i].XOffset()) * weight;
            b[blk_num0] += offset_diff;
            b[blk_num1] -= offset_diff;
          } else {
            continue;
          }
        }
      }
    }

    for (auto &&scaffold_net: scaffold_net_list) {
      blk_num0 = scaffold_net.Block0()->Num();
      blk_num1 = scaffold_net.Block1()->Num();
      pin_loc0 = scaffold_net.XAbsLoc0();
      pin_loc1 = scaffold_net.XAbsLoc1();
      weight = scaffold_net.Weight()/(std::fabs(pin_loc0 - pin_loc1) + WidthEpsilon()) * r;

      coefficients.emplace_back(T(blk_num0, blk_num0, weight ));
      coefficients.emplace_back(T(blk_num1, blk_num1, weight));
      coefficients.emplace_back(T(blk_num0, blk_num1, -weight));
      coefficients.emplace_back(T(blk_num1, blk_num0, -weight));
      offset_diff = (scaffold_net.XOffset1() - scaffold_net.XOffset0()) * weight;
      b[blk_num0] += offset_diff;
      b[blk_num1] -= offset_diff;
    }

    for (size_t i = 0; i < block_list.size(); ++i) {
      if (block_list[i].IsFixed()) {
        coefficients.emplace_back(T(i, i, 1));
        b[i] = block_list[i].LLX();
      }
    }
  } else {
    for (auto &&net: net_list) {
      if (net.P() <= 1) continue;
      inv_p = net.InvP();
      net.UpdateMaxMinY();
      max_pin_index = net.MaxBlkPinNumY();
      min_pin_index = net.MinBlkPinNumY();
      for (size_t i = 0; i < net.blk_pin_list.size(); i++) {
        blk_num0 = net.blk_pin_list[i].BlockNum();
        pin_loc0 = block_list[blk_num0].LLY() + net.blk_pin_list[i].YOffset();
        is_movable0 = net.blk_pin_list[i].GetBlock()->IsMovable();
        for (size_t j = i + 1; j < net.blk_pin_list.size(); j++) {
          if ((i != max_pin_index) && (i != min_pin_index)) {
            if ((j != max_pin_index) && (j != min_pin_index)) continue;
          }
          blk_num1 = net.blk_pin_list[j].BlockNum();
          if (blk_num0 == blk_num1) continue;
          pin_loc1 = block_list[blk_num1].LLY() + net.blk_pin_list[j].YOffset();
          is_movable1 = net.blk_pin_list[j].GetBlock()->IsMovable();
          weight = inv_p / (std::fabs(pin_loc0 - pin_loc1) + HeightEpsilon()) * (1-r);
          if (!is_movable0 && is_movable1) {
            b[blk_num1] += (pin_loc0 - net.blk_pin_list[j].YOffset()) * weight;
            coefficients.emplace_back(T(blk_num1, blk_num1, weight));
          } else if (is_movable0 && !is_movable1) {
            b[blk_num0] += (pin_loc1 - net.blk_pin_list[i].YOffset()) * weight;
            coefficients.emplace_back(T(blk_num0, blk_num0, weight));
          } else if (is_movable0 && is_movable1) {
            coefficients.emplace_back(T(blk_num0, blk_num0, weight));
            coefficients.emplace_back(T(blk_num1, blk_num1, weight));
            coefficients.emplace_back(T(blk_num0, blk_num1, -weight));
            coefficients.emplace_back(T(blk_num1, blk_num0, -weight));
            offset_diff = (net.blk_pin_list[j].YOffset() - net.blk_pin_list[i].YOffset()) * weight;
            b[blk_num0] += offset_diff;
            b[blk_num1] -= offset_diff;
          } else {
            continue;
          }
        }
      }
    }

    for (auto &&scaffold_net: scaffold_net_list) {
      blk_num0 = scaffold_net.Block0()->Num();
      blk_num1 = scaffold_net.Block1()->Num();
      pin_loc0 = scaffold_net.YAbsLoc0();
      pin_loc1 = scaffold_net.YAbsLoc1();
      weight = scaffold_net.Weight()/(std::fabs(pin_loc0 - pin_loc1) + HeightEpsilon()) * r;

      coefficients.emplace_back(T(blk_num0, blk_num0, weight * r));
      coefficients.emplace_back(T(blk_num1, blk_num1, weight * r));
      coefficients.emplace_back(T(blk_num0, blk_num1, -weight * r));
      coefficients.emplace_back(T(blk_num1, blk_num0, -weight * r));
      offset_diff = (scaffold_net.YOffset1() - scaffold_net.YOffset0()) * weight;
      b[blk_num0] += offset_diff * r;
      b[blk_num1] -= offset_diff * r;
    }

    for (size_t i = 0; i < block_list.size(); ++i) { // add the diagonal non-zero element for fixed blocks
      if (block_list[i].IsFixed()) {
        coefficients.emplace_back(T(i, i, 1));
        b[i] = block_list[i].LLY();
      }
    }
  }
  if (globalVerboseLevel >= LOG_WARNING) {
    if (coefficients_capacity != coefficients.capacity()) {
      std::cout << "WARNING: coefficients capacity changed!\n";
      std::cout << "\told capacity: " << coefficients_capacity << "\n";
      std::cout << "\tnew capacity: " << coefficients.size() << "\n";
    }
  }
}

void DPLinear::BuildProblemDPLinearX() {
  BuildProblemDPLinear(true, bx);
  std::vector<Block> &block_list = *BlockList();
  for (size_t i = 0; i < block_list.size(); ++i) {
    if (block_list[i].IsMovable()) {
      coefficients.emplace_back(T(i, i, 0.1));
      bx[i] = block_list[i].LLX() * 0.1;
    }
  }
  Ax.setFromTriplets(coefficients.begin(), coefficients.end());
}

void DPLinear::BuildProblemDPLinearY() {
  BuildProblemDPLinear(false, by);
  std::vector<Block> &block_list = *BlockList();
  for (size_t i = 0; i < block_list.size(); ++i) { // add the diagonal non-zero element for fixed blocks
    if (block_list[i].IsMovable()) {
      coefficients.emplace_back(T(i, i, 0.1));
      by[i] = block_list[i].LLY() * 0.1;
    }
  }
  Ay.setFromTriplets(coefficients.begin(), coefficients.end());
}

void DPLinear::StartPlacement() {
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "Start detailed placement\n";
  }
  SanityCheck();
  CGInit();

  r = 0.1;
  std::vector<Block> &block_list = *BlockList();
  //for (int t=0; t<=10; ++t) {
    //r = 0.1 * t;
    HPWLX_converge = false;
    HPWLX_old = 1e30;
    for (size_t i = 0; i < block_list.size(); ++i) {
      x[i] = block_list[i].LLX();
    }
    UpdateMaxMinX();
    //UpdateScaffoldNetList();
    for (int i = 0; i < 50; ++i) {
      BuildProblemDPLinearX();
      SolveProblemX();
      UpdateCGFlagsX();
      if (HPWLX_converge) {
        if (globalVerboseLevel >= LOG_DEBUG) {
          std::cout << "iterations x:     " << i << "\n";
        }
        break;
      }
    }
    HPWLY_converge = false;
    HPWLY_old = 1e30;
    for (size_t i = 0; i < block_list.size(); ++i) {
      y[i] = block_list[i].LLY();
    }
    UpdateMaxMinY();
    //UpdateScaffoldNetList();
    for (int i = 0; i < 50; ++i) {
      BuildProblemDPLinearY();
      SolveProblemY();
      UpdateCGFlagsY();
      if (HPWLY_converge) {
        if (globalVerboseLevel >= LOG_DEBUG) {
          std::cout << "iterations y:     " << i << "\n";
        }
        break;
      }
    }
  //}
  if (globalVerboseLevel >= LOG_INFO) {
    std::cout << "Quadratic Placement With Anchor Complete\n";
  }
  ReportHPWL(LOG_INFO);
}