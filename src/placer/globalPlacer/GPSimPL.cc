//
// Created by Yihang Yang on 8/4/2019.
//

#include "GPSimPL.h"

#include <cmath>

#include <algorithm>

#include <unsupported/Eigen/SparseExtra>

GPSimPL::GPSimPL() : Placer() {
  grid_bin_height = 0;
  grid_bin_width = 0;
  grid_cnt_y = 0;
  grid_cnt_x = 0;
  width_epsilon = 0;
  height_epsilon = 0;
}

GPSimPL::GPSimPL(double aspectRatio, double fillingRate) : Placer(aspectRatio, fillingRate) {
  grid_bin_height = 0;
  grid_bin_width = 0;
  grid_cnt_y = 0;
  grid_cnt_x = 0;
  width_epsilon = 0;
  height_epsilon = 0;
}

void GPSimPL::InitCGFlags() {
  HPWLX_new = 0;
  HPWLY_new = 0;
  HPWLX_old = DBL_MAX;
  HPWLY_old = DBL_MAX;
  HPWLX_converge = false;
  HPWLY_converge = false;
}

void GPSimPL::BlockLocRandomInit() {
  std::minstd_rand0 generator{1};
  int region_width = RegionWidth();
  int region_height = RegionHeight();
  std::uniform_real_distribution<double> distribution(0, 1);

  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("HPWL before initialization: %e\n", circuit_->HPWL());
  }

  for (auto &block: *BlockList()) {
    if (block.IsMovable()) {
      block.setCenterX(RegionLeft() + region_width * distribution(generator));
      block.setCenterY(RegionBottom() + region_height * distribution(generator));
    }
  }
  if (globalVerboseLevel >= LOG_INFO) {
    std::cout << "Block location randomly uniform initialization complete\n";
  }

  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("HPWL after initialization: %e\n", circuit_->HPWL());
  }
}

void GPSimPL::BlockLocCenterInit() {
  double region_center_x = (RegionRight() + RegionLeft()) / 2.0;
  double region_center_y = (RegionTop() + RegionBottom()) / 2.0;
  for (auto &block: *BlockList()) {
    if (block.IsMovable()) {
      block.setCenterX(region_center_x);
      block.setCenterY(region_center_y);
    }
  }
  if (globalVerboseLevel >= LOG_INFO) {
    std::cout << "Block location initialization complete\n";
  }
  ReportHPWL(LOG_INFO);
}

void GPSimPL::CGInit() {
  SetEpsilon(); // set a small value for net weight dividend to avoid divergence
  int sz = BlockList()->size();
  vx.resize(sz);
  vy.resize(sz);
  bx.resize(sz);
  by.resize(sz);
  Ax.resize(sz, sz);
  Ay.resize(sz, sz);
  x_anchor.resize(sz);
  y_anchor.resize(sz);

  cgx.setMaxIterations(cg_iteration_max_num_);
  cgx.setTolerance(cg_tolerance_);
  cgy.setMaxIterations(cg_iteration_max_num_);
  cgy.setTolerance(cg_tolerance_);

  int coefficient_size = 0;
  int net_sz = 0;
  for (auto &net: *NetList()) {
    net_sz = net.P();
    // if a net has size n, then in total, there will be (2(n-2)+1)*4 non-zero entries in the matrix
    if (net_sz > 1) coefficient_size += ((net_sz - 2) * 2 + 1) * 4;
  }
  // this is to reserve space for anchor, because each block may need an anchor
  coefficient_size += sz;

  // this is to reserve space for anchor in the center of the placement region
  coefficient_size += sz;
  coefficients.reserve(coefficient_size);
}

void GPSimPL::UpdateCGFlagsX() {
  UpdateHPWLX();
  //std::cout << "HPWLX_old\tHPWLX_new\n";
  //std::cout << HPWLX_old << "\t" << HPWLX_new << "\n";
  if (HPWLX_new == 0) { // this is for 1 degree net, this happens in extremely rare cases
    HPWLX_converge = true;
  } else {
    HPWLX_converge = (std::fabs(1 - HPWLX_new / HPWLX_old) < HPWL_intra_linearSolver_precision);
    HPWLX_old = HPWLX_new;
  }
}

void GPSimPL::UpdateMaxMinX() {
  for (auto &net: *NetList()) {
    net.UpdateMaxMinIndexX();
  }
}

void GPSimPL::UpdateMaxMinCtoCX() {
  HPWLX_new = 0;
  for (auto &net: *NetList()) {
    HPWLX_new += net.HPWLCtoCX();
  }
  //std::cout << "HPWLX_old: " << HPWLX_old << "\n";
  //std::cout << "HPWLX_new: " << HPWLX_new << "\n";
  //std::cout << 1 - HPWLX_new/HPWLX_old << "\n";
  if (HPWLX_new == 0) { // this is for 1 degree net, this happens in extremely rare cases
    HPWLX_converge = true;
  } else {
    HPWLX_converge = (std::fabs(1 - HPWLX_new / HPWLX_old) < HPWL_intra_linearSolver_precision);
    HPWLX_old = HPWLX_new;
  }
}

void GPSimPL::UpdateCGFlagsY() {
  UpdateHPWLY();
  //std::cout << "HPWLY_old\tHPWLY_new\n";
  //std::cout << HPWLY_old << "\t" << HPWLY_new << "\n";
  if (HPWLY_new == 0) { // this is for 1 degree net, this happens in extremely rare cases
    HPWLY_converge = true;
  } else {
    HPWLY_converge = (std::fabs(1 - HPWLY_new / HPWLY_old) < HPWL_intra_linearSolver_precision);
    HPWLY_old = HPWLY_new;
  }
}

void GPSimPL::UpdateMaxMinY() {
  for (auto &net: *NetList()) {
    net.UpdateMaxMinIndexY();
  }
}

void GPSimPL::UpdateMaxMinCtoCY() {
  // update the y direction max and min node in each net
  HPWLY_new = 0;
  for (auto &net: *NetList()) {
    HPWLY_new += net.HPWLCtoCY();
  }
  //std::cout << "HPWLY_old: " << HPWLY_old << "\n";
  //std::cout << "HPWLY_new: " << HPWLY_new << "\n";
  //std::cout << 1 - HPWLY_new/HPWLY_old << "\n";
  if (HPWLY_new == 0) { // this is for 1 degree net, this happens in extremely rare cases
    HPWLY_converge = true;
  } else {
    HPWLY_converge = (std::fabs(1 - HPWLY_new / HPWLY_old) < HPWL_intra_linearSolver_precision);
    HPWLY_old = HPWLY_new;
  }
}

void GPSimPL::AddMatrixElement(Net &net, int i, int j) {
  double weight, inv_p, pin_loc0, pin_loc1, offset_diff;
  int blk_num0, blk_num1;
  bool is_movable0, is_movable1;
  blk_num0 = net.blk_pin_list[i].BlkNum();
  pin_loc0 = net.blk_pin_list[i].AbsX();
  is_movable0 = net.blk_pin_list[i].BlkPtr()->IsMovable();

  inv_p = net.InvP();
  blk_num1 = net.blk_pin_list[j].BlkNum();
  if (blk_num0 == blk_num1) return;
  pin_loc1 = net.blk_pin_list[j].AbsX();
  is_movable1 = net.blk_pin_list[j].BlkPtr()->IsMovable();
  weight = inv_p / (std::fabs(pin_loc0 - pin_loc1) + WidthEpsilon());
  if (!is_movable0 && is_movable1) {
    bx[blk_num1] += (pin_loc0 - net.blk_pin_list[j].OffsetX()) * weight;
    coefficients.emplace_back(T(blk_num1, blk_num1, weight));
  } else if (is_movable0 && !is_movable1) {
    bx[blk_num0] += (pin_loc1 - net.blk_pin_list[i].OffsetX()) * weight;
    coefficients.emplace_back(T(blk_num0, blk_num0, weight));
  } else if (is_movable0 && is_movable1) {
    coefficients.emplace_back(T(blk_num0, blk_num0, weight));
    coefficients.emplace_back(T(blk_num1, blk_num1, weight));
    if (blk_num0 > blk_num1) {
      coefficients.emplace_back(T(blk_num0, blk_num1, -weight));
    } else {
      coefficients.emplace_back(T(blk_num1, blk_num0, -weight));
    }
    offset_diff = (net.blk_pin_list[j].OffsetX() - net.blk_pin_list[i].OffsetX()) * weight;
    bx[blk_num0] += offset_diff;
    bx[blk_num1] -= offset_diff;
  } else {
  }
}

