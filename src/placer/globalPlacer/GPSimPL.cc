//
// Created by yihan on 8/4/2019.
//

#include "GPSimPL.h"
#include <cmath>
#include "../../common/misc.h"

GPSimPL::GPSimPL(): Placer() {}

GPSimPL::GPSimPL(double aspectRatio, double fillingRate): Placer(aspectRatio, fillingRate) {}

int GPSimPL::TotBlockNum() {
  return GetCircuit()->TotBlockNum();
}

double GPSimPL::WidthEpsilon() {
  return circuit_->AveMovWidth()/100.0;
}

double GPSimPL::HeightEpsilon() {
  return circuit_->AveMovHeight()/100.0;
}

void GPSimPL::InitCGFlags() {
  HPWLX_new = 0;
  HPWLY_new = 0;
  HPWLX_old = 1e30;
  HPWLY_old = 1e30;
  HPWLX_converge = false;
  HPWLY_converge = false;
}

void GPSimPL::BlockLocInit() {
  int length_x = Right() - Left();
  int length_y = Top() - Bottom();
  std::uniform_real_distribution<double> distribution(0,1);
  std::vector<Block> &block_list = *BlockList();
  for (auto &&block: block_list) {
    if (block.IsMovable()) {
      block.SetCenterX(Left() + length_x * distribution(generator));
      block.SetCenterY(Bottom() + length_y * distribution(generator));
    }
  }
  std::cout << "Block location randomly uniform initialization complete\n";
  ReportHPWL();
}

