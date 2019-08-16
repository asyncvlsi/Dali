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
  HPWLX_converge = false;
  HPWLY_converge = false;
}

void GPSimPL::BlockLocInit() {
  int length_x = Right() - Left();
  int length_y = Top() - Bottom();
  std::default_random_engine generator{0};
  std::uniform_real_distribution<double> distribution(0,1);
  std::vector<Block> &block_list = *BlockList();
  for (auto &&block: block_list) {
    if (block.IsMovable()) {
      block.SetCenterX(Left() + length_x * distribution(generator));
      block.SetCenterY(Bottom() + length_y * distribution(generator));
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
  std::cout << "HPWLX_old\tHPWLX_new\n";
  std::cout << HPWLX_old << "\t" << HPWLX_new << "\n";
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
  std::cout << "HPWLY_old\tHPWLY_new\n";
  std::cout << HPWLY_old << "\t" << HPWLY_new << "\n";
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

void GPSimPL::BuildProblemB2B(bool is_x_direction, SpMat &A, Eigen::VectorXd &b) {
  std::vector<Block> &block_list = *BlockList();
  std::vector<Net> &net_list = *NetList();
  std::vector<T> coefficients;
  coefficients.reserve(10000000);
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
        pin_loc0 = net.blk_pin_list[i].AbsX();
        is_movable0 = net.blk_pin_list[i].GetBlock()->IsMovable();
        for (size_t j = i + 1; j < net.blk_pin_list.size(); j++) {
          if ((i != max_pin_index) && (i != min_pin_index)) {
            if ((j != max_pin_index) && (j != min_pin_index)) continue;
          }
          blk_num1 = net.blk_pin_list[j].BlockNum();
          if (blk_num0 == blk_num1) continue;
          pin_loc1 = net.blk_pin_list[j].AbsX();
          is_movable1 = net.blk_pin_list[j].GetBlock()->IsMovable();
          weight = inv_p / (fabs(pin_loc0 - pin_loc1) + WidthEpsilon());
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
      if (!block_list[i].IsMovable()) {
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
        pin_loc0 = net.blk_pin_list[i].AbsY();
        is_movable0 = net.blk_pin_list[i].GetBlock()->IsMovable();
        for (size_t j = i + 1; j < net.blk_pin_list.size(); j++) {
          if ((i != max_pin_index) && (i != min_pin_index)) {
            if ((j != max_pin_index) && (j != min_pin_index)) continue;
          }
          blk_num1 = net.blk_pin_list[j].BlockNum();
          if (blk_num0 == blk_num1) continue;
          pin_loc1 = net.blk_pin_list[j].AbsY();
          is_movable1 = net.blk_pin_list[j].GetBlock()->IsMovable();
          weight = inv_p / (fabs(pin_loc0 - pin_loc1) + HeightEpsilon());
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
    for (size_t i = 0; i < block_list.size(); ++i) {
      if (!block_list[i].IsMovable()) {
        coefficients.emplace_back(T(i, i, 1));
        b[i] = block_list[i].LLY();
      }
    }
  }
  A.setFromTriplets(coefficients.begin(), coefficients.end());
}

void GPSimPL::QuadraticPlacement() {
  std::cout << "Total number of movable cells: " << GetCircuit()->TotMovableBlockNum() << "\n";
  std::cout << "Total number of cells: " << TotBlockNum() << "\n";
  std::vector<Block> &block_list = *BlockList();
  int cellNum = TotBlockNum();

  Eigen::ConjugateGradient <SpMat> cgx;
  cgx.setMaxIterations(cg_iteration_max_num);
  cgx.setTolerance(cg_precision);
  HPWLX_converge = false;
  HPWLX_old = 1e30;
  Eigen::VectorXd x(cellNum), eigen_bx(cellNum);
  SpMat eigen_Ax(cellNum, cellNum);
  for (size_t i=0; i<block_list.size(); ++i) {
    x[i] = block_list[i].LLX();
  }

  UpdateMaxMinX();
  for (int i=0; i<50; ++i) {
    BuildProblemB2B(true, eigen_Ax, eigen_bx);
    cgx.compute(eigen_Ax);
    x = cgx.solveWithGuess(eigen_bx, x);
    //std::cout << "Here is the vector x:\n" << x << std::endl;
    std::cout << "\t#iterations:     " << cgx.iterations() << std::endl;
    std::cout << "\testimated error: " << cgx.error() << std::endl;
    for (long int num=0; num<x.size(); ++num) {
      block_list[num].SetLLX(x[num]);
    }
    UpdateCGFlagsX();
    if (HPWLX_converge) {
      std::cout << "iterations x:     " << i << "\n";
      break;
    }
  }

  Eigen::ConjugateGradient <SpMat> cgy;
  cgy.setMaxIterations(cg_iteration_max_num);
  cgy.setTolerance(cg_precision);
  // Assembly:
  HPWLY_converge = false;
  HPWLY_old = 1e30;
  Eigen::VectorXd y(cellNum), eigen_by(cellNum); // the solution and the right hand side-vector resulting from the constraints
  SpMat eigen_Ay(cellNum, cellNum); // sparse matrix
  for (size_t i=0; i<block_list.size(); ++i) {
    y[i] = block_list[i].LLY();
  }

  UpdateMaxMinY();
  for (int i=0; i<15; ++i) {
    BuildProblemB2B(false, eigen_Ay, eigen_by); // fill A and b
    // Solving:
    cgy.compute(eigen_Ay);
    y = cgy.solveWithGuess(eigen_by, y);
    std::cout << "\t#iterations:     " << cgy.iterations() << std::endl;
    std::cout << "\testimated error: " << cgy.error() << std::endl;
    for (long int num=0; num<y.size(); ++num) {
      block_list[num].SetLLY(y[num]);
    }
    UpdateCGFlagsY();
    if (HPWLY_converge) {
      std::cout << "iterations y:     " << i << "\n";
      break;
    }
  }
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
  ReportHPWL();
  QuadraticPlacement();
  std::cout << "Initial Placement Complete\n";
  ReportHPWL();
}