void GPSimPL::BuildProblemB2B(bool is_x_direction, Eigen::VectorXd &b) {
  std::vector<Block> &block_list = *BlockList();
  size_t coefficients_capacity = coefficients.capacity();
  coefficients.resize(0);

  int sz = b.size();
  for (int i = 0; i < sz; ++i) {
    b[i] = 0;
  }

  double center_weight = 0.03 / std::sqrt(sz);
  double weight_center_x = (RegionLeft() + RegionRight()) / 2.0 * center_weight;
  double weight_center_y = (RegionBottom() + RegionTop()) / 2.0 * center_weight;

  double weight;
  double weight_adjust;
  double decay_length = 5 * circuit_->AveBlkHeight();
  double adjust_factor = 1.5;
  double inv_p;

  double pin_loc;
  int blk_num;
  bool is_movable;
  double offset_diff;

  int max_pin_index;
  int blk_num_max;
  double pin_loc_max;
  bool is_movable_max;
  double offset_max;

  int min_pin_index;
  int blk_num_min;
  double pin_loc_min;
  bool is_movable_min;
  double offset_min;

  if (is_x_direction) {
    for (auto &&net: *NetList()) {
      if (net.P() <= 1 || net.P() >= net_ignore_threshold) continue;
      inv_p = net.InvP();
      net.UpdateMaxMinIndexX();
      max_pin_index = net.MaxBlkPinNumX();
      min_pin_index = net.MinBlkPinNumX();

      blk_num_max = net.blk_pin_list[max_pin_index].BlkNum();
      pin_loc_max = net.blk_pin_list[max_pin_index].AbsX();
      is_movable_max = net.blk_pin_list[max_pin_index].BlkPtr()->IsMovable();
      offset_max = net.blk_pin_list[max_pin_index].OffsetX();

      blk_num_min = net.blk_pin_list[min_pin_index].BlkNum();
      pin_loc_min = net.blk_pin_list[min_pin_index].AbsX();
      is_movable_min = net.blk_pin_list[min_pin_index].BlkPtr()->IsMovable();
      offset_min = net.blk_pin_list[min_pin_index].OffsetX();

      for (auto &pair: net.blk_pin_list) {
        blk_num = pair.BlkNum();
        pin_loc = pair.AbsX();
        is_movable = pair.BlkPtr()->IsMovable();

        if (blk_num != blk_num_max) {
          double distance = std::fabs(pin_loc - pin_loc_max);
          weight_adjust = adjust_factor * (1 - exp(-distance / decay_length));
          weight = inv_p / (distance + WidthEpsilon()) * weight_adjust;
          if (!is_movable && is_movable_max) {
            b[blk_num_max] += (pin_loc - offset_max) * weight;
            coefficients.emplace_back(blk_num_max, blk_num_max, weight);
          } else if (is_movable && !is_movable_max) {
            b[blk_num] += (pin_loc_max - offset_max) * weight;
            coefficients.emplace_back(blk_num, blk_num, weight);
          } else if (is_movable && is_movable_max) {
            coefficients.emplace_back(blk_num, blk_num, weight);
            coefficients.emplace_back(blk_num_max, blk_num_max, weight);
            if (blk_num > blk_num_max) { // build a lower matrix
              coefficients.emplace_back(blk_num, blk_num_max, -weight);
            } else {
              coefficients.emplace_back(blk_num_max, blk_num, -weight);
            }
            offset_diff = (offset_max - pair.OffsetX()) * weight;
            b[blk_num] += offset_diff;
            b[blk_num_max] -= offset_diff;
          }
        }

        if ((blk_num != blk_num_max) && (blk_num != blk_num_min)) {
          double distance = std::fabs(pin_loc - pin_loc_max);
          weight_adjust = adjust_factor * (1 - exp(-distance / decay_length));
          weight = inv_p / (distance + WidthEpsilon()) * weight_adjust;
          if (!is_movable && is_movable_min) {
            b[blk_num_min] += (pin_loc - offset_min) * weight;
            coefficients.emplace_back(blk_num_min, blk_num_min, weight);
          } else if (is_movable && !is_movable_min) {
            b[blk_num] += (pin_loc_min - offset_min) * weight;
            coefficients.emplace_back(blk_num, blk_num, weight);
          } else if (is_movable && is_movable_min) {
            coefficients.emplace_back(blk_num, blk_num, weight);
            coefficients.emplace_back(blk_num_min, blk_num_min, weight);
            if (blk_num > blk_num_min) { // build a lower matrix
              coefficients.emplace_back(blk_num, blk_num_min, -weight);
            } else {
              coefficients.emplace_back(blk_num_min, blk_num, -weight);
            }
            offset_diff = (offset_min - pair.OffsetX()) * weight;
            b[blk_num] += offset_diff;
            b[blk_num_min] -= offset_diff;
          }
        }

      }
    }
    for (int i = 0; i < sz; ++i) {
      if (block_list[i].IsFixed()) {
        coefficients.emplace_back(i, i, 1);
        b[i] = block_list[i].LLX();
      } else {
        if (block_list[i].LLX() < RegionLeft() || block_list[i].URX() > RegionRight()) {
          coefficients.emplace_back(i, i, center_weight);
          b[i] += weight_center_x;
        }
      }
    }
  } else {
    for (auto &net: *NetList()) {
      if (net.P() <= 1 || net.P() >= net_ignore_threshold) continue;
      inv_p = net.InvP();
      net.UpdateMaxMinIndexY();
      max_pin_index = net.MaxBlkPinNumY();
      min_pin_index = net.MinBlkPinNumY();

      blk_num_max = net.blk_pin_list[max_pin_index].BlkNum();
      pin_loc_max = net.blk_pin_list[max_pin_index].AbsY();
      is_movable_max = net.blk_pin_list[max_pin_index].BlkPtr()->IsMovable();
      offset_max = net.blk_pin_list[max_pin_index].OffsetY();

      blk_num_min = net.blk_pin_list[min_pin_index].BlkNum();
      pin_loc_min = net.blk_pin_list[min_pin_index].AbsY();
      is_movable_min = net.blk_pin_list[min_pin_index].BlkPtr()->IsMovable();
      offset_min = net.blk_pin_list[min_pin_index].OffsetY();

      for (auto &pair: net.blk_pin_list) {
        blk_num = pair.BlkNum();
        pin_loc = pair.AbsY();
        is_movable = pair.BlkPtr()->IsMovable();

        if (blk_num != blk_num_max) {
          double distance = std::fabs(pin_loc - pin_loc_max);
          weight_adjust = adjust_factor * (1 - exp(-distance / decay_length));
          weight = inv_p / (distance + HeightEpsilon()) * weight_adjust;
          if (!is_movable && is_movable_max) {
            b[blk_num_max] += (pin_loc - offset_max) * weight;
            coefficients.emplace_back(blk_num_max, blk_num_max, weight);
          } else if (is_movable && !is_movable_max) {
            b[blk_num] += (pin_loc_max - offset_max) * weight;
            coefficients.emplace_back(blk_num, blk_num, weight);
          } else if (is_movable && is_movable_max) {
            coefficients.emplace_back(blk_num, blk_num, weight);
            coefficients.emplace_back(blk_num_max, blk_num_max, weight);
            if (blk_num > blk_num_max) { // build a lower matrix
              coefficients.emplace_back(blk_num, blk_num_max, -weight);
            } else {
              coefficients.emplace_back(blk_num_max, blk_num, -weight);
            }
            offset_diff = (offset_max - pair.OffsetY()) * weight;
            b[blk_num] += offset_diff;
            b[blk_num_max] -= offset_diff;
          }
        }

        if ((blk_num != blk_num_max) && (blk_num != blk_num_min)) {
          double distance = std::fabs(pin_loc - pin_loc_max);
          weight_adjust = adjust_factor * (1 - exp(-distance / decay_length));
          weight = inv_p / (distance + HeightEpsilon()) * weight_adjust;
          if (!is_movable && is_movable_min) {
            b[blk_num_min] += (pin_loc - offset_min) * weight;
            coefficients.emplace_back(blk_num_min, blk_num_min, weight);
          } else if (is_movable && !is_movable_min) {
            b[blk_num] += (pin_loc_min - offset_min) * weight;
            coefficients.emplace_back(blk_num, blk_num, weight);
          } else if (is_movable && is_movable_min) {
            coefficients.emplace_back(blk_num, blk_num, weight);
            coefficients.emplace_back(blk_num_min, blk_num_min, weight);
            if (blk_num > blk_num_min) { // build a lower matrix
              coefficients.emplace_back(blk_num, blk_num_min, -weight);
            } else {
              coefficients.emplace_back(blk_num_min, blk_num, -weight);
            }
            offset_diff = (offset_min - pair.OffsetY()) * weight;
            b[blk_num] += offset_diff;
            b[blk_num_min] -= offset_diff;
          }
        }

      }
    }
    for (int i = 0; i < sz; ++i) { // add the diagonal non-zero element for fixed blocks
      if (block_list[i].IsFixed()) {
        coefficients.emplace_back(i, i, 1);
        b[i] = block_list[i].LLY();
      } else {
        if (block_list[i].LLY() < RegionBottom() || block_list[i].URY() > RegionTop()) {
          coefficients.emplace_back(i, i, center_weight);
          b[i] += weight_center_y;
        }
      }
    }
  }

  /*std::sort(coefficients.begin(), coefficients.end(), [](T& t1, T& t2) {
    return (t1.row() < t2.row() || (t1.row() == t2.row() && t1.col() < t2.col()) || (t1.row() == t2.row() && t1.col() == t2.col() && t1.value() < t2.value()));
  });*/

  if (globalVerboseLevel >= LOG_WARNING) {
    if (coefficients_capacity != coefficients.capacity()) {
      std::cout << "WARNING: coefficients capacity changed!\n"
                << "\told capacity: " << coefficients_capacity << "\n"
                << "\tnew capacity: " << coefficients.size() << "\n";
    }
  }
}

void GPSimPL::BuildProblemB2BX() {
  BuildProblemB2B(true, bx);
  Ax.setFromTriplets(coefficients.begin(), coefficients.end());
//  static int count = 0;
//  count += 1;
//  if (count == 100) {
//    bool s = saveMarket(Ax, "sparse_matrices");
//    if (s) {
//      std::cout << "Sparse matrix saved successfully\n";
//    }
//  }
}

void GPSimPL::BuildProblemB2BY() {
  BuildProblemB2B(false, by);
  Ay.setFromTriplets(coefficients.begin(), coefficients.end());
}

void GPSimPL::SolveProblemX() {
  cgx.compute(Ax);
  vx = cgx.solveWithGuess(bx, vx);
  error_x = cgx.error();
  if (globalVerboseLevel >= LOG_DEBUG) {
    std::cout << "    #iterations:     " << cgx.iterations() << std::endl;
    std::cout << "    estimated error: " << error_x << std::endl;
  }

  int sz = vx.size();
  auto it_block_list = BlockList()->begin();
  Eigen::VectorXd::const_iterator ivx = vx.begin();
  for (int num = 0; num < sz; ++num) {
    it_block_list[num].setLLX(ivx[num]);
  }
}

void GPSimPL::SolveProblemY() {
  cgy.compute(Ay);
  vy = cgy.solveWithGuess(by, vy);
  error_y = cgy.error();
  if (globalVerboseLevel >= LOG_DEBUG) {
    std::cout << "    #iterations:     " << cgy.iterations() << std::endl;
    std::cout << "    estimated error: " << error_y << std::endl;
  }

  int sz = vy.size();
  auto it_block_list = BlockList()->begin();
  Eigen::VectorXd::const_iterator ivy = vy.begin();
  for (int num = 0; num < sz; ++num) {
    it_block_list[num].setLLY(ivy[num]);
  }
}

void GPSimPL::PullBlockBackToRegion() {
  int sz = vx.size();
  auto it_block_list = BlockList()->begin();
  Eigen::VectorXd::iterator ivx = vx.begin();
  Eigen::VectorXd::iterator ivy = vy.begin();

  double blk_hi_bound_x;
  double blk_hi_bound_y;

  for (int num = 0; num < sz; ++num) {
    if (it_block_list[num].IsMovable()) {
      if (ivx[num] < RegionLeft()) {
        ivx[num] = RegionLeft();
      }
      blk_hi_bound_x = RegionRight() - it_block_list[num].Width();
      if (ivx[num] > blk_hi_bound_x) {
        ivx[num] = blk_hi_bound_x;
      }

      if (ivy[num] < RegionBottom()) {
        ivy[num] = RegionBottom();
      }
      blk_hi_bound_y = RegionTop() - it_block_list[num].Height();
      if (ivy[num] > blk_hi_bound_y) {
        ivy[num] = blk_hi_bound_y;
      }
    }
  }

  for (int num = 0; num < sz; ++num) {
    it_block_list[num].SetLoc(ivx[num], ivy[num]);
  }
}