void GPSimPL::CGInit() {
  std::vector<Block> &block_list = *BlockList();
  int size = block_list.size();
  x.resize(size);
  y.resize(size);
  bx.resize(size);
  by.resize(size);
  Ax.resize(size, size);
  Ay.resize(size, size);
  x_anchor.resize(size);
  y_anchor.resize(size);

  cgx.setMaxIterations(cg_iteration_max_num);
  cgx.setTolerance(cg_precision);
  cgy.setMaxIterations(cg_iteration_max_num);
  cgy.setTolerance(cg_precision);

  std::vector<Net> &net_list = *NetList();
  int coefficient_size = 0;
  int net_size = 0;
  for (auto &&net: net_list) {
    net_size = net.P();
    if (net_size > 1) {
      coefficient_size += ((net_size-2)*2 + 1) * 4;
    }
  }
  coefficient_size += size; // this is for anchor, because each blk has an anchor
  //std::cout << "Reserve space for " << coefficient_size << " non-zero matrix entries\n";
  coefficients.reserve(coefficient_size);
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
  //std::cout << "HPWLX_old\tHPWLX_new\n";
  //std::cout << HPWLX_old << "\t" << HPWLX_new << "\n";
  if (HPWLX_new == 0) { // this is for 1 degree net, this happens in extremely rare cases
    HPWLX_converge = true;
  } else {
    HPWLX_converge = (std::fabs(1 - HPWLX_new / HPWLX_old) < HPWL_intra_linearSolver_precision);
    HPWLX_old = HPWLX_new;
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
    HPWLX_converge = true;
  } else {
    HPWLX_converge = (std::fabs(1 - HPWLX_new / HPWLX_old) < HPWL_intra_linearSolver_precision);
    HPWLX_old = HPWLX_new;
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
  //std::cout << "HPWLY_old\tHPWLY_new\n";
  //std::cout << HPWLY_old << "\t" << HPWLY_new << "\n";
  if (HPWLY_new == 0) { // this is for 1 degree net, this happens in extremely rare cases
    HPWLY_converge = true;
  } else {
    HPWLY_converge = (std::fabs(1 - HPWLY_new / HPWLY_old) < HPWL_intra_linearSolver_precision);
    HPWLY_old = HPWLY_new;
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
    HPWLY_converge = true;
  } else {
    HPWLY_converge = (std::fabs(1 - HPWLY_new / HPWLY_old) < HPWL_intra_linearSolver_precision);
    HPWLY_old = HPWLY_new;
  }
}

void GPSimPL::BuildProblemB2B(bool is_x_direction, Eigen::VectorXd &b) {
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
          weight = inv_p / (std::fabs(pin_loc0 - pin_loc1) + WidthEpsilon());
          if (!is_movable0 && is_movable1) {
            b[blk_num1] += (pin_loc0 - net.blk_pin_list[j].XOffset()) * weight;
            coefficients.emplace_back(T(blk_num1, blk_num1, weight));
          } else if (is_movable0 && !is_movable1) {
            b[blk_num0] += (pin_loc1 - net.blk_pin_list[i].XOffset()) * weight;
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
          weight = inv_p / (std::fabs(pin_loc0 - pin_loc1) + HeightEpsilon());
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
    for (size_t i = 0; i < block_list.size(); ++i) { // add the diagonal non-zero element for fixed blocks
      if (block_list[i].IsFixed()) {
        coefficients.emplace_back(T(i, i, 1));
        b[i] = block_list[i].LLY();
      }
    }
  }
  if (coefficients_capacity != coefficients.capacity()) {
    std::cout << "WARNING: coefficients capacity changed!\n";
    std::cout << "\told capacity: " << coefficients_capacity << "\n";
    std::cout << "\tnew capacity: " << coefficients.size() << "\n";
  }
}

void GPSimPL::BuildProblemB2BX() {
  BuildProblemB2B(true, bx);
  Ax.setFromTriplets(coefficients.begin(), coefficients.end());
}

void GPSimPL::BuildProblemB2BY() {
  BuildProblemB2B(false, by);
  Ay.setFromTriplets(coefficients.begin(), coefficients.end());
}

void GPSimPL::SolveProblemX() {
  std::vector<Block> &block_list = *BlockList();
  cgx.compute(Ax);
  x = cgx.solveWithGuess(bx, x);
  std::cout << "    #iterations:     " << cgx.iterations() << std::endl;
  std::cout << "    estimated error: " << cgx.error() << std::endl;
  for (long int num=0; num<x.size(); ++num) {
    if (block_list[num].IsMovable()) {
      if (x[num] < Left()) {
        x[num] = Left() + block_list[num].Width()/2.0;
      }
      if (x[num] > Right()) {
        x[num] = Right() - block_list[num].Width()/2.0;
      }
    }
    block_list[num].SetLLX(x[num]);
  }
}

void GPSimPL::SolveProblemY() {
  std::vector<Block> &block_list = *BlockList();
  cgy.compute(Ay);
  y = cgy.solveWithGuess(by, y);
  std::cout << "    #iterations:     " << cgy.iterations() << std::endl;
  std::cout << "    estimated error: " << cgy.error() << std::endl;
  for (long int num=0; num<y.size(); ++num) {
    if (block_list[num].IsMovable()) {
      if (y[num] < Bottom()) {
        y[num] = Bottom() + block_list[num].Height()/2.0;
      }
      if (y[num] > Top()) {
        y[num] = Top() - block_list[num].Height()/2.0;
      }
    }
    block_list[num].SetLLY(y[num]);
  }
}

void GPSimPL::InitialPlacement() {
  std::vector<Block> &block_list = *BlockList();

  HPWLX_converge = false;
  HPWLX_old = 1e30;
  for (size_t i=0; i<block_list.size(); ++i) {
    x[i] = block_list[i].LLX();
  }
  UpdateMaxMinX();
  for (int i=0; i<50; ++i) {
    BuildProblemB2BX();
    SolveProblemX();
    UpdateCGFlagsX();
    if (HPWLX_converge) {
      std::cout << "iterations x:     " << i << "\n";
      break;
    }
  }

  // Assembly:
  HPWLY_converge = false;
  HPWLY_old = 1e30;
  for (size_t i=0; i<block_list.size(); ++i) {
    y[i] = block_list[i].LLY();
  }
  UpdateMaxMinY();
  for (int i=0; i<50; ++i) {
    BuildProblemB2BY(); // fill A and b
    SolveProblemY();// Solving:
    UpdateCGFlagsY();
    if (HPWLY_converge) {
      std::cout << "iterations y:     " << i << "\n";
      break;
    }
  }

  std::cout << "Initial Placement Complete\n";
  ReportHPWL();
}

void GPSimPL::InitGridBins() {
  /* this is a function which create the grid bins, based on the grid_cnt and the boundaries of chip region, LEFT, RIGHT, BOTTOM, TOP
   * these four boundaries are given by the bounding box of placed macros/terminals now, these value should be specified in the .scl file
   * but it seems for global placement, the accurate value does not really matter now
   * in the future, when we want to do more careful global placement, we will then make some modifications*/

  /* determine the width and height of grid bin based on the boundaries and given grid_cnt */

  grid_bin_height = (int)(std::round(8 * GetCircuit()->AveMovHeight()));
  grid_cnt = std::ceil((double)(Top() - Bottom()) / grid_bin_height);
  grid_bin_width = std::ceil((double)(Right() - Left()) / grid_cnt);

  //std::cout << "grid_bin_height: " << grid_bin_height << "\n";
  //std::cout << "grid_bin_width: " << grid_bin_width << "\n";
  //std::cout << "grid_cnt: " << grid_cnt << "\n";

  /* create an empty grid bin column with size grid_cnt and push the grid bin column to grid_bin_matrix for grid_cnt times */
  std::vector<GridBin> temp_grid_bin_column(grid_cnt);
  for (int i = 0; i < grid_cnt; i++) {
    grid_bin_matrix.push_back(temp_grid_bin_column);
  }

  /* for each grid bin, we need to initialize the attributes, including index, boundaries, area, and potential available white space
   * the adjacent bin list is created for the convenience of overfilled bin clustering */
  for (int i=0; i<(int)(grid_bin_matrix.size()); i++) {
    for (int j = 0; j < (int)(grid_bin_matrix[i].size()); j++) {
      grid_bin_matrix[i][j].index.x = i;
      grid_bin_matrix[i][j].index.y = j;
      grid_bin_matrix[i][j].bottom = Bottom() + j * grid_bin_height;
      grid_bin_matrix[i][j].top = Bottom() + (j+1) * grid_bin_height;
      grid_bin_matrix[i][j].left = Left() + i * grid_bin_width;
      grid_bin_matrix[i][j].right = Left() + (i+1) * grid_bin_width;
      grid_bin_matrix[i][j].area = (grid_bin_matrix[i][j].right - grid_bin_matrix[i][j].left) * (grid_bin_matrix[i][j].top - grid_bin_matrix[i][j].bottom);
      grid_bin_matrix[i][j].white_space = grid_bin_matrix[i][j].area; // at the very beginning, assuming the white space is the same as area
      grid_bin_matrix[i][j].create_adjacent_bin_list(grid_cnt);
    }
  }

  for (auto &&grid_bin_column: grid_bin_matrix) {
    grid_bin_column[grid_cnt - 1].top = Top();
  }
  for (auto &&grid_bin: grid_bin_matrix[grid_cnt - 1]) {
    grid_bin.right = Right();
  }

  /* check whether the grid bin is occupied by terminals, and we only need to check it once, if we do not want to change GridBin configurations in the future
   * the idea of the part is that for each macro/terminal, we can easily calculate which grid bins it overlap with, and for each grid bin it overlap with,
   * we will check whether this grid bin is covered by this terminal, if yes, we set the label of this grid bin "all_terminal" to be true, initially, which is false
   * if not, we deduce the white space by the amount of terminal/macro and grid bin overlap region, because a grid bin might be covered by more than one terminals/macros
   * if the final white space is 0, we know the grid bin is also cover by terminals, and we set the flag "all_terminal" to be true also. */
  std::vector<Block> &block_list = *BlockList();
  int node_llx, node_lly, node_urx, node_ury, bin_llx, bin_lly, bin_urx, bin_ury;
  int min_urx, max_llx, min_ury, max_lly;
  int overlap_x, overlap_y, overlap_area;
  bool all_terminal, terminal_out_of_region, terminal_out_of_box;
  int left_index, right_index, bottom_index, top_index;
  for (size_t i=0; i<block_list.size(); i++) {
    /* find the left, right, bottom, top index of the grid */
    if (block_list[i].IsMovable()) continue;
    terminal_out_of_region = ((int)block_list[i].LLX() >= Right()) || ((int)block_list[i].URX() <= Left()) || ((int)block_list[i].LLY() >= Top()) || ((int)block_list[i].URY() <= Bottom());
    if (terminal_out_of_region) {
      continue;
    }
    left_index = (int)std::floor((block_list[i].LLX() - Left())/grid_bin_width);
    right_index = (int)std::floor((block_list[i].URX() - Left())/grid_bin_width);
    bottom_index = (int)std::floor((block_list[i].LLY() - Bottom())/grid_bin_height);
    top_index = (int)std::floor((block_list[i].URY() - Bottom())/grid_bin_height);
    /* sometimes the grid boundaries might be the placement region boundaries, need some fix here
     * because the top boundary and the right boundary of a grid bin does not actually belong to that grid bin */
    if (left_index < 0) {
      left_index = 0;
    }
    if (right_index == grid_cnt) {
      right_index = grid_cnt - 1;
    }
    if (bottom_index < 0) {
      bottom_index = 0;
    }
    if (top_index == grid_cnt) {
      top_index = grid_cnt - 1;
    }

    /* for each terminal, we will check which grid is inside it, and directly set the all_terminal attribute to true for that grid
     * some small terminals might occupy the same grid, we need to deduct the overlap area from the white space of that grid bin
     * when the final white space is 0, we know this grid bin is occupied by several terminals*/
    for (int j=left_index; j<=right_index; j++) {
      for (int k=bottom_index; k<=top_index; k++) {
        /* this is kind of rare case, the top/right of a terminal overlap with the bottom/left of a grid box
         * if this happens, we need to ignore this terminal for this grid box. */
        terminal_out_of_box = ((int)block_list[i].LLX() >= grid_bin_matrix[j][k].right) || ((int)block_list[i].URX() <= grid_bin_matrix[j][k].left) ||
            ((int)block_list[i].LLY() >= grid_bin_matrix[j][k].top) || ((int)block_list[i].URY() <= grid_bin_matrix[j][k].bottom);
        if (terminal_out_of_box) {
          continue;
        }
        grid_bin_matrix[j][k].terminal_list.push_back(i);

        /* if grid bin is covered by a large terminal, then this grid is all_terminal for sure */
        all_terminal = ((int)block_list[i].LLX() <= grid_bin_matrix[j][k].LLX()) && ((int)block_list[i].LLY() <= grid_bin_matrix[j][k].LLY()) &&
            ((int)block_list[i].URX() >= grid_bin_matrix[j][k].URX()) && ((int)block_list[i].URY() >= grid_bin_matrix[j][k].URY());
        grid_bin_matrix[j][k].all_terminal = all_terminal;
        /* if not, we need to calculate the overlap of grid bin and this terminal to calculate white space, when white space is 0, this grid bin is also all_terminal */
        if (all_terminal) {
          grid_bin_matrix[j][k].white_space = 0;
        } else {
          node_llx = (int)block_list[i].LLX();
          node_lly = (int)block_list[i].LLY();
          node_urx = (int)block_list[i].URX();
          node_ury = (int)block_list[i].URY();
          bin_llx = grid_bin_matrix[j][k].LLX();
          bin_lly = grid_bin_matrix[j][k].LLY();
          bin_urx = grid_bin_matrix[j][k].URX();
          bin_ury = grid_bin_matrix[j][k].URY();

          max_llx = std::max(node_llx, bin_llx);
          max_lly = std::max(node_lly, bin_lly);
          min_urx = std::min(node_urx, bin_urx);
          min_ury = std::min(node_ury, bin_ury);

          overlap_x = min_urx - max_llx;
          overlap_y = min_ury - max_lly;
          overlap_area = overlap_x * overlap_y;
          grid_bin_matrix[j][k].white_space -= overlap_area;
          if (grid_bin_matrix[j][k].white_space < 1) {
            grid_bin_matrix[j][k].all_terminal = true;
            grid_bin_matrix[j][k].white_space = 0;
          }
        }
      }
    }
  }
  //std::cout << "Grid bin mesh initialization complete\n";
}

void GPSimPL::InitWhiteSpaceLut() {
  /* this is a member function to initialize white space look-up table
   * this table is a matrix, one way to calculate the white space in a region is to add all white space of every single grid bin in it
   * but an easier way is to define an accumulate function and store it as a look-up table
   * when we want to find the white space in a region, just read it from the look-up table*/

  /* this for loop is created to initialize the size of the loop-up table */
  std::vector< int > tmp_vector(grid_cnt);
  for (int i=0; i<grid_cnt ; i++) {
    grid_bin_white_space_LUT.push_back(tmp_vector);
  }

  /* this for loop is used for computing elements in the look-up table
   * there are four cases, element at (0,0), elements on the left edge, elements on the right edge, otherwise*/
  for (size_t kx=0; kx<grid_bin_matrix.size(); kx++) {
    for (size_t ky=0; ky<grid_bin_matrix[kx].size(); ky++) {
      grid_bin_white_space_LUT[kx][ky] = 0;
      if (kx==0) {
        if (ky==0) {
          grid_bin_white_space_LUT[kx][ky] = grid_bin_matrix[0][0].white_space;
        } else {
          grid_bin_white_space_LUT[kx][ky] = grid_bin_white_space_LUT[kx][ky-1] + grid_bin_matrix[kx][ky].white_space;
        }
      } else {
        if (ky==0) {
          grid_bin_white_space_LUT[kx][ky] = grid_bin_white_space_LUT[kx-1][ky] + grid_bin_matrix[kx][ky].white_space;
        } else {
          grid_bin_white_space_LUT[kx][ky] = grid_bin_white_space_LUT[kx-1][ky] + grid_bin_white_space_LUT[kx][ky-1]
              + grid_bin_matrix[kx][ky].white_space - grid_bin_white_space_LUT[kx-1][ky-1];
        }
      }
    }
  }
  //std::cout << "White space look-up table initialization complete\n";
}

void GPSimPL::LookAheadLgInit() {
  InitGridBins();
  InitWhiteSpaceLut();
}

void GPSimPL::LookAheadClose() {
  grid_bin_matrix.clear();
  grid_bin_white_space_LUT.clear();
}

void GPSimPL::ClearGridBinFlag() {
  for (auto &&bin_column: grid_bin_matrix) {
    for (auto &&bin: bin_column) {
      bin.global_placed = false;
    }
  }
}

int GPSimPL::LookUpWhiteSpace(GridBinIndex const &ll_index, GridBinIndex const &ur_index) {
  /* this function is used to return the white space in a region specified by ll_index, and ur_index
   * there are four cases, element at (0,0), elements on the left edge, elements on the right edge, otherwise */

  int total_white_space;
  if (ll_index.x == 0) {
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
  }
  return total_white_space;
}

void GPSimPL::UpdateGridBinState() {
  /* this is a member function to initialize grid bins, because the cell_list, cell_area and over_fill state can be changed
   * so we need to update them when necessary*/

  /* clean the old data */
  for (auto &&bin_column:grid_bin_matrix) {
    for (auto &&bin:bin_column) {
      bin.cell_list.clear();
      bin.cell_area = 0;
      bin.over_fill = false;
    }
  }

  /* for each cell, find the index of the grid bin it should be in
   * note that in extreme cases, the index might be smaller than 0 or larger than the maximum allowed index
   * because the cell is on the boundaries, so we need to make some modifications for these extreme cases*/
  std::vector<Block> &block_list = *BlockList();
  int x_index, y_index;
  for (size_t i=0; i<block_list.size(); i++) {
    if (block_list[i].IsFixed()) continue;
    x_index = (int)std::floor((block_list[i].X() - Left())/grid_bin_width);
    y_index = (int)std::floor((block_list[i].Y() - Bottom())/grid_bin_height);
    if (x_index < 0) {
      x_index = 0;
    }
    if (x_index > grid_cnt-1) {
      x_index = grid_cnt - 1;
    }
    if (y_index < 0) {
      y_index = 0;
    }
    if (y_index > grid_cnt-1) {
      y_index = grid_cnt - 1;
    }
    grid_bin_matrix[x_index][y_index].cell_list.push_back(i);
    grid_bin_matrix[x_index][y_index].cell_area += block_list[i].Area();
  }

  /* below is the criterion to decide whether a grid bin is over_filled or not
   * 1. if this bin if fully occupied by terminals, but its cell_list is non-empty, which means there is some cells overlap with this grid bin, we say it is over_fill
   * 2. if not fully occupied by terminals, but filling_rate is larger than the TARGET_FILLING_RATE, then set is to over_fill
   * 3. if this bin is not overfilled, but cells in this bin overlaps with terminals in this bin, we also mark it as over_fill*/
  bool over_fill = false;
  for (auto &&bin_column: grid_bin_matrix) {
    for (auto &&bin: bin_column) {
      if (bin.global_placed) {
        bin.over_fill = false;
        continue;
      }
      if (bin.is_all_terminal()) {
        if (!bin.cell_list.empty()) {
          bin.over_fill = true;
        }
      } else {
        bin.filling_rate = bin.cell_area/(double)bin.white_space;
        if (bin.filling_rate > FillingRate()) {
          bin.over_fill = true;
        }
      }
      if (!bin.OverFill()) {
        for (auto &&cell_num: bin.cell_list) {
          for (auto &&terminal_num: bin.terminal_list) {
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
  /* this function is to cluster overfilled grid bins, and sort them based on total cell area
   * the algorithm to cluster overfilled grid bins is breadth first search*/
  cluster_list.clear();
  for (auto &&bin_column: grid_bin_matrix) {
    for (auto &&bin: bin_column) {
      bin.cluster_visited = false;
    }
  }
  GridBinIndex tmp_index;
  for (size_t i=0; i<grid_bin_matrix.size(); i++) {
    for (size_t j=0; j<grid_bin_matrix[i].size(); j++) {
      if ((grid_bin_matrix[i][j].cluster_visited)||(!grid_bin_matrix[i][j].over_fill)) {
        grid_bin_matrix[i][j].cluster_visited = true;
        continue;
      }
      tmp_index.x = i;
      tmp_index.y = j;
      GridBinCluster H;
      H.grid_bin_index_set.insert(tmp_index);
      grid_bin_matrix[i][j].cluster_visited = true;
      std::queue < GridBinIndex > Q;
      Q.push(tmp_index);
      while (!Q.empty()) {
        tmp_index = Q.front();
        Q.pop();
        for (auto &&index: grid_bin_matrix[tmp_index.x][tmp_index.y].adjacent_bin_index) {
          GridBin *bin = &grid_bin_matrix[index.x][index.y];
          if ((!bin->cluster_visited)&&(bin->over_fill)) {
            bin->cluster_visited = true;
            H.grid_bin_index_set.insert(index);
            Q.push(index);
          }
        }
      }
      cluster_list.push_back(H);
    }
  }
}

void GPSimPL::SortGridBinCluster() {
  // sort grid bin clusters based on total cell area
  // first calculate the total cell area, total white space
  for (auto &&cluster: cluster_list) {
    cluster.total_cell_area = 0;
    cluster.total_white_space = 0;
    for (auto &&index: cluster.grid_bin_index_set) {
      GridBin *tmp_grid_bin = &grid_bin_matrix[index.x][index.y];
      cluster.total_cell_area += tmp_grid_bin->cell_area;
      cluster.total_white_space += tmp_grid_bin->white_space;
    }
    //std::cout << "Total cell area: " << cluster.total_cell_area;
    //std::cout << " Total white space: " << cluster.total_white_space << "\n";
  }
  /* then do the Selection sort */
  size_t max_index;
  GridBinCluster tmp_cluster;
  for (size_t i=0; i<cluster_list.size(); i++) {
    max_index = i;
    for (size_t j=i+1; j<cluster_list.size(); j++) {
      if (cluster_list[j].total_cell_area > cluster_list[max_index].total_cell_area) {
        max_index = j;
      }
    }
    if (i != max_index) {
      tmp_cluster = cluster_list[i];
      cluster_list[i] = cluster_list[max_index];
      cluster_list[max_index] = tmp_cluster;
    }
    //std::cout << "Total cell area: " << cluster_list[i].total_cell_area;
    //std::cout << " Total white space: " << cluster_list[i].total_white_space << "\n";
  }
}

void GPSimPL::UpdateClusterList() {
  ClusterOverfilledGridBin();
  SortGridBinCluster();
}

void GPSimPL::FindMinimumBoxForFirstCluster() {
  /* this function find the box for the first cluster, such that the total white space in the box is larger than the total
   * cell area, the way to do this is just by expanding the boundaries of the bounding box of the first cluster */

  while(!queue_box_bin.empty()) queue_box_bin.pop();
  // clear the queue_box_bin

  if (cluster_list.empty()) return;
  // this is redundant, but for safety reasons. Probably this statement can be safely removed
  std::vector<Block> &block_list = *BlockList();

  BoxBin R;
  R.cut_direction_x = false;
  R.ll_index.x = grid_cnt - 1;
  R.ll_index.y = grid_cnt - 1;
  R.ur_index.x = 0;
  R.ur_index.y = 0;
  // initialize a box with y cut-direction
  // identify the bounding box of the initial cluster
  for (auto &&index: cluster_list[0].grid_bin_index_set) {
    GridBin *bin = &grid_bin_matrix[index.x][index.y];
    if (bin->index.x < R.ll_index.x) {
      R.ll_index.x = bin->index.x;
    }
    if (bin->index.x > R.ur_index.x) {
      R.ur_index.x = bin->index.x;
    }
    if (bin->index.y < R.ll_index.y) {
      R.ll_index.y = bin->index.y;
    }
    if (bin->index.y > R.ur_index.y) {
      R.ur_index.y = bin->index.y;
    }
  }
  while (true) {
    // update cell area, white space, and thus filling rate to determine whether to expand this box or not
    R.total_white_space = LookUpWhiteSpace(R.ll_index, R.ur_index);
    R.update_cell_area_white_space_LUT(grid_bin_white_space_LUT, grid_bin_matrix);
    //R.update_cell_area_white_space(grid_bin_matrix);
    if (R.filling_rate > FillingRate()) {
      R.expand_box(grid_cnt);
    }
    else {
      break;
    }
  }
  R.update_cell_in_box(grid_bin_matrix);
  R.ll_point.x = grid_bin_matrix[R.ll_index.x][R.ll_index.y].left;
  R.ll_point.y = grid_bin_matrix[R.ll_index.x][R.ll_index.y].bottom;
  R.ur_point.x = grid_bin_matrix[R.ur_index.x][R.ur_index.y].right;
  R.ur_point.y = grid_bin_matrix[R.ur_index.x][R.ur_index.y].top;

  R.left = R.ll_point.x;
  R.bottom = R.ll_point.y;
  R.right = R.ur_point.x;
  R.top = R.ur_point.y;

  if (R.ll_index == R.ur_index) {
    R.update_terminal_list(grid_bin_matrix);
    if (R.has_terminal()) {
      R.update_ob_boundaries(block_list);
    }
  }
  queue_box_bin.push(R);
  //std::cout << "Bounding box total white space: " << queue_box_bin.front().total_white_space << "\n";
  //std::cout << "Bounding box total cell area: " << queue_box_bin.front().total_cell_area << "\n";

  for (int kx = R.ll_index.x; kx <= R.ur_index.x; kx++) {
    for (int ky = R.ll_index.y; ky <= R.ur_index.y; ky++) {
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
  //std::cout << box1.ll_index << box1.ur_index << "\n";
  //std::cout << box2.ll_index << box2.ur_index << "\n";
  //box1.update_all_terminal(grid_bin_matrix);
  //box2.update_all_terminal(grid_bin_matrix);
  // if the white space in one bin is dominating the other, ignore the smaller one
  dominating_box_flag = 0;
  if (box1.total_white_space/(double)box.total_white_space <= 0.01) {
    dominating_box_flag = 1;
  }
  if (box2.total_white_space/(double)box.total_white_space <= 0.01) {
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
      box1.update_terminal_list(grid_bin_matrix);
      box1.update_ob_boundaries(block_list);
    }
    if (box2.ll_index == box2.ur_index) {
      box2.update_terminal_list(grid_bin_matrix);
      box2.update_ob_boundaries(block_list);
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
      box2.update_terminal_list(grid_bin_matrix);
      box2.update_ob_boundaries(block_list);
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
      box1.update_terminal_list(grid_bin_matrix);
      box1.update_ob_boundaries(block_list);
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

    if (box1.total_white_space/(double)box.total_white_space <= 0.01) {
      box2.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box2.cell_list = box.cell_list;
      box2.total_cell_area = box.total_cell_area;
      box2.update_ob_boundaries(block_list);
      queue_box_bin.push(box2);
    } else if (box2.total_white_space/(double)box.total_white_space <= 0.01) {
      box1.ll_point = box.ll_point;
      box1.ur_point = box.ur_point;
      box1.cell_list = box.cell_list;
      box1.total_cell_area = box.total_cell_area;
      box1.update_ob_boundaries(block_list);
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
      box1.update_ob_boundaries(block_list);
      box2.update_ob_boundaries(block_list);
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

    if (box1.total_white_space/(double)box.total_white_space <= 0.01) {
      box2.ll_point = box.ll_point;
      box2.ur_point = box.ur_point;
      box2.cell_list = box.cell_list;
      box2.total_cell_area = box.total_cell_area;
      box2.update_ob_boundaries(block_list);
      queue_box_bin.push(box2);
    } else if (box2.total_white_space/(double)box.total_white_space <= 0.01) {
      box1.ll_point = box.ll_point;
      box1.ur_point = box.ur_point;
      box1.cell_list = box.cell_list;
      box1.total_cell_area = box.total_cell_area;
      box1.update_ob_boundaries(block_list);
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
      box1.update_ob_boundaries(block_list);
      box2.update_ob_boundaries(block_list);
      queue_box_bin.push(box1);
      queue_box_bin.push(box2);
    }
  }
}

void GPSimPL::PlaceBlkInBox(BoxBin &box) {
    std::vector<Block> &block_list = *BlockList();
    /* this is the simplest version, just linearly move cells in the cell_box to the grid box
     * non-linearity is not considered yet*/
    double cell_box_left, cell_box_bottom;
    double cell_box_width, cell_box_height;
    cell_box_left = box.ll_point.x;
    cell_box_bottom = box.ll_point.y;
    cell_box_width = box.ur_point.x - cell_box_left;
    cell_box_height = box.ur_point.y - cell_box_bottom;
    Block *cell;
    for (auto &&cell_id: box.cell_list) {
      cell = &block_list[cell_id];
      cell->SetCenterX((cell->X() - cell_box_left)/cell_box_width * (box.right - box.left) + box.left);
      cell->SetCenterY((cell->Y() - cell_box_bottom)/cell_box_height * (box.top - box.bottom) + box.bottom);
      /*if ((box.left < LEFT) || (box.bottom < BOTTOM)) {
        std::cout << "LEFT:" << LEFT << " " << "BOTTOM:" << BOTTOM << "\n";
        std::cout << box.left << " " << box.bottom << "\n";
      }*/
    }
}

double GPSimPL::BlkOverlapArea(Block *node1, Block *node2) {
  bool not_overlap;
  not_overlap = ((node1->LLX() >= node2->URX())||(node1->LLY() >= node2->URY())) || ((node2->LLX() >= node1->URX())||(node2->LLY() >= node1->URY()));
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
  size_t max_cell_num_in_box = 1;
  box.cut_direction_x = true;
  std::queue< BoxBin > box_Q;
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

      int ave_blk_height = std::ceil(GetCircuit()->AveMovHeight());
      //std::cout << "Average block height: " << ave_blk_height << "  " << GetCircuit()->AveMovHeight() << "\n";
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
        for (auto &&cell_id: front_box.cell_list) {
          cell = &block_list[cell_id];
          cell->SetCenterX((front_box.left + front_box.right)/2.0);
          cell->SetCenterY((front_box.bottom + front_box.top)/2.0);
        }
      } else {
        PlaceBlkInBox(front_box);
      }
    }
    box_Q.pop();
  }

}

bool GPSimPL::RecursiveBisectionBlkSpreading() {
  /* keep splitting the biggest box to many small boxes, and keep update the shape of each box and cells should be assigned to the box */
  while(!queue_box_bin.empty()) {
    if (queue_box_bin.empty()) break;
    BoxBin &box = queue_box_bin.front();
    /* when the box is a grid bin box or a smaller box with no terminals inside, start moving cells to the box */
    /* if there is terminals inside a box, keep splitting it */
    if (box.ll_index == box.ur_index) {
      if (box.has_terminal()) {
        SplitGridBox(box);
        queue_box_bin.pop();
        continue;
      }
      /* if no terminals in side a box, do cell placement inside the box */
      PlaceBlkInBoxBisection(box);
      queue_box_bin.pop();
    } else {
      SplitBox(box);
      queue_box_bin.pop();
    }
  }
  return true;
}

void GPSimPL::BackUpBlkLoc () {
  std::vector<Block> &block_list = *BlockList();
  for (size_t i=0; i<block_list.size(); ++i) {
    x_anchor[i] = block_list[i].LLX();
    y_anchor[i] = block_list[i].LLY();
  }
}

void GPSimPL::UpdateAnchorLoc() {
  std::vector<Block> &block_list = *BlockList();
  double tmp_value;
  for (size_t i=0; i<block_list.size(); ++i) {
    if (block_list[i].IsFixed()) continue;
    tmp_value = x_anchor[i];
    x_anchor[i] = block_list[i].LLX();
    block_list[i].SetLLX(tmp_value);

    tmp_value = y_anchor[i];
    y_anchor[i] = block_list[i].LLY();
    block_list[i].SetLLY(tmp_value);
  }
}

void GPSimPL::BuildProblemB2BWithAnchorX() {
  BuildProblemB2BX();
  std::vector<Block> &block_list = *BlockList();
  double weight = 0;
  double pin_loc0, pin_loc1;
  for (size_t i=0; i<block_list.size(); ++i) {
    if (block_list[i].IsFixed()) continue;
    pin_loc0 = block_list[i].LLX();
    pin_loc1 = x_anchor[i];
    weight = alpha/(std::fabs(pin_loc0 - pin_loc1) + WidthEpsilon());
    bx[i] += pin_loc1 * weight;
    coefficients.emplace_back(T(i, i, weight));
  }
  Ax.setFromTriplets(coefficients.begin(), coefficients.end());
}

void GPSimPL::BuildProblemB2BWithAnchorY() {
  BuildProblemB2BY();
  std::vector<Block> &block_list = *BlockList();
  double weight = 0;
  double pin_loc0, pin_loc1;
  for (size_t i=0; i<block_list.size(); ++i) {
    if (block_list[i].IsFixed()) continue;
    pin_loc0 = block_list[i].LLY();
    pin_loc1 = y_anchor[i];
    weight = alpha/(std::fabs(pin_loc0 - pin_loc1) + WidthEpsilon());
    by[i] += pin_loc1 * weight;
    coefficients.emplace_back(T(i, i, weight));
  }
  Ay.setFromTriplets(coefficients.begin(), coefficients.end());
}

void GPSimPL::QuadraticPlacementWithAnchor() {
  std::vector<Block> &block_list = *BlockList();

  HPWLX_converge = false;
  HPWLX_old = 1e30;
  for (size_t i=0; i<block_list.size(); ++i) {
    x[i] = block_list[i].LLX();
  }
  UpdateMaxMinX();
  for (int i=0; i<50; ++i) {
    BuildProblemB2BWithAnchorX();
    SolveProblemX();
    UpdateCGFlagsX();
    if (HPWLX_converge) {
      //std::cout << "iterations x:     " << i << "\n";
      break;
    }
  }
  HPWLY_converge = false;
  HPWLY_old = 1e30;
  for (size_t i=0; i<block_list.size(); ++i) {
    y[i] = block_list[i].LLY();
  }
  UpdateMaxMinY();
  for (int i=0; i<50; ++i) {
    BuildProblemB2BWithAnchorY();
    SolveProblemY();
    UpdateCGFlagsY();
    if (HPWLY_converge) {
      //std::cout << "iterations y:     " << i << "\n";
      break;
    }
  }
  std::cout << "Quadratic Placement With Anchor Complete\n";
  ReportHPWL();
}

void GPSimPL::UpdateAnchorNetWeight() {
  alpha += 0.01;
}

void GPSimPL::LookAheadLegalization() {
  BackUpBlkLoc();
  ClearGridBinFlag();

  do {
    UpdateGridBinState();
    UpdateClusterList();
    FindMinimumBoxForFirstCluster();
    RecursiveBisectionBlkSpreading();
  } while (!cluster_list.empty());

  /*UpdateGridBinState();
  grid_bin_matrix[3][3].Report();
  grid_bin_matrix[4][3].Report();
  write_not_overfill_grid_bins("grid_bin_not_overfill.txt");
  write_overfill_grid_bins("grid_bin_overfill.txt");
  UpdateClusterList();
  FindMinimumBoxForFirstCluster();
  RecursiveBisectionBlkSpreading();*/

  UpdateHPWLX();
  UpdateHPWLY();
  std::cout << "Look-ahead legalization complete\n";
  ReportHPWL();
}

void GPSimPL::UpdateLALConvergeState() {
  HPWL_LAL_new = HPWLX_new + HPWLY_new;
  HPWL_LAL_converge = std::fabs(1 - HPWL_LAL_new/HPWL_LAL_old) < HPWL_inter_linearSolver_precision;
  HPWL_LAL_old = HPWL_LAL_new;
}

void GPSimPL::StartPlacement() {
  SanityCheck();

  CGInit();
  LookAheadLgInit();
  BlockLocInit();
  InitialPlacement();

  for (int i=0; i<look_ahead_iter_max; ++i) {
    LookAheadLegalization();
    UpdateLALConvergeState();
    if (HPWL_LAL_converge || i==look_ahead_iter_max-1) { // if HPWL sconverges
      std::cout << "Iterative look-ahead legalization complete" << std::endl;
      break;
    } else {
      UpdateAnchorLoc();
      UpdateAnchorNetWeight();
    }
    QuadraticPlacementWithAnchor();
  }
  //ReportHPWL();
  std::cout << "Global Placement complete\n";

  LookAheadClose();
  //DrawBlockNetList("cg_result.txt");
}

void GPSimPL::DrawBlockNetList(std::string const &name_of_file) {
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open input file " + name_of_file);
  ost << Left() << " " << Bottom() << " " << Right() - Left() << " " << Top() - Bottom() << "\n";
  std::vector<Block> &block_list = *BlockList();
  for (auto &&block: block_list) {
    ost << block.LLX() << " " << block.LLY() << " " << block.Width() << " " << block.Height() << "\n";
    //ost << block.X() << " " << block.Y() << " " << 1 << " " << 1 << "\n";
  }
  ost.close();
}

void GPSimPL::write_all_terminal_grid_bins(std::string const &name_of_file) {
  /* this is a member function for testing, print grid bins where the flag "all_terminal" is true */
  std::ofstream ost(name_of_file.c_str());
  Assert(ost.is_open(), "Cannot open file" + name_of_file);
  for (auto &&bin_column: grid_bin_matrix) {
    for (auto &&bin: bin_column) {
      double low_x, low_y, width, height;
      width = bin.right - bin.left;
      height = bin.top - bin.bottom;
      low_x = bin.left;
      low_y = bin.bottom;
      int N = 3;
      double step_x = width/N, step_y = height/N;
      if (bin.is_all_terminal()) {
        for (int j = 0; j < N; j++) {
          ost << low_x << "\t" << low_y + j*step_y << "\n";
          ost << low_x + width << "\t" << low_y + j*step_y << "\n";
        }
        for (int j = 0; j < N; j++) {
          ost << low_x + j*step_x << "\t" << low_y << "\n";
          ost << low_x + j*step_x << "\t" << low_y + height << "\n";
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
  for (auto &&bin_column: grid_bin_matrix) {
    for (auto &&bin: bin_column) {
      double low_x, low_y, width, height;
      width = bin.right - bin.left;
      height = bin.top - bin.bottom;
      low_x = bin.left;
      low_y = bin.bottom;
      int N = 3;
      double step_x = width/N, step_y = height/N;
      if (!bin.is_all_terminal()) {
        for (int j = 0; j < N; j++) {
          ost << low_x << "\t" << low_y + j*step_y << "\n";
          ost << low_x + width << "\t" << low_y + j*step_y << "\n";
        }
        for (int j = 0; j < N; j++) {
          ost << low_x + j*step_x << "\t" << low_y << "\n";
          ost << low_x + j*step_x << "\t" << low_y + height << "\n";
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
  for (auto &&bin_column: grid_bin_matrix) {
    for (auto &&bin: bin_column) {
      double low_x, low_y, width, height;
      width = bin.right - bin.left;
      height = bin.top - bin.bottom;
      low_x = bin.left;
      low_y = bin.bottom;
      int N = 10;
      double step_x = width/N, step_y = height/N;
      if (bin.OverFill()) {
        for (int j = 0; j < N; j++) {
          ost << low_x << "\t" << low_y + j*step_y << "\n";
          ost << low_x + width << "\t" << low_y + j*step_y << "\n";
        }
        for (int j = 0; j < N; j++) {
          ost << low_x + j*step_x << "\t" << low_y << "\n";
          ost << low_x + j*step_x << "\t" << low_y + height << "\n";
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
  for (auto &&bin_column: grid_bin_matrix) {
    for (auto &&bin: bin_column) {
      double low_x, low_y, width, height;
      width = bin.right - bin.left;
      height = bin.top - bin.bottom;
      low_x = bin.left;
      low_y = bin.bottom;
      int N = 10;
      double step_x = width/N, step_y = height/N;
      if (!bin.OverFill()) {
        for (int j = 0; j < N; j++) {
          ost << low_x << "\t" << low_y + j*step_y << "\n";
          ost << low_x + width << "\t" << low_y + j*step_y << "\n";
        }
        for (int j = 0; j < N; j++) {
          ost << low_x + j*step_x << "\t" << low_y << "\n";
          ost << low_x + j*step_x << "\t" << low_y + height << "\n";
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
  for (size_t i=0; i<n; i++) {
    for (auto &&index: cluster_list[i].grid_bin_index_set) {
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
  low_x = R->ll_index.x * grid_bin_width + Left();
  low_y = R->ll_index.y * grid_bin_height + Bottom();
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