void GPSimPL::InitialPlacement() {
  double cpu_time = get_cpu_time();

  std::vector<Block> &block_list = *BlockList();

  HPWLX_converge = false;
  HPWLX_old = DBL_MAX;
  for (size_t i = 0; i < block_list.size(); ++i) {
    vx[i] = block_list[i].LLX();
  }
  for (int i = 0; i < b2b_update_max_iteration; ++i) {
    BuildProblemB2BX();
    SolveProblemX();
    UpdateCGFlagsX();
    if (HPWLX_converge && i >= 10) {
      if (globalVerboseLevel >= LOG_DEBUG) {
        std::cout << "iterations x:     " << i << "\n";
      }
      break;
    }
  }

  HPWLY_converge = false;
  HPWLY_old = DBL_MAX;
  for (size_t i = 0; i < block_list.size(); ++i) {
    vy[i] = block_list[i].LLY();
  }
  for (int i = 0; i < b2b_update_max_iteration; ++i) {
    BuildProblemB2BY(); // fill A and b
    SolveProblemY();// Solving:
    UpdateCGFlagsY();
    if (HPWLY_converge && i >= 10) {
      if (globalVerboseLevel >= LOG_DEBUG) {
        std::cout << "iterations y:     " << i << "\n";
      }
      break;
    }
  }

  PullBlockBackToRegion();

  cg_total_hpwl_ = HPWLX_new + HPWLY_new;

  if (globalVerboseLevel >= LOG_INFO) {
    std::cout << "Initial Placement Complete\n";
  }

  cpu_time = get_cpu_time() - cpu_time;
  tot_cg_time += cpu_time;
}

void GPSimPL::InitGridBins() {
  /****
   * This function initialize the grid bin matrix, each bin has an area which can accommodate around number_of_cell_in_bin # of cells
   * Part1
   * grid_bin_height and grid_bin_width is determined by the following formula:
   *    grid_bin_height = sqrt(number_of_cell_in_bin * average_area / filling_rate)
   * the number of bins in the y-direction is given by:
   *    grid_cnt_y = (Top() - Bottom())/grid_bin_height
   *    grid_cnt_x = (Right() - Left())/grid_bin_width
   * And initialize the space of grid_bin_matrix
   *
   * Part2
   * for each grid bin, we need to initialize the attributes,
   * including index, boundaries, area, and potential available white space
   * the adjacent bin list is cached for the convenience of overfilled bin clustering
   *
   * Part3
   * check whether the grid bin is occupied by fixed blocks, and we only do this once, and cache this result for future usage
   * Assumption: fixed blocks do not overlap with each other!!!!!!!
   * we will check whether this grid bin is covered by a fixed block,
   * if yes, we set the label of this grid bin "all_terminal" to be true, initially, this value is false
   * if not, we deduce the white space by the amount of fixed block and grid bin overlap region
   * because a grid bin might be covered by more than one fixed blocks
   * if the final white space is 0, we know the grid bin is also all covered by fixed blocks
   * and we set the flag "all_terminal" to true.
   * ****/

  // Part1
  grid_bin_height = int(std::round(std::sqrt(number_of_cell_in_bin * GetCircuit()->AveMovBlkArea() / FillingRate())));
  grid_bin_width = grid_bin_height;
  grid_cnt_y = std::ceil(double(RegionTop() - RegionBottom()) / grid_bin_height);
  grid_cnt_x = std::ceil(double(RegionRight() - RegionLeft()) / grid_bin_width);
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "Global placement bin width, height: " << grid_bin_width << "  " << grid_bin_height << "\n";
  }

  std::vector<GridBin> temp_grid_bin_column(grid_cnt_y);
  grid_bin_matrix.resize(grid_cnt_x, temp_grid_bin_column);

  // Part2
  for (int i = 0; i < grid_cnt_x; i++) {
    for (int j = 0; j < grid_cnt_y; j++) {
      grid_bin_matrix[i][j].index = {i, j};
      grid_bin_matrix[i][j].bottom = RegionBottom() + j * grid_bin_height;
      grid_bin_matrix[i][j].top = RegionBottom() + (j + 1) * grid_bin_height;
      grid_bin_matrix[i][j].left = RegionLeft() + i * grid_bin_width;
      grid_bin_matrix[i][j].right = RegionLeft() + (i + 1) * grid_bin_width;
      grid_bin_matrix[i][j].white_space = grid_bin_matrix[i][j].Area();
      // at the very beginning, assuming the white space is the same as area
      grid_bin_matrix[i][j].create_adjacent_bin_list(grid_cnt_x, grid_cnt_y);
    }
  }

  // make sure the top placement boundary is the same as the top of the topmost bins
  for (int i = 0; i < grid_cnt_x; ++i) {
    grid_bin_matrix[i][grid_cnt_y - 1].top = RegionTop();
    grid_bin_matrix[i][grid_cnt_y - 1].white_space = grid_bin_matrix[i][grid_cnt_y - 1].Area();
  }
  // make sure the right placement boundary is the same as the right of the rightmost bins
  for (int i = 0; i < grid_cnt_y; ++i) {
    grid_bin_matrix[grid_cnt_x - 1][i].right = RegionRight();
    grid_bin_matrix[grid_cnt_x - 1][i].white_space = grid_bin_matrix[grid_cnt_x - 1][i].Area();
  }


  // Part3
  auto block_list = BlockList()->begin();
  int sz = int(BlockList()->size());
  int min_urx, max_llx, min_ury, max_lly;
  bool all_terminal, fixed_blk_out_of_region, blk_out_of_bin;
  int left_index, right_index, bottom_index, top_index;
  for (int i = 0; i < sz; i++) {
    /* find the left, right, bottom, top index of the grid */
    if (block_list[i].IsMovable()) continue;
    fixed_blk_out_of_region = int(block_list[i].LLX()) >= RegionRight() ||
        int(block_list[i].URX()) <= RegionLeft() ||
        int(block_list[i].LLY()) >= RegionTop() ||
        int(block_list[i].URY()) <= RegionBottom();
    if (fixed_blk_out_of_region) continue;
    left_index = (int) std::floor((block_list[i].LLX() - RegionLeft()) / grid_bin_width);
    right_index = (int) std::floor((block_list[i].URX() - RegionLeft()) / grid_bin_width);
    bottom_index = (int) std::floor((block_list[i].LLY() - RegionBottom()) / grid_bin_height);
    top_index = (int) std::floor((block_list[i].URY() - RegionBottom()) / grid_bin_height);
    /* the grid boundaries might be the placement region boundaries
     * if a block touches the rightmost and topmost boundaries, the index need to be fixed
     * to make sure no memory access out of scope */
    if (left_index < 0) left_index = 0;
    if (right_index >= grid_cnt_x) right_index = grid_cnt_x - 1;
    if (bottom_index < 0) bottom_index = 0;
    if (top_index >= grid_cnt_y) top_index = grid_cnt_y - 1;

    /* for each terminal, we will check which grid is inside it, and directly set the all_terminal attribute to true for that grid
     * some small terminals might occupy the same grid, we need to deduct the overlap area from the white space of that grid bin
     * when the final white space is 0, we know this grid bin is occupied by several terminals*/
    for (int j = left_index; j <= right_index; ++j) {
      for (int k = bottom_index; k <= top_index; ++k) {
        /* the following case might happen:
         * the top/right of a fixed block overlap with the bottom/left of a grid box
         * if this case happens, we need to ignore this fixed block for this grid box. */
        blk_out_of_bin = int(block_list[i].LLX() >= grid_bin_matrix[j][k].right) ||
            int(block_list[i].URX() <= grid_bin_matrix[j][k].left) ||
            int(block_list[i].LLY() >= grid_bin_matrix[j][k].top) ||
            int(block_list[i].URY() <= grid_bin_matrix[j][k].bottom);
        if (blk_out_of_bin) continue;
        grid_bin_matrix[j][k].terminal_list.push_back(i);

        // if grid bin is covered by a large fixed block, then all_terminal flag for this block is set to be true
        all_terminal = int(block_list[i].LLX() <= grid_bin_matrix[j][k].LLX()) &&
            int(block_list[i].LLY() <= grid_bin_matrix[j][k].LLY()) &&
            int(block_list[i].URX() >= grid_bin_matrix[j][k].URX()) &&
            int(block_list[i].URY() >= grid_bin_matrix[j][k].URY());
        grid_bin_matrix[j][k].all_terminal = all_terminal;
        // if all_terminal flag is false, we need to calculate the overlap of grid bin and this fixed block to get the white space,
        // when white space is less than 1, this grid bin is also all_terminal
        if (all_terminal) {
          grid_bin_matrix[j][k].white_space = 0;
        } else {
          // this part calculate the overlap of two rectangles
          max_llx = std::max(int(block_list[i].LLX()), grid_bin_matrix[j][k].LLX());
          max_lly = std::max(int(block_list[i].LLY()), grid_bin_matrix[j][k].LLY());
          min_urx = std::min(int(block_list[i].URX()), grid_bin_matrix[j][k].URX());
          min_ury = std::min(int(block_list[i].URY()), grid_bin_matrix[j][k].URY());
          grid_bin_matrix[j][k].white_space -= (unsigned long int) (min_urx - max_llx) * (min_ury - max_lly);
          if (grid_bin_matrix[j][k].white_space < 1) {
            grid_bin_matrix[j][k].all_terminal = true;
            grid_bin_matrix[j][k].white_space = 0;
          }
        }
      }
    }
  }
}

void GPSimPL::InitWhiteSpaceLUT() {
  /****
   * this is a member function to initialize white space look-up table
   * this table is a matrix, one way to calculate the white space in a region is to add all white space of every single grid bin in this region
   * an easier way is to define an accumulate function and store it as a look-up table
   * when we want to find the white space in a region, the value can be easily extracted from the look-up table
   * ****/

  // this for loop is created to initialize the size of the loop-up table
  std::vector<unsigned long int> tmp_vector(grid_cnt_y);
  grid_bin_white_space_LUT.resize(grid_cnt_x, tmp_vector);

  // this for loop is used for computing elements in the look-up table
  // there are four cases, element at (0,0), elements on the left edge, elements on the right edge, otherwise
  for (int kx = 0; kx < grid_cnt_x; ++kx) {
    for (int ky = 0; ky < grid_cnt_y; ++ky) {
      grid_bin_white_space_LUT[kx][ky] = 0;
      if (kx == 0) {
        if (ky == 0) {
          grid_bin_white_space_LUT[kx][ky] = grid_bin_matrix[0][0].white_space;
        } else {
          grid_bin_white_space_LUT[kx][ky] = grid_bin_white_space_LUT[kx][ky - 1] + grid_bin_matrix[kx][ky].white_space;
        }
      } else {
        if (ky == 0) {
          grid_bin_white_space_LUT[kx][ky] = grid_bin_white_space_LUT[kx - 1][ky] + grid_bin_matrix[kx][ky].white_space;
        } else {
          grid_bin_white_space_LUT[kx][ky] = grid_bin_white_space_LUT[kx - 1][ky] + grid_bin_white_space_LUT[kx][ky - 1]
              + grid_bin_matrix[kx][ky].white_space - grid_bin_white_space_LUT[kx - 1][ky - 1];
        }
      }
    }
  }
}

void GPSimPL::LookAheadLgInit() {
  InitGridBins();
  InitWhiteSpaceLUT();
}

void GPSimPL::LookAheadClose() {
  grid_bin_matrix.clear();
  grid_bin_white_space_LUT.clear();
}

void GPSimPL::ClearGridBinFlag() {
  for (auto &bin_column: grid_bin_matrix) {
    for (auto &bin: bin_column) bin.global_placed = false;
  }
}

unsigned long int GPSimPL::LookUpWhiteSpace(GridBinIndex const &ll_index, GridBinIndex const &ur_index) {
  /****
   * this function is used to return the white space in a region specified by ll_index, and ur_index
   * there are four cases, element at (0,0), elements on the left edge, elements on the right edge, otherwise
   * ****/

  unsigned long int total_white_space;
  /*if (ll_index.x == 0) {
    if (ll_index.y == 0) {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y];
    } else {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y]
          - grid_bin_white_space_LUT[ur_index.x][ll_index.y-1];
    }
  } else {
    if (ll_index.y == 0) {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y]
          - grid_bin_white_space_LUT[ll_index.x-1][ur_index.y];
    } else {
      total_white_space = grid_bin_white_space_LUT[ur_index.x][ur_index.y]
          - grid_bin_white_space_LUT[ur_index.x][ll_index.y-1]
          - grid_bin_white_space_LUT[ll_index.x-1][ur_index.y]
          + grid_bin_white_space_LUT[ll_index.x-1][ll_index.y-1];
    }
  }*/
  WindowQuadruple window = {ll_index.x, ll_index.y, ur_index.x, ur_index.y};
  total_white_space = LookUpWhiteSpace(window);
  return total_white_space;
}

unsigned long int GPSimPL::LookUpWhiteSpace(WindowQuadruple &window) {
  unsigned long int total_white_space;
  if (window.llx == 0) {
    if (window.lly == 0) {
      total_white_space = grid_bin_white_space_LUT[window.urx][window.ury];
    } else {
      total_white_space = grid_bin_white_space_LUT[window.urx][window.ury]
          - grid_bin_white_space_LUT[window.urx][window.lly - 1];
    }
  } else {
    if (window.lly == 0) {
      total_white_space = grid_bin_white_space_LUT[window.urx][window.ury]
          - grid_bin_white_space_LUT[window.llx - 1][window.ury];
    } else {
      total_white_space = grid_bin_white_space_LUT[window.urx][window.ury]
          - grid_bin_white_space_LUT[window.urx][window.lly - 1]
          - grid_bin_white_space_LUT[window.llx - 1][window.ury]
          + grid_bin_white_space_LUT[window.llx - 1][window.lly - 1];
    }
  }
  return total_white_space;
}

unsigned long int GPSimPL::LookUpBlkArea(WindowQuadruple &window) {
  unsigned long int res = 0;
  for (int x = window.llx; x <= window.urx; ++x) {
    for (int y = window.lly; y <= window.ury; ++y) {
      res += grid_bin_matrix[x][y].cell_area;
    }
  }
  return res;
}

unsigned long int GPSimPL::WindowArea(WindowQuadruple &window) {
  unsigned long int res = 0;
  if (window.urx == grid_cnt_x - 1) {
    if (window.ury == grid_cnt_y - 1) {
      res = (window.urx - window.llx) * (window.ury - window.lly) * grid_bin_width * grid_bin_height;
      res += (window.urx - window.llx) * grid_bin_width * grid_bin_matrix[window.urx][window.ury].Height();
      res += (window.ury - window.lly) * grid_bin_height * grid_bin_matrix[window.urx][window.ury].Width();
      res += grid_bin_matrix[window.urx][window.ury].Area();
    } else {
      res = (window.urx - window.llx) * (window.ury - window.lly + 1) * grid_bin_width * grid_bin_height;
      res += (window.ury - window.lly + 1) * grid_bin_height * grid_bin_matrix[window.urx][window.ury].Width();
    }
  } else {
    if (window.ury == grid_cnt_y - 1) {
      res = (window.urx - window.llx + 1) * (window.ury - window.lly) * grid_bin_width * grid_bin_height;
      res += (window.urx - window.llx + 1) * grid_bin_width * grid_bin_matrix[window.urx][window.ury].Height();
    } else {
      res = (window.urx - window.llx + 1) * (window.ury - window.lly + 1) * grid_bin_width * grid_bin_height;
    }
  }
  /*
  for (int i=window.llx; i<=window.urx; ++i) {
    for (int j=window.lly; j<=window.ury; ++j) {
      res += grid_bin_matrix[i][j].Area();
    }
  }*/
  return res;
}

void GPSimPL::UpdateGridBinState() {
  /****
   * this is a member function to update grid bin status, because the cell_list, cell_area and over_fill state can be changed
   * so we need to update them when necessary
   * ****/

  // clean the old data
  for (auto &bin_column:grid_bin_matrix) {
    for (auto &bin:bin_column) {
      bin.cell_list.clear();
      bin.cell_area = 0;
      bin.over_fill = false;
    }
  }

  // for each cell, find the index of the grid bin it should be in
  // note that in extreme cases, the index might be smaller than 0 or larger than the maximum allowed index
  // because the cell is on the boundaries, so we need to make some modifications for these extreme cases
  auto block_list = BlockList()->begin();
  int sz = int(BlockList()->size());
  int x_index, y_index;
  for (int i = 0; i < sz; i++) {
    if (block_list[i].IsFixed()) continue;
    x_index = (int) std::floor((block_list[i].X() - RegionLeft()) / grid_bin_width);
    y_index = (int) std::floor((block_list[i].Y() - RegionBottom()) / grid_bin_height);
    if (x_index < 0) x_index = 0;
    if (x_index > grid_cnt_x - 1) x_index = grid_cnt_x - 1;
    if (y_index < 0) y_index = 0;
    if (y_index > grid_cnt_y - 1) y_index = grid_cnt_y - 1;
    grid_bin_matrix[x_index][y_index].cell_list.push_back(i);
    grid_bin_matrix[x_index][y_index].cell_area += block_list[i].Area();
  }

  /**** below is the criterion to decide whether a grid bin is over_filled or not
   * 1. if this bin if fully occupied by fixed blocks, but its cell_list is non-empty, which means there is some cells overlap with this grid bin, we say it is over_fill
   * 2. if not fully occupied by terminals, but filling_rate is larger than the TARGET_FILLING_RATE, then set is to over_fill
   * 3. if this bin is not overfilled, but cells in this bin overlaps with fixed blocks in this bin, we also mark it as over_fill
   * ****/
  //TODO: the third criterion might be changed in the next
  bool over_fill = false;
  for (auto &bin_column: grid_bin_matrix) {
    for (auto &bin: bin_column) {
      if (bin.global_placed) {
        bin.over_fill = false;
        continue;
      }
      if (bin.IsAllFixedBlk()) {
        if (!bin.cell_list.empty()) bin.over_fill = true;
      } else {
        bin.filling_rate = double(bin.cell_area) / double(bin.white_space);
        if (bin.filling_rate > FillingRate()) bin.over_fill = true;
      }
      if (!bin.OverFill()) {
        for (auto &cell_num: bin.cell_list) {
          for (auto &terminal_num: bin.terminal_list) {
            over_fill = block_list[cell_num].IsOverlap(block_list[terminal_num]);
            if (over_fill) {
              bin.over_fill = true;
              break;
            }
          }
          if (over_fill) break;
          // two breaks have to be used to break two loops
        }
      }
    }
  }
}

void GPSimPL::ClusterOverfilledGridBin() {
  /****
   * this function is to cluster overfilled grid bins, and sort them based on total cell area
   * the algorithm to cluster overfilled grid bins is breadth first search
   * ****/
  cluster_list.clear();
  int m = (int) grid_bin_matrix.size(); // number of rows
  int n = (int) grid_bin_matrix[0].size(); // number of columns

  for (int i = 0; i < m; ++i) {
    for (int j = 0; j < n; ++j) grid_bin_matrix[i][j].cluster_visited = false;
  }
  int cnt;
  for (int i = 0; i < m; i++) {
    for (int j = 0; j < n; j++) {
      if (grid_bin_matrix[i][j].cluster_visited || !grid_bin_matrix[i][j].over_fill) continue;
      GridBinIndex b(i, j);
      GridBinCluster H;
      H.bin_set.insert(b);
      grid_bin_matrix[i][j].cluster_visited = true;
      cnt = 0;
      std::queue<GridBinIndex> Q;
      Q.push(b);
      while (!Q.empty()) {
        b = Q.front();
        Q.pop();
        for (auto &index: grid_bin_matrix[b.x][b.y].adjacent_bin_index) {
          GridBin &bin = grid_bin_matrix[index.x][index.y];
          if (!bin.cluster_visited && bin.over_fill) {
            if (cnt > 3) {
              cluster_list.push_back(H);
              break;
            }
            bin.cluster_visited = true;
            H.bin_set.insert(index);
            ++cnt;
            Q.push(index);
          }
        }
      }
      cluster_list.push_back(H);
    }
  }
}

void GPSimPL::UpdateClusterArea() {
  /****
   * Calculate the total cell area and total white space in each cluster
   * ****/
  for (auto &cluster: cluster_list) {
    cluster.total_cell_area = 0;
    cluster.total_white_space = 0;
    for (auto &index: cluster.bin_set) {
      cluster.total_cell_area += grid_bin_matrix[index.x][index.y].cell_area;
      cluster.total_white_space += grid_bin_matrix[index.x][index.y].white_space;
    }
  }
}

void GPSimPL::UpdateClusterList() {
  ClusterOverfilledGridBin();
  UpdateClusterArea();
}

void GPSimPL::FindMinimumBoxForLargestCluster() {
  /****
   * this function find the box for the largest cluster,
   * such that the total white space in the box is larger than the total cell area
   * the way to do this is just by expanding the boundaries of the bounding box of the first cluster
   *
   * Part 1
   * find the index of the maximum cluster
   * ****/

  // clear the queue_box_bin
  while (!queue_box_bin.empty()) queue_box_bin.pop();
  if (cluster_list.empty()) return;

  // Part 1
  int max_cluster = 0;
  int list_sz = cluster_list.size();
  for (int i = 0; i < list_sz; ++i) {
    if (cluster_list[i].total_cell_area > cluster_list[max_cluster].total_cell_area) {
      max_cluster = i;
    }
  }

  std::vector<Block> &block_list = *BlockList();

  BoxBin R;
  R.cut_direction_x = false;

  R.ll_index.x = grid_cnt_x - 1;
  R.ll_index.y = grid_cnt_y - 1;
  R.ur_index.x = 0;
  R.ur_index.y = 0;
  // initialize a box with y cut-direction
  // identify the bounding box of the initial cluster
  for (auto &index: cluster_list[max_cluster].bin_set) {
    R.ll_index.x = std::min(R.ll_index.x, index.x);
    R.ur_index.x = std::max(R.ur_index.x, index.x);
    R.ll_index.y = std::min(R.ll_index.y, index.y);
    R.ur_index.y = std::max(R.ur_index.y, index.y);
  }
  while (true) {
    // update cell area, white space, and thus filling rate to determine whether to expand this box or not
    R.total_white_space = LookUpWhiteSpace(R.ll_index, R.ur_index);
    R.UpdateCellAreaWhiteSpaceFillingRate(grid_bin_white_space_LUT, grid_bin_matrix);
    if (R.filling_rate > FillingRate()) {
      R.ExpandBox(grid_cnt_x, grid_cnt_y);
    } else {
      break;
    }
    //std::cout << R.total_white_space << "  " << R.filling_rate << "  " << FillingRate() << "\n";
  }

  R.total_white_space = LookUpWhiteSpace(R.ll_index, R.ur_index);
  R.UpdateCellAreaWhiteSpaceFillingRate(grid_bin_white_space_LUT, grid_bin_matrix);
  R.UpdateCellList(grid_bin_matrix);
  R.ll_point.x = grid_bin_matrix[R.ll_index.x][R.ll_index.y].left;
  R.ll_point.y = grid_bin_matrix[R.ll_index.x][R.ll_index.y].bottom;
  R.ur_point.x = grid_bin_matrix[R.ur_index.x][R.ur_index.y].right;
  R.ur_point.y = grid_bin_matrix[R.ur_index.x][R.ur_index.y].top;

  R.left = int(R.ll_point.x);
  R.bottom = int(R.ll_point.y);
  R.right = int(R.ur_point.x);
  R.top = int(R.ur_point.y);

  if (R.ll_index == R.ur_index) {
    R.UpdateFixedBlkList(grid_bin_matrix);
    if (R.IsContainFixedBlk()) {
      R.UpdateObsBoundary(block_list);
    }
  }
  queue_box_bin.push(R);
  //std::cout << "Bounding box total white space: " << queue_box_bin.front().total_white_space << "\n";
  //std::cout << "Bounding box total cell area: " << queue_box_bin.front().total_cell_area << "\n";

  for (int kx = R.ll_index.x; kx <= R.ur_index.x; ++kx) {
    for (int ky = R.ll_index.y; ky <= R.ur_index.y; ++ky) {
      grid_bin_matrix[kx][ky].global_placed = true;
    }
  }

}

void GPSimPL::SplitBox(BoxBin &box) {
  std::vector<Block> &block_list = *BlockList();
  bool flag_bisection_complete;
  int dominating_box_flag; // indicate whether there is a dominating BoxBin
  BoxBin box1, box2;
  box1.ll_index = box.ll_index;
  box2.ur_index = box.ur_index;
  // this part of code can be simplified, but after which the code might be unclear
  // cut-line along vertical direction
  if (box.cut_direction_x) {
    flag_bisection_complete = box.update_cut_index_white_space(grid_bin_white_space_LUT);
    if (flag_bisection_complete) {
      box1.cut_direction_x = false;
      box2.cut_direction_x = false;
      box1.ur_index = box.cut_ur_index;
      box2.ll_index = box.cut_ll_index;
    } else {
      // if bisection fail in one direction, do bisection in the other direction
      box.cut_direction_x = false;
      flag_bisection_complete = box.update_cut_index_white_space(grid_bin_white_space_LUT);
      if (flag_bisection_complete) {
        box1.cut_direction_x = false;
        box2.cut_direction_x = false;
        box1.ur_index = box.cut_ur_index;
        box2.ll_index = box.cut_ll_index;
      }
    }
  } else {
    // cut-line along horizontal direction
    flag_bisection_complete = box.update_cut_index_white_space(grid_bin_white_space_LUT);
    if (flag_bisection_complete) {
      box1.cut_direction_x = true;
      box2.cut_direction_x = true;
      box1.ur_index = box.cut_ur_index;
      box2.ll_index = box.cut_ll_index;
    } else {
      box.cut_direction_x = true;
      flag_bisection_complete = box.update_cut_index_white_space(grid_bin_white_space_LUT);
      if (flag_bisection_complete) {
        box1.cut_direction_x = true;
        box2.cut_direction_x = true;
        box1.ur_index = box.cut_ur_index;
        box2.ll_index = box.cut_ll_index;
      }
    }
  }
  box1.update_cell_area_white_space(grid_bin_matrix);
  box2.update_cell_area_white_space(grid_bin_matrix);
  //std::cout << box1.ll_index_ << box1.ur_index_ << "\n";
  //std::cout << box2.ll_index_ << box2.ur_index_ << "\n";
  //box1.update_all_terminal(grid_bin_matrix);
  //box2.update_all_terminal(grid_bin_matrix);
  // if the white space in one bin is dominating the other, ignore the smaller one
  dominating_box_flag = 0;
  if (double(box1.total_white_space) / double(box.total_white_space) <= 0.01) {
    dominating_box_flag = 1;
  }
  if (double(box2.total_white_space) / double(box.total_white_space) <= 0.01) {
    dominating_box_flag = 2;
  }

  box1.update_boundaries(grid_bin_matrix);
  box2.update_boundaries(grid_bin_matrix);

  if (dominating_box_flag == 0) {
    //std::cout << "cell list size: " << box.cell_list.size() << "\n";
    //box.update_cell_area(block_list);
    //std::cout << "total_cell_area: " << box.total_cell_area << "\n";
    box.update_cut_point_cell_list_low_high(block_list, box1.total_white_space, box2.total_white_space);
    box1.cell_list = box.cell_list_low;
    box2.cell_list = box.cell_list_high;
    box1.ll_point = box.ll_point;
    box2.ur_point = box.ur_point;
    box1.ur_point = box.cut_ur_point;
    box2.ll_point = box.cut_ll_point;
    box1.total_cell_area = box.total_cell_area_low;
    box2.total_cell_area = box.total_cell_area_high;

    if (box1.ll_index == box1.ur_index) {
      box1.UpdateFixedBlkList(grid_bin_matrix);
      box1.UpdateObsBoundary(block_list);
    }
    if (box2.ll_index == box2.ur_index) {
      box2.UpdateFixedBlkList(grid_bin_matrix);
      box2.UpdateObsBoundary(block_list);
    }

    /*if ((box1.left < LEFT) || (box1.bottom < BOTTOM)) {
      std::cout << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM << "\n";
      std::cout << box1.left << " " << box1.bottom << "\n";
    }
    if ((box2.left < LEFT) || (box2.bottom < BOTTOM)) {
      std::cout << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM << "\n";
      std::cout << box2.left << " " << box2.bottom << "\n";
    }*/

    queue_box_bin.push(box1);
    queue_box_bin.push(box2);
    //box1.write_box_boundary("first_bounding_box.txt", grid_bin_width, grid_bin_height, LEFT, BOTTOM);
    //box2.write_box_boundary("first_bounding_box.txt", grid_bin_width, grid_bin_height, LEFT, BOTTOM);
    //box1.write_cell_region("first_cell_bounding_box.txt");
    //box2.write_cell_region("first_cell_bounding_box.txt");
  } else if (dominating_box_flag == 1) {
    box2.ll_point = box.ll_point;
    box2.ur_point = box.ur_point;
    box2.cell_list = box.cell_list;
    box2.total_cell_area = box.total_cell_area;
    if (box2.ll_index == box2.ur_index) {
      box2.UpdateFixedBlkList(grid_bin_matrix);
      box2.UpdateObsBoundary(block_list);
    }

    /*if ((box2.left < LEFT) || (box2.bottom < BOTTOM)) {
      std::cout << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM << "\n";
      std::cout << box2.left << " " << box2.bottom << "\n";
    }*/

    queue_box_bin.push(box2);
    //box2.write_box_boundary("first_bounding_box.txt", grid_bin_width, grid_bin_height, LEFT, BOTTOM);
    //box2.write_cell_region("first_cell_bounding_box.txt");
  } else {
    box1.ll_point = box.ll_point;
    box1.ur_point = box.ur_point;
    box1.cell_list = box.cell_list;
    box1.total_cell_area = box.total_cell_area;
    if (box1.ll_index == box1.ur_index) {
      box1.UpdateFixedBlkList(grid_bin_matrix);
      box1.UpdateObsBoundary(block_list);
    }

    /*if ((box1.left < LEFT) || (box1.bottom < BOTTOM)) {
      std::cout << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM << "\n";
      std::cout << box1.left << " " << box1.bottom << "\n";
    }*/

    queue_box_bin.push(box1);
    //box1.write_box_boundary("first_bounding_box.txt", grid_bin_width, grid_bin_height, LEFT, BOTTOM);
    //box1.write_cell_region("first_cell_bounding_box.txt");
  }
}

void GPSimPL::SplitGridBox(BoxBin &box) {
  std::vector<Block> &block_list = *BlockList();
  BoxBin box1, box2;
  box1.left = box.left;
  box1.bottom = box.bottom;
  box2.right = box.right;
  box2.top = box.top;
  box1.ll_index = box.ll_index;
  box1.ur_index = box.ur_index;
  box2.ll_index = box.ll_index;
  box2.ur_index = box.ur_index;
  /* split along the direction with more boundary lines */
  if (box.horizontal_obstacle_boundaries.size() > box.vertical_obstacle_boundaries.size()) {
    box.cut_direction_x = true;
    box1.right = box.right;
    box1.top = box.horizontal_obstacle_boundaries[0];
    box2.left = box.left;
    box2.bottom = box.horizontal_obstacle_boundaries[0];
    box1.update_terminal_list_white_space(block_list, box.terminal_list);
    box2.update_terminal_list_white_space(block_list, box.terminal_list);

    if (double(box1.total_white_space) / (double) box.total_white_space <= 0.01) {
      box2.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box2.cell_list = box.cell_list;
      box2.total_cell_area = box.total_cell_area;
      box2.UpdateObsBoundary(block_list);
      queue_box_bin.push(box2);
    } else if (double(box2.total_white_space) / (double) box.total_white_space <= 0.01) {
      box1.ll_point = box.ll_point;
      box1.ur_point = box.ur_point;
      box1.cell_list = box.cell_list;
      box1.total_cell_area = box.total_cell_area;
      box1.UpdateObsBoundary(block_list);
      queue_box_bin.push(box1);
    } else {
      box.update_cut_point_cell_list_low_high(block_list, box1.total_white_space, box2.total_white_space);
      box1.cell_list = box.cell_list_low;
      box2.cell_list = box.cell_list_high;
      box1.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box1.ur_point = box.cut_ur_point;
      box2.ll_point = box.cut_ll_point;
      box1.total_cell_area = box.total_cell_area_low;
      box2.total_cell_area = box.total_cell_area_high;
      box1.UpdateObsBoundary(block_list);
      box2.UpdateObsBoundary(block_list);
      queue_box_bin.push(box1);
      queue_box_bin.push(box2);
    }
  } else {
    box.cut_direction_x = false;
    box1.right = box.vertical_obstacle_boundaries[0];
    box1.top = box.top;
    box2.left = box.vertical_obstacle_boundaries[0];
    box2.bottom = box.bottom;
    box1.update_terminal_list_white_space(block_list, box.terminal_list);
    box2.update_terminal_list_white_space(block_list, box.terminal_list);

    if (double(box1.total_white_space) / (double) box.total_white_space <= 0.01) {
      box2.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box2.cell_list = box.cell_list;
      box2.total_cell_area = box.total_cell_area;
      box2.UpdateObsBoundary(block_list);
      queue_box_bin.push(box2);
    } else if (double(box2.total_white_space) / (double) box.total_white_space <= 0.01) {
      box1.ll_point = box.ll_point;
      box1.ur_point = box.ur_point;
      box1.cell_list = box.cell_list;
      box1.total_cell_area = box.total_cell_area;
      box1.UpdateObsBoundary(block_list);
      queue_box_bin.push(box1);
    } else {
      box.update_cut_point_cell_list_low_high(block_list, box1.total_white_space, box2.total_white_space);
      box1.cell_list = box.cell_list_low;
      box2.cell_list = box.cell_list_high;
      box1.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box1.ur_point = box.cut_ur_point;
      box2.ll_point = box.cut_ll_point;
      box1.total_cell_area = box.total_cell_area_low;
      box2.total_cell_area = box.total_cell_area_high;
      box1.UpdateObsBoundary(block_list);
      box2.UpdateObsBoundary(block_list);
      queue_box_bin.push(box1);
      queue_box_bin.push(box2);
    }
  }
}

void GPSimPL::PlaceBlkInBox(BoxBin &box) {
  std::vector<Block> &block_list = *BlockList();
  /* this is the simplest version, just linearly move cells in the cell_box to the grid box
  * non-linearity is not considered yet*/

  /*double cell_box_left, cell_box_bottom;
  double cell_box_width, cell_box_height;
  cell_box_left = box.ll_point.x;
  cell_box_bottom = box.ll_point.y;
  cell_box_width = box.ur_point.x - cell_box_left;
  cell_box_height = box.ur_point.y - cell_box_bottom;
  Block *cell;

  for (auto &cell_id: box.cell_list) {
    cell = &block_list[cell_id];
    cell->SetCenterX((cell->X() - cell_box_left)/cell_box_width * (box.right - box.left) + box.left);
    cell->SetCenterY((cell->Y() - cell_box_bottom)/cell_box_height * (box.top - box.bottom) + box.bottom);
  }*/

  int sz = box.cell_list.size();
  std::vector<std::pair<int, double>> index_loc_list_x(sz);
  std::vector<std::pair<int, double>> index_loc_list_y(sz);
  for (int i = 0; i < sz; ++i) {
    index_loc_list_x[i].first = box.cell_list[i];
    index_loc_list_x[i].second = block_list[box.cell_list[i]].X();
    index_loc_list_y[i].first = box.cell_list[i];
    index_loc_list_y[i].second = block_list[box.cell_list[i]].Y();
  }

  std::sort(index_loc_list_x.begin(),
            index_loc_list_x.end(),
            [](const std::pair<int, double> &p1, const std::pair<int, double> &p2) {
              return p1.second < p2.second;
            });
  double total_length = 0;
  for (auto &cell_id: box.cell_list) {
    total_length += block_list[cell_id].Width();
  }
  double cur_pos = 0;
  int box_width = box.right - box.left;
  int cell_num;
  for (auto &pair: index_loc_list_x) {
    cell_num = pair.first;
    block_list[cell_num].setCenterX(box.left + cur_pos / total_length * box_width);
    cur_pos += block_list[cell_num].Width();
  }

  std::sort(index_loc_list_y.begin(),
            index_loc_list_y.end(),
            [](const std::pair<int, double> &p1, const std::pair<int, double> &p2) {
              return p1.second < p2.second;
            });
  total_length = 0;
  for (auto &cell_id: box.cell_list) {
    total_length += block_list[cell_id].Height();
  }
  cur_pos = 0;
  int box_height = box.top - box.bottom;
  for (auto &pair: index_loc_list_y) {
    cell_num = pair.first;
    block_list[cell_num].setCenterY(box.bottom + cur_pos / total_length * box_height);
    cur_pos += block_list[cell_num].Height();
  }
}

void GPSimPL::RoughLegalBlkInBox(BoxBin &box) {
  int sz = box.cell_list.size();
  if (sz == 0) return;
  std::vector<int> row_start;
  row_start.assign(box.top - box.bottom + 1, box.left);

  std::vector<IndexLocPair<int>> index_loc_list;
  IndexLocPair<int> tmp_index_loc_pair(0, 0, 0);
  index_loc_list.resize(sz, tmp_index_loc_pair);

  std::vector<Block> &block_list = *BlockList();
  int blk_num;
  for (int i = 0; i < sz; ++i) {
    blk_num = box.cell_list[i];
    index_loc_list[i].num = blk_num;
    index_loc_list[i].x = block_list[blk_num].LLX();
    index_loc_list[i].y = block_list[blk_num].LLY();
  }
  std::sort(index_loc_list.begin(), index_loc_list.end());

  int init_x;
  int init_y;
  int height;
  int width;
  int start_row;
  int end_row;
  int best_row;
  int best_loc;
  double min_cost;
  double tmp_cost;
  int tmp_end_row;
  int tmp_x;
  int tmp_y;

  double min_x = right_;
  double max_x = left_;

  for (auto &pair: index_loc_list) {
    auto &block = block_list[pair.num];

    init_x = int(block.LLX());
    init_y = int(block.LLY());

    height = int(block.Height());
    width = int(block.Width());
    start_row = std::max(0, init_y - box.bottom - 2 * height);
    end_row = std::min(box.top - box.bottom - height, init_y - box.bottom + 3 * height);

    best_row = 0;
    best_loc = INT_MIN;
    min_cost = DBL_MAX;

    for (int tmp_row = start_row; tmp_row <= end_row; ++tmp_row) {
      tmp_end_row = tmp_row + height - 1;
      tmp_x = std::max(box.left, init_x - 1 * width);
      for (int i = tmp_row; i <= tmp_end_row; ++i) {
        tmp_x = std::max(tmp_x, row_start[i]);
      }

      tmp_y = tmp_row + box.bottom;
      //double tmp_hpwl = EstimatedHPWL(block, tmp_x, tmp_y);

      tmp_cost = std::abs(tmp_x - init_x) + std::abs(tmp_y - init_y);
      if (tmp_cost < min_cost) {
        best_loc = tmp_x;
        best_row = tmp_row;
        min_cost = tmp_cost;
      }
    }

    int res_x = best_loc;
    int res_y = best_row + box.bottom;

    //std::cout << res_x << "  " << res_y << "  " << min_cost << "  " << block.Num() << "\n";

    block.SetLoc(res_x, res_y);

    min_x = std::min(min_x, res_x + width / 2.0);
    max_x = std::max(max_x, res_x + width / 2.0);

    start_row = int(block.LLY()) - box.bottom;
    end_row = start_row + int(block.Height()) - 1;

    int end_x = int(block.URX());
    for (int i = start_row; i <= end_row; ++i) {
      row_start[i] = end_x;
    }
  }

  double span_x = max_x - min_x;
  if (span_x >= 0 && span_x < 1e-8) return;
  double box_width = box.right - box.left;
  if (span_x < 0) {
    std::cout << span_x << "  " << index_loc_list.size() << "\n";
  }
  assert(span_x > 0);
  for (auto &pair: index_loc_list) {
    auto &block = block_list[pair.num];
    block.setCenterX((block.X() - min_x) / span_x * box_width + box.left);
  }

}

double GPSimPL::BlkOverlapArea(Block *node1, Block *node2) {
  bool not_overlap;
  not_overlap = ((node1->LLX() >= node2->URX()) || (node1->LLY() >= node2->URY()))
      || ((node2->LLX() >= node1->URX()) || (node2->LLY() >= node1->URY()));
  if (not_overlap) {
    return 0;
  } else {
    double node1_llx, node1_lly, node1_urx, node1_ury;
    double node2_llx, node2_lly, node2_urx, node2_ury;
    double max_llx, max_lly, min_urx, min_ury;
    double overlap_x, overlap_y, overlap_area;

    node1_llx = node1->LLX();
    node1_lly = node1->LLY();
    node1_urx = node1->URX();
    node1_ury = node1->URY();
    node2_llx = node2->LLX();
    node2_lly = node2->LLY();
    node2_urx = node2->URX();
    node2_ury = node2->URY();

    max_llx = std::max(node1_llx, node2_llx);
    max_lly = std::max(node1_lly, node2_lly);
    min_urx = std::min(node1_urx, node2_urx);
    min_ury = std::min(node1_ury, node2_ury);

    overlap_x = min_urx - max_llx;
    overlap_y = min_ury - max_lly;
    overlap_area = overlap_x * overlap_y;
    return overlap_area;
  }
}

void GPSimPL::PlaceBlkInBoxBisection(BoxBin &box) {
  std::vector<Block> &block_list = *BlockList();
  /* keep bisect a grid bin until the leaf bin has less than say 2 nodes? */
  size_t max_cell_num_in_box = 10;
  box.cut_direction_x = true;
  std::queue<BoxBin> box_Q;
  box_Q.push(box);
  while (!box_Q.empty()) {
    //std::cout << " Q.size: " << box_Q.size() << "\n";
    BoxBin &front_box = box_Q.front();
    if (front_box.cell_list.size() > max_cell_num_in_box) {
      // split box and push it to box_Q
      BoxBin box1, box2;
      box1.ur_index = front_box.ur_index;
      box2.ll_index = front_box.ll_index;
      box1.bottom = front_box.bottom;
      box1.left = front_box.left;
      box2.top = front_box.top;
      box2.right = front_box.right;

      int ave_blk_height = std::ceil(GetCircuit()->AveMovBlkHeight());
      //std::cout << "Average block height: " << ave_blk_height << "  " << GetCircuit()->AveMovBlkHeight() << "\n";
      front_box.cut_direction_x = (front_box.top - front_box.bottom > ave_blk_height);
      int cut_line_w = 0; // cut-line for White space
      front_box.update_cut_point_cell_list_low_high_leaf(block_list, cut_line_w, ave_blk_height);
      if (front_box.cut_direction_x) {
        box1.top = cut_line_w;
        box1.right = front_box.right;

        box2.bottom = cut_line_w;
        box2.left = front_box.left;
      } else {
        box1.top = front_box.top;
        box1.right = cut_line_w;

        box2.bottom = front_box.bottom;
        box2.left = cut_line_w;
      }
      box1.cell_list = front_box.cell_list_low;
      box2.cell_list = front_box.cell_list_high;
      box1.ll_point = front_box.ll_point;
      box2.ur_point = front_box.ur_point;
      box1.ur_point = front_box.cut_ur_point;
      box2.ll_point = front_box.cut_ll_point;
      box1.total_cell_area = front_box.total_cell_area_low;
      box2.total_cell_area = front_box.total_cell_area_high;

      if (!box1.cell_list.empty()) {
        box_Q.push(box1);
      }
      if (!box2.cell_list.empty()) {
        box_Q.push(box2);
      }
    } else {
      // shift cells to the center of the final box
      if (max_cell_num_in_box == 1) {
        Block *cell;
        for (auto &cell_id: front_box.cell_list) {
          cell = &block_list[cell_id];
          cell->setCenterX((front_box.left + front_box.right) / 2.0);
          cell->setCenterY((front_box.bottom + front_box.top) / 2.0);
        }
      } else {
        PlaceBlkInBox(front_box);
        //RoughLegalBlkInBox(front_box);
      }
    }
    box_Q.pop();
  }
}

bool GPSimPL::RecursiveBisectionBlkSpreading() {
  /* keep splitting the biggest box to many small boxes, and keep update the shape of each box and cells should be assigned to the box */
  while (!queue_box_bin.empty()) {
    if (queue_box_bin.empty()) break;
    BoxBin &box = queue_box_bin.front();
    /* when the box is a grid bin box or a smaller box with no terminals inside, start moving cells to the box */
    /* if there is terminals inside a box, keep splitting it */
    if (box.ll_index == box.ur_index) {
      if (box.IsContainFixedBlk()) {
        SplitGridBox(box);
        queue_box_bin.pop();
        continue;
      }
      /* if no terminals in side a box, do cell placement inside the box */
      //PlaceBlkInBoxBisection(box);
      PlaceBlkInBox(box);
      //RoughLegalBlkInBox(box);
    } else {
      SplitBox(box);
    }
    queue_box_bin.pop();
  }
  return true;
}

void GPSimPL::BackUpBlkLoc() {
  std::vector<Block> &block_list = *BlockList();
  for (size_t i = 0; i < block_list.size(); ++i) {
    x_anchor[i] = block_list[i].LLX();
    y_anchor[i] = block_list[i].LLY();
  }
}

void GPSimPL::UpdateAnchorLoc() {
  auto block_list = BlockList()->begin();
  auto sz = BlockList()->size();
  auto it_x_anchor = x_anchor.begin();
  auto it_y_anchor = y_anchor.begin();

  //double tmp_value;
  for (size_t i = 0; i < sz; ++i) {
    //tmp_value = block_list[i].LLX();
    //block_list[i].SetLLX(it_x_anchor[i]);
    //it_x_anchor[i] = tmp_value;

    //tmp_value = block_list[i].LLY();
    //block_list[i].SetLLY(it_y_anchor[i]);
    //it_y_anchor[i] = tmp_value;

    it_x_anchor[i] = block_list[i].LLX();
    it_y_anchor[i] = block_list[i].LLY();
  }
}

void GPSimPL::BuildProblemB2BWithAnchorX() {
  BuildProblemB2BX();
  auto block_list = BlockList()->begin();
  auto sz = BlockList()->size();
  auto it_x_anchor = x_anchor.begin();
  auto it_bx = bx.begin();

  double weight = 0;
  double pin_loc0, pin_loc1;
  for (size_t i = 0; i < sz; ++i) {
    if (block_list[i].IsFixed()) continue;
    pin_loc0 = block_list[i].LLX();
    pin_loc1 = it_x_anchor[i];
    /*if (std::fabs(pin_loc0 - pin_loc1) < 10) {
      weight = alpha;
    } else {
      weight = alpha / (std::fabs(pin_loc0 - pin_loc1) + WidthEpsilon());
    }*/
    weight = alpha / (std::fabs(pin_loc0 - pin_loc1) + WidthEpsilon());
    it_bx[i] += pin_loc1 * weight;
    coefficients.emplace_back(T(i, i, weight));
  }
  Ax.setFromTriplets(coefficients.begin(), coefficients.end());
}

void GPSimPL::BuildProblemB2BWithAnchorY() {
  BuildProblemB2BY();
  auto block_list = BlockList()->begin();
  auto sz = BlockList()->size();
  auto it_y_anchor = y_anchor.begin();
  auto it_by = by.begin();

  double weight = 0;
  double pin_loc0, pin_loc1;
  for (size_t i = 0; i < sz; ++i) {
    if (block_list[i].IsFixed()) continue;
    pin_loc0 = block_list[i].LLY();
    pin_loc1 = it_y_anchor[i];
    /*if (std::fabs(pin_loc0 - pin_loc1) < 10) {
      weight = alpha;
    } else {
      weight = alpha / (std::fabs(pin_loc0 - pin_loc1) + WidthEpsilon());
    }*/
    weight = alpha / (std::fabs(pin_loc0 - pin_loc1) + WidthEpsilon());
    it_by[i] += pin_loc1 * weight;
    coefficients.emplace_back(T(i, i, weight));
  }
  Ay.setFromTriplets(coefficients.begin(), coefficients.end());
}

void GPSimPL::QuadraticPlacementWithAnchor() {
  double cpu_time = get_cpu_time();

  if (globalVerboseLevel >= LOG_DEBUG) {
    std::cout << "alpha: " << alpha << "\n";
  }
  std::vector<Block> &block_list = *BlockList();

  UpdateAnchorLoc();
  UpdateAnchorNetWeight();

  HPWLX_converge = false;
  HPWLX_old = DBL_MAX;
  for (size_t i = 0; i < block_list.size(); ++i) {
    vx[i] = block_list[i].LLX();
  }
  UpdateMaxMinX();
  for (int i = 0; i < 50; ++i) {
    BuildProblemB2BWithAnchorX();
    SolveProblemX();
    UpdateCGFlagsX();
    if (HPWLX_converge && i >= 2) {
      if (globalVerboseLevel >= LOG_DEBUG) {
        std::cout << "iterations x:     " << i << "\n";
      }
      break;
    }
  }

  HPWLY_converge = false;
  HPWLY_old = DBL_MAX;
  for (size_t i = 0; i < block_list.size(); ++i) {
    vy[i] = block_list[i].LLY();
  }
  UpdateMaxMinY();
  for (int i = 0; i < 50; ++i) {
    BuildProblemB2BWithAnchorY();
    SolveProblemY();
    UpdateCGFlagsY();
    if (HPWLY_converge && i >= 2) {
      if (globalVerboseLevel >= LOG_DEBUG) {
        std::cout << "iterations y:     " << i << "\n";
      }
      break;
    }
  }

  cg_total_hpwl_ = HPWLX_new + HPWLY_new;

  PullBlockBackToRegion();

  if (globalVerboseLevel >= LOG_INFO) {
    std::cout << "Quadratic Placement With Anchor Complete\n";
  }

  cpu_time = get_cpu_time() - cpu_time;
  tot_cg_time += cpu_time;
}

void GPSimPL::LookAheadLegalization() {
  double cpu_time = get_cpu_time();

  //BackUpBlkLoc();
  ClearGridBinFlag();
  do {
    UpdateGridBinState();
    UpdateClusterList();
    FindMinimumBoxForLargestCluster();
    RecursiveBisectionBlkSpreading();
  } while (!cluster_list.empty());

  UpdateHPWLX();
  UpdateHPWLY();
  if (globalVerboseLevel >= LOG_INFO) {
    std::cout << "Look-ahead legalization complete\n";
  }

  cpu_time = get_cpu_time() - cpu_time;
  tot_lal_time += cpu_time;
}

void GPSimPL::CheckAndShift() {
  if (circuit_->TotFixedBlkCnt() > 0) return;
  /****
   * This method is useful, when a circuit does not have any fixed blocks.
   * In this case, the shift of the whole circuit does not influence HPWL and overlap.
   * But if the circuit is placed close to the right placement boundary, it give very few change to legalizer if cells close
   * to the right boundary need to find different locations.
   *
   * 1. Find the leftmost, rightmost, topmost, bottommost cell edges
   * 2. Calculate the total margin in x direction and y direction
   * 3. Evenly assign the margin to each side
   * ****/

  double left_most = INT_MAX;
  double right_most = INT_MIN;
  double bottom_most = INT_MAX;
  double top_most = INT_MIN;

  for (auto &blk: *BlockList()) {
    left_most = std::min(left_most, blk.LLX());
    right_most = std::max(right_most, blk.URX());
    bottom_most = std::min(bottom_most, blk.LLY());
    top_most = std::max(top_most, blk.URY());
  }

  double margin_x = (right_ - left_) - (right_most - left_most);
  double margin_y = (top_ - bottom_) - (top_most - bottom_most);

  double delta_x = left_ + margin_x / 10 - left_most;
  double delta_y = bottom_ + margin_y / 2 - bottom_most;

  for (auto &blk: *BlockList()) {
    blk.IncreaseX(delta_x);
    blk.IncreaseY(delta_y);
  }
}

void GPSimPL::UpdateLALConvergeState() {
  HPWL_LAL_new = HPWL();
  HPWL_LAL_converge = std::fabs(1 - HPWL_LAL_new / HPWL_LAL_old) < HPWL_inter_linearSolver_precision;
  if (globalVerboseLevel >= LOG_DEBUG) {
    std::cout << "Old HPWL after look ahead legalization: " << HPWL_LAL_old << "\n";
    std::cout << "New HPWL after look ahead legalization: " << HPWL_LAL_new << "\n";
    std::cout << "Converge is: " << HPWL_LAL_converge << "\n";
  }
  HPWL_LAL_old = HPWL_LAL_new;

  lal_total_hpwl_ = HPWL_LAL_new;
}

bool GPSimPL::StartPlacement() {
  double wall_time = get_wall_time();
  double cpu_time = get_cpu_time();
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "---------------------------------------\n"
              << "Start global placement\n";
  }
  SanityCheck();
  CGInit();
  LookAheadLgInit();
  //BlockLocCenterInit();
  BlockLocRandomInit();
  if (NetList()->empty()) {
    if (globalVerboseLevel >= LOG_CRITICAL) {
      std::cout << "\033[0;36m"
                << "Global Placement complete\n"
                << "\033[0m";
    }
    return true;
  }

  if (is_dump) DumpResult("rand_init.txt");

  InitialPlacement();

  if (is_dump) DumpResult("cg_result_0.txt");

  //std::cout << cg_total_hpwl_ << "  " << circuit_->HPWL() << "\n";

  for (cur_iter_ = 0; cur_iter_ < max_iter_; ++cur_iter_) {
    if (globalVerboseLevel >= LOG_DEBUG) {
      std::cout << cur_iter_ << "-th iteration\n";
    }
    LookAheadLegalization();
    if (is_dump) DumpResult("lal_result_" + std::to_string(cur_iter_) + ".txt");
    UpdateLALConvergeState();
    if (globalVerboseLevel >= LOG_CRITICAL) {
      printf("It %d: \t%e  %e\n", cur_iter_, cg_total_hpwl_, lal_total_hpwl_);
    }
    if (HPWL_LAL_converge) { // if HPWL converges
      if (cur_iter_ >= 80) {
        if (globalVerboseLevel >= LOG_CRITICAL) {
          std::cout << "Iterative look-ahead legalization complete" << std::endl;
          std::cout << "Total number of iteration: " << cur_iter_ + 1 << std::endl;
        }
        break;
      }
    }
    QuadraticPlacementWithAnchor();
    if (is_dump) DumpResult("cg_result_" + std::to_string(cur_iter_ + 1) + ".txt");
  }
  if (globalVerboseLevel >= LOG_CRITICAL) {
    std::cout << "\033[0;36m"
              << "Global Placement complete\n"
              << "\033[0m";
    printf("(cg time: %.4fs, lal time: %.4fs)\n", tot_cg_time, tot_lal_time);
  }
  LookAheadClose();
  //CheckAndShift();
  UpdateMovableBlkPlacementStatus();
  ReportHPWL(LOG_CRITICAL);

  wall_time = get_wall_time() - wall_time;
  cpu_time = get_cpu_time() - cpu_time;
  if (globalVerboseLevel >= LOG_CRITICAL) {
    printf("(wall time: %.4fs, cpu time: %.4fs)\n", wall_time, cpu_time);
  }
  ReportMemory(LOG_CRITICAL);

  return true;
}

void GPSimPL::DumpResult(std::string const &name_of_file) {
  UpdateGridBinState();
  static int counter = 0;
  //std::cout << "DumpNum:" << counter << "\n";
  circuit_->GenMATLABTable(name_of_file);
  //write_not_all_terminal_grid_bins("grid_bin_not_all_terminal" + std::to_string(counter) + ".txt");
  //write_overfill_grid_bins("grid_bin_overfill" + std::to_string(counter) + ".txt");
  ++counter;
}

void GPSimPL::DrawBlockNetList(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open input file " + name_of_file);
  ost << RegionLeft() << " " << RegionBottom() << " " << RegionRight() - RegionLeft() << " "
      << RegionTop() - RegionBottom() << "\n";
  std::vector<Block> &block_list = *BlockList();
  for (auto &block: block_list) {
    ost << block.LLX() << " " << block.LLY() << " " << block.Width() << " " << block.Height() << "\n";
  }
  ost.close();
}

void GPSimPL::write_all_terminal_grid_bins(std::string const &name_of_file) {
  /* this is a member function for testing, print grid bins where the flag "all_terminal" is true */
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file" + name_of_file);
  for (auto &bin_column: grid_bin_matrix) {
    for (auto &bin: bin_column) {
      double low_x, low_y, width, height;
      width = bin.right - bin.left;
      height = bin.top - bin.bottom;
      low_x = bin.left;
      low_y = bin.bottom;
      int N = 3;
      double step_x = width / N, step_y = height / N;
      if (bin.IsAllFixedBlk()) {
        for (int j = 0; j < N; j++) {
          ost << low_x << "\t" << low_y + j * step_y << "\n";
          ost << low_x + width << "\t" << low_y + j * step_y << "\n";
        }
        for (int j = 0; j < N; j++) {
          ost << low_x + j * step_x << "\t" << low_y << "\n";
          ost << low_x + j * step_x << "\t" << low_y + height << "\n";
        }
      }
    }
  }
  ost.close();
}

void GPSimPL::write_not_all_terminal_grid_bins(std::string const &name_of_file) {
  /* this is a member function for testing, print grid bins where the flag "all_terminal" is false */
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file" + name_of_file);
  for (auto &bin_column: grid_bin_matrix) {
    for (auto &bin: bin_column) {
      double low_x, low_y, width, height;
      width = bin.right - bin.left;
      height = bin.top - bin.bottom;
      low_x = bin.left;
      low_y = bin.bottom;
      int N = 3;
      double step_x = width / N, step_y = height / N;
      if (!bin.IsAllFixedBlk()) {
        for (int j = 0; j < N; j++) {
          ost << low_x << "\t" << low_y + j * step_y << "\n";
          ost << low_x + width << "\t" << low_y + j * step_y << "\n";
        }
        for (int j = 0; j < N; j++) {
          ost << low_x + j * step_x << "\t" << low_y << "\n";
          ost << low_x + j * step_x << "\t" << low_y + height << "\n";
        }
      }
    }
  }
  ost.close();
}

void GPSimPL::write_overfill_grid_bins(std::string const &name_of_file) {
  /* this is a member function for testing, print grid bins where the flag "over_fill" is true */
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file" + name_of_file);
  for (auto &bin_column: grid_bin_matrix) {
    for (auto &bin: bin_column) {
      double low_x, low_y, width, height;
      width = bin.right - bin.left;
      height = bin.top - bin.bottom;
      low_x = bin.left;
      low_y = bin.bottom;
      int N = 10;
      double step_x = width / N, step_y = height / N;
      if (bin.OverFill()) {
        for (int j = 0; j < N; j++) {
          ost << low_x << "\t" << low_y + j * step_y << "\n";
          ost << low_x + width << "\t" << low_y + j * step_y << "\n";
        }
        for (int j = 0; j < N; j++) {
          ost << low_x + j * step_x << "\t" << low_y << "\n";
          ost << low_x + j * step_x << "\t" << low_y + height << "\n";
        }
      }
    }
  }
  ost.close();
}

void GPSimPL::write_not_overfill_grid_bins(std::string const &name_of_file) {
  /* this is a member function for testing, print grid bins where the flag "over_fill" is false */
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file" + name_of_file);
  for (auto &bin_column: grid_bin_matrix) {
    for (auto &bin: bin_column) {
      double low_x, low_y, width, height;
      width = bin.right - bin.left;
      height = bin.top - bin.bottom;
      low_x = bin.left;
      low_y = bin.bottom;
      int N = 10;
      double step_x = width / N, step_y = height / N;
      if (!bin.OverFill()) {
        for (int j = 0; j < N; j++) {
          ost << low_x << "\t" << low_y + j * step_y << "\n";
          ost << low_x + width << "\t" << low_y + j * step_y << "\n";
        }
        for (int j = 0; j < N; j++) {
          ost << low_x + j * step_x << "\t" << low_y << "\n";
          ost << low_x + j * step_x << "\t" << low_y + height << "\n";
        }
      }
    }
  }
  ost.close();
}

void GPSimPL::write_first_n_bin_cluster(std::string const &name_of_file, size_t n) {
  /* this is a member function for testing, print the first n over_filled clusters */
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file" + name_of_file);
  for (size_t i = 0; i < n; i++) {
    for (auto &index: cluster_list[i].bin_set) {
      double low_x, low_y, width, height;
      GridBin *GridBin = &grid_bin_matrix[index.x][index.y];
      width = GridBin->right - GridBin->left;
      height = GridBin->top - GridBin->bottom;
      low_x = GridBin->left;
      low_y = GridBin->bottom;
      int step = 40;
      if (GridBin->OverFill()) {
        for (int j = 0; j < height; j += step) {
          ost << low_x << "\t" << low_y + j << "\n";
          ost << low_x + width << "\t" << low_y + j << "\n";
        }
        for (int j = 0; j < width; j += step) {
          ost << low_x + j << "\t" << low_y << "\n";
          ost << low_x + j << "\t" << low_y + height << "\n";
        }
      }
    }
  }
  ost.close();
}

void GPSimPL::write_first_bin_cluster(std::string const &name_of_file) {
  /* this is a member function for testing, print the first one over_filled clusters */
  write_first_n_bin_cluster(name_of_file, 1);
}

void GPSimPL::write_n_bin_cluster(std::string const &name_of_file, size_t n) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file" + name_of_file);
  for (auto &index: cluster_list[n].bin_set) {
    double low_x, low_y, width, height;
    GridBin *GridBin = &grid_bin_matrix[index.x][index.y];
    width = GridBin->right - GridBin->left;
    height = GridBin->top - GridBin->bottom;
    low_x = GridBin->left;
    low_y = GridBin->bottom;
    int step = 40;
    if (GridBin->OverFill()) {
      for (int j = 0; j < height; j += step) {
        ost << low_x << "\t" << low_y + j << "\n";
        ost << low_x + width << "\t" << low_y + j << "\n";
      }
      for (int j = 0; j < width; j += step) {
        ost << low_x + j << "\t" << low_y << "\n";
        ost << low_x + j << "\t" << low_y + height << "\n";
      }
    }
  }
  ost.close();
}

void GPSimPL::write_all_bin_cluster(const std::string &name_of_file) {
  /* this is a member function for testing, print all over_filled clusters */
  write_first_n_bin_cluster(name_of_file, cluster_list.size());
}

void GPSimPL::write_first_box(std::string const &name_of_file) {
  /* this is a member function for testing, print the first n over_filled clusters */
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file" + name_of_file);
  double low_x, low_y, width, height;
  BoxBin *R = &queue_box_bin.front();
  width = (R->ur_index.x - R->ll_index.x + 1) * grid_bin_width;
  height = (R->ur_index.y - R->ll_index.y + 1) * grid_bin_height;
  low_x = R->ll_index.x * grid_bin_width + RegionLeft();
  low_y = R->ll_index.y * grid_bin_height + RegionBottom();
  int step = 20;
  for (int j = 0; j < height; j += step) {
    ost << low_x << "\t" << low_y + j << "\n";
    ost << low_x + width << "\t" << low_y + j << "\n";
  }
  for (int j = 0; j < width; j += step) {
    ost << low_x + j << "\t" << low_y << "\n";
    ost << low_x + j << "\t" << low_y + height << "\n";
  }
  ost.close();
}

void GPSimPL::write_first_box_cell_bounding(std::string const &name_of_file) {
  /* this is a member function for testing, print the bounding box of cells in which all cells should be placed into corresponding boxes */
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file" + name_of_file);
  double low_x, low_y, width, height;
  BoxBin *R = &queue_box_bin.front();
  width = R->ur_point.x - R->ll_point.x;
  height = R->ur_point.y - R->ll_point.y;
  low_x = R->ll_point.x;
  low_y = R->ll_point.y;
  int step = 30;
  for (int j = 0; j < height; j += step) {
    ost << low_x << "\t" << low_y + j << "\n";
    ost << low_x + width << "\t" << low_y + j << "\n";
  }
  for (int j = 0; j < width; j += step) {
    ost << low_x + j << "\t" << low_y << "\n";
    ost << low_x + j << "\t" << low_y + height << "\n";
  }
  ost.close();
}
