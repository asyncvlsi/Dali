//
// Created by Yihang Yang on 2019-05-20.
//

#include <cmath>
#include "placeral.h"

placer_al_t::placer_al_t(): placer_t() {

}

placer_al_t::placer_al_t(double aspectRatio, double fillingRate): placer_t(aspectRatio, fillingRate) {

}

size_t placer_al_t::movable_block_num() {
  return _movable_block_num;
}

size_t placer_al_t::terminal_block_num() {
  return _terminal_block_num;
}

size_t placer_al_t::block_num() {
  return _movable_block_num + _terminal_block_num;
}

double placer_al_t::width_epsilon() {
  return _width_epsilon;
}

double placer_al_t::height_epsilon() {
  return _height_epsilon;
}

bool placer_al_t::set_input_circuit(circuit_t *circuit) {
  block_list.clear();
  net_list.clear();
  if (circuit->blockList.empty()) {
    std::cout << "Error!\n";
    std::cout << "Invalid input circuit: empty block list!\n";
    return false;
  }
  if (circuit->netList.empty()) {
    std::cout << "Error!\n";
    std::cout << "Invalid input circuit: empty net list!\n";
    return false;
  }

  _circuit = circuit;
  _movable_block_num = 0;
  _terminal_block_num = 0;
  _width_epsilon = 0;
  _height_epsilon = 0;
  for (auto &&block: circuit->blockList) {
    block_al_t block_al;
    block_al.retrieve_info_from_database(block);
    block_list.push_back(block_al);
    //std::cout << block_dla << "\n";
    if (block_al.is_movable()) {
      ++_movable_block_num;
      _width_epsilon += block.width();
      _height_epsilon += block.height();
    } else {
      ++_terminal_block_num;
    }
  }
  if (_movable_block_num == 0) {
    std::cout << "Error!\n";
    std::cout << "No movable blocks in circuit!\n";
    return false;
  }
  _width_epsilon /= _movable_block_num*100;
  _height_epsilon /= _movable_block_num*100;

  for (auto &&net: circuit->netList) {
    net_al_t net_al;
    net_al.retrieve_info_from_database(net);
    for (auto &&pin: net.pin_list) {
      int block_num = circuit->blockNameMap.find(pin.get_block()->name())->second;
      block_al_t *block_al_ptr = &block_list[block_num];
      pin_t pin_al(pin.x_offset(), pin.y_offset(), block_al_ptr);
      net_al.pin_list.push_back(pin_al);
    }
    net_list.push_back(net_al);
  }
  return true;
}

void placer_al_t::initialize_HPWL_flags() {
  HPWLX_new = 0;
  HPWLY_new = 0;
  HPWLX_old = 1e30;
  HPWLY_old = 1e30;
  HPWLx_converge = false;
  HPWLy_converge = false;
}

void placer_al_t::uniform_initialization() {
  int Nx = right() - left();
  int Ny = top() - bottom();

  std::default_random_engine generator{0};
  std::uniform_real_distribution<float> distribution(0, 1);
  for (auto &&node: block_list) {
    if (!node.is_movable()) continue;
    node.set_center_dx(left() + Nx*distribution(generator));
    node.set_center_dy(bottom() + Ny*distribution(generator));
    // uniform distribution around the center of placement region
  }
}

void placer_al_t::build_problem_b2b_x(SpMat &eigen_A, Eigen::VectorXd &b) {
  std::vector<T> coefficients;
  coefficients.reserve(10000000);
  for (int i=0; i<b.size(); i++) {
    b[i] = 0;
  }
  double weightX, invP, tmpPinLocX0, tmpPinLocX1, tmpDiffOffset;
  size_t tmpNodeNum0, tmpNodeNum1, maxPinIndex_x, minPinIndex_x;
  for (auto &&net: net_list) {
    if (net.p()==1) continue;
    invP = net.inv_p();
    maxPinIndex_x = net.max_pin_index_x();
    minPinIndex_x = net.min_pin_index_x();
    for (size_t i=0; i<net.pin_list.size(); i++) {
      tmpNodeNum0 = net.pin_list[i].get_block()->num();
      tmpPinLocX0 = block_list[tmpNodeNum0].dllx();
      for (size_t j=i+1; j<net.pin_list.size(); j++) {
        if ((i!=maxPinIndex_x)&&(i!=minPinIndex_x)) {
          if ((j!=maxPinIndex_x)&&(j!=minPinIndex_x)) continue;
        }
        tmpNodeNum1 = net.pin_list[j].get_block()->num();
        if (tmpNodeNum0 == tmpNodeNum1) continue;
        tmpPinLocX1 = block_list[tmpNodeNum1].dllx();
        weightX = invP/(fabs(tmpPinLocX0 - tmpPinLocX1) + width_epsilon());
        if (!block_list[tmpNodeNum0].is_movable() && block_list[tmpNodeNum1].is_movable()) {
          b[tmpNodeNum1] += (tmpPinLocX0 - net.pin_list[j].x_offset()) * weightX;
          coefficients.emplace_back(T(tmpNodeNum1,tmpNodeNum1,weightX));
        } else if (block_list[tmpNodeNum0].is_movable() && !block_list[tmpNodeNum1].is_movable()) {
          b[tmpNodeNum0] += (tmpPinLocX1 - net.pin_list[i].x_offset()) * weightX;
          coefficients.emplace_back(T(tmpNodeNum0,tmpNodeNum0,weightX));
        } else if (block_list[tmpNodeNum0].is_movable() && block_list[tmpNodeNum1].is_movable()){
          coefficients.emplace_back(T(tmpNodeNum0,tmpNodeNum0,weightX));
          coefficients.emplace_back(T(tmpNodeNum1,tmpNodeNum1,weightX));
          coefficients.emplace_back(T(tmpNodeNum0,tmpNodeNum1,-weightX));
          coefficients.emplace_back(T(tmpNodeNum1,tmpNodeNum0,-weightX));
          tmpDiffOffset = (net.pin_list[j].x_offset() - net.pin_list[i].x_offset()) * weightX;
          b[tmpNodeNum0] += tmpDiffOffset;
          b[tmpNodeNum1] -= tmpDiffOffset;
        } else {
          continue;
        }
      }
    }
  }
  for (size_t i=0; i<block_list.size(); ++i) {
    if (!block_list[i].is_movable()) {
      coefficients.emplace_back(T(i,i,1));
      b[i] = block_list[i].dllx();
    }
  }
  eigen_A.setFromTriplets(coefficients.begin(), coefficients.end());
}

void placer_al_t::build_problem_b2b_y(SpMat &eigen_A, Eigen::VectorXd &b) {
  std::vector<T> coefficients;
  coefficients.reserve(10000000);
  for (int i=0; i<b.size(); i++) {
    b[i] = 0;
  }
  double weightY, invP, tmpPinLocY0, tmpPinLocY1, tmpDiffOffset;
  size_t tmpNodeNum0, tmpNodeNum1, maxPinIndex_y, minPinIndex_y;
  for (auto &&net: net_list) {
    if (net.p()==1) continue;
    invP = net.inv_p();
    maxPinIndex_y = net.max_pin_index_y();
    minPinIndex_y = net.min_pin_index_y();
    for (size_t i=0; i<net.pin_list.size(); i++) {
      tmpNodeNum0 = net.pin_list[i].get_block()->num();
      tmpPinLocY0 = block_list[tmpNodeNum0].dlly();
      for (size_t j=i+1; j<net.pin_list.size(); j++) {
        if ((i!=maxPinIndex_y)&&(i!=minPinIndex_y)) {
          if ((j!=maxPinIndex_y)&&(j!=minPinIndex_y)) continue;
        }
        tmpNodeNum1 = net.pin_list[j].get_block()->num();
        if (tmpNodeNum0 == tmpNodeNum1) continue;
        tmpPinLocY1 = block_list[tmpNodeNum1].dlly();
        weightY = invP/((double)fabs(tmpPinLocY0 - tmpPinLocY1) + height_epsilon());
        if (!block_list[tmpNodeNum0].is_movable() && block_list[tmpNodeNum1].is_movable()) {
          b[tmpNodeNum1] += (tmpPinLocY0 - net.pin_list[j].y_offset()) * weightY;
          coefficients.emplace_back(T(tmpNodeNum1,tmpNodeNum1,weightY));
        } else if (block_list[tmpNodeNum0].is_movable() && !block_list[tmpNodeNum1].is_movable()) {
          b[tmpNodeNum0] += (tmpPinLocY1 - net.pin_list[i].y_offset()) * weightY;
          coefficients.emplace_back(T(tmpNodeNum0,tmpNodeNum0,weightY));
        } else if (block_list[tmpNodeNum0].is_movable() && block_list[tmpNodeNum1].is_movable()) {
          coefficients.emplace_back(T(tmpNodeNum0,tmpNodeNum0,weightY));
          coefficients.emplace_back(T(tmpNodeNum1,tmpNodeNum1,weightY));
          coefficients.emplace_back(T(tmpNodeNum0,tmpNodeNum1,-weightY));
          coefficients.emplace_back(T(tmpNodeNum1,tmpNodeNum0,-weightY));
          tmpDiffOffset = (net.pin_list[j].y_offset() - net.pin_list[i].y_offset()) * weightY;
          b[tmpNodeNum0] += tmpDiffOffset;
          b[tmpNodeNum1] -= tmpDiffOffset;
        } else {
          continue;
        }
      }
    }
  }
  for (size_t i=0; i<block_list.size(); ++i) {
    if (!block_list[i].is_movable()) {
      coefficients.emplace_back(T(i,i,1));
      b[i] = block_list[i].dlly();
    }
  }
  eigen_A.setFromTriplets(coefficients.begin(), coefficients.end());
}

void placer_al_t::eigen_cg_solver() {
  std::cout << "Total number of movable cells: " << movable_block_num() << "\n";
  std::cout << "Total number of cells: " << block_num() << "\n";
  int cellNum = block_num();
  Eigen::ConjugateGradient <SpMat> cgx;
  cgx.setMaxIterations(cgIterMaxNum);
  cgx.setTolerance(cgTolerance);
  HPWLx_converge = false;
  HPWLX_old = 1e30;
  Eigen::VectorXd x(cellNum), eigen_bx(cellNum);
  SpMat eigen_Ax(cellNum, cellNum);
  for (size_t i=0; i<block_list.size(); ++i) {
    x[i] = block_list[i].dllx();
  }
  for (int i=0; i<b2bIterMaxNum; ++i) {
    if (HPWLx_converge) {
      std::cout << "iterations:     " << i << "\n";
      break;
    }

    build_problem_b2b_x(eigen_Ax, eigen_bx);
    cgx.compute(eigen_Ax);
    x = cgx.solveWithGuess(eigen_bx, x);
    //std::cout << "Here is the vector x:\n" << x << std::endl;
    std::cout << "\t#iterations:     " << cgx.iterations() << std::endl;
    std::cout << "\testimated error: " << cgx.error() << std::endl;
    for (int num=0; num<x.size(); ++num) {
      block_list[num].set_dllx(x[num]);
    }
    update_max_min_node_x();
  }

  Eigen::ConjugateGradient <SpMat> cgy;
  cgy.setMaxIterations(cgIterMaxNum);
  cgy.setTolerance(cgTolerance);
  // Assembly:
  HPWLy_converge = false;
  HPWLY_old = 1e30;
  Eigen::VectorXd y(cellNum), eigen_by(cellNum); // the solution and the right hand side-vector resulting from the constraints
  SpMat eigen_Ay(cellNum, cellNum); // sparse matrix
  for (size_t i=0; i<block_list.size(); ++i) {
    y[i] = block_list[i].dlly();
  }
  for (int i=0; i<b2bIterMaxNum; ++i) {
    if (HPWLy_converge) {
      std::cout << "iterations:     " << i << "\n";
      break;
    }
    build_problem_b2b_y(eigen_Ay, eigen_by); // fill A and b
    // Solving:
    cgy.compute(eigen_Ay);
    y = cgy.solveWithGuess(eigen_by, y);
    std::cout << "\t#iterations:     " << cgy.iterations() << std::endl;
    std::cout << "\testimated error: " << cgy.error() << std::endl;
    for (int num=0; num<y.size(); ++num) {
      block_list[num].set_dlly(y[num]);
    }
    update_max_min_node_y();
  }
}

void placer_al_t::update_HPWL_x() {
  HPWLX_new = 0;
  for (auto &&net: net_list) {
    HPWLX_new += net.dhpwlx();
  }
}

void placer_al_t::update_max_min_node_x() {
  HPWLX_new = 0;
  for (auto &&net: net_list) {
    HPWLX_new += net.dhpwlx();
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

void placer_al_t::update_HPWL_y() {
  // update the y direction max and min node in each net
  HPWLY_new = 0;
  for (auto &&net: net_list) {
    HPWLY_new += net.dhpwly();
  }
}

void placer_al_t::update_max_min_node_y() {
  // update the y direction max and min node in each net
  HPWLY_new = 0;
  for (auto &&net: net_list) {
    HPWLY_new += net.dhpwly();
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

void placer_al_t::add_boundary_list() {
  boundary_list.clear();
  int max_width=0, max_height=0;
  for (auto &&block: block_list) {
    // find maximum width and maximum height
    if (!block.is_movable()) {
      continue;
    }
    if (block.width() > max_width) {
      max_width = block.width();
    }
    if (block.height() > max_height) {
      max_height = block.height();
    }
  }

  block_al_t tmp_block;
  int multiplier = 500;
  tmp_block.set_movable(false);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_dllx(left() - tmp_block.width());
  tmp_block.set_dlly(top());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(right() - left());
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_dllx(left());
  tmp_block.set_dlly(top());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_dllx(right());
  tmp_block.set_dlly(top());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_width(top() - bottom());
  tmp_block.set_dllx(right());
  tmp_block.set_dlly(bottom());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_dllx(right());
  tmp_block.set_dlly(bottom() - tmp_block.height());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(right() - left());
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_dllx(left());
  tmp_block.set_dlly(bottom() - tmp_block.height());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_dllx(left() - tmp_block.width());
  tmp_block.set_dlly(bottom() - tmp_block.height());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(top() - bottom());
  tmp_block.set_dllx(left() - tmp_block.width());
  tmp_block.set_dlly(bottom());
  boundary_list.push_back(tmp_block);
}

void placer_al_t::initialize_bin_list(){
  // initialize the bin_list, build up the bin_list matrix
  bin_list.clear();
  int max_width=0, max_height=0;
  for (auto &&block: block_list) {
    // find maximum width and maximum height
    if (!block.is_movable()) {
      continue;
    }
    if (block.width() > max_width) {
      max_width = block.width();
    }
    if (block.height() > max_height) {
      max_height = block.height();
    }
  }
  bin_list_size_x = (int)std::ceil((right() - left() + 4*max_width)/(double)max_width); // determine the bin numbers in x direction
  bin_list_size_y = (int)std::ceil((top() - bottom() + 4*max_height)/(double)max_height); // determine the bin numbers in y direction
  bin_width = max_width;
  bin_height = max_height;
  std::vector<bin_t> tmp_bin_column(bin_list_size_y);
  for (int x=0; x<bin_list_size_x; x++) {
    // bin_list[x][y] indicates the bin in the  x-th x, and y-th y bin
    bin_list.push_back(tmp_bin_column);
  }
  int Left_new = left() - (bin_list_size_x * bin_width - (right() - left()))/2;
  int Bottom_new = bottom() - (bin_list_size_y * bin_height - (top() - bottom()))/2;
  virtual_bin_boundary.set_dllx(Left_new);
  virtual_bin_boundary.set_dlly(Bottom_new);
  virtual_bin_boundary.set_width(bin_list_size_x * bin_width);
  virtual_bin_boundary.set_height(bin_list_size_y * bin_height);
  for (size_t x=0; x<bin_list.size(); x++) {
    for (size_t y=0; y<bin_list[x].size(); y++) {
      bin_list[x][y].set_left(Left_new + x*bin_width);
      bin_list[x][y].set_bottom(Bottom_new + y*bin_height);
      bin_list[x][y].set_width(bin_width);
      bin_list[x][y].set_height(bin_height);
    }
  }
}

bool placer_al_t::draw_bin_list(std::string const &filename) {
  std::ofstream ost(filename.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open output file: " << filename << "\n";
    return false;
  }
  for (auto &&bin_column: bin_list) {
    for (auto &&bin: bin_column) {
      ost << "rectangle('Position',[" << bin.left() << " " << bin.bottom() << " " << bin.width() << " " << bin.height() << "],'LineWidth',1)\n";
    }
  }
  ost << "rectangle('Position',[" << left() << " " << bottom() << " " << right() - left() << " " << top() - bottom() << "],'LineWidth',1)\n";
  ost << "axis auto equal\n";
  ost.close();

  return true;
}

void placer_al_t::shift_to_region_center() {
  double leftmost = block_list[0].dllx(), bottommost = block_list[0].dlly(), rightmost = block_list[0].durx(), topmost = block_list[0].dury();
  for (auto &&block: block_list) {
    if (block.dllx() < leftmost) leftmost = block.dllx();
    if (block.dlly() < bottommost) bottommost = block.dlly();
    if (block.durx() > rightmost) rightmost = block.durx();
    if (block.dury() > topmost) topmost = block.dury();
  }
  for (auto &&block: block_list) {
    block.set_dllx(block.dllx() - leftmost + 1/2.0*(right() + left() - rightmost + leftmost));
    block.set_dlly(block.dlly() - bottommost + 1/2.0*(top() + bottom() - topmost + bottommost));
  }
}

bool placer_al_t::draw_block_net_list(std::string const &filename) {
  std::ofstream ost(filename.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open output file: " << filename << "\n";
    return false;
  }
  ost << left() << " " << bottom() << " " << right()-left() << " " << top()-bottom() << "\n";
  for (auto &&block: block_list) {
    ost << block.dllx() << " " << block.dlly() << " " << block.width() << " " << block.height() << "\n";
  }
  /*
  block_al_t *block_ptr0, *block_ptr1;
  for (auto &&net: netList) {
    for (size_t i=0; i<net.pin_list.size(); i++) {
      block_ptr0 = (block_al_t *)net.pin_list[i].get_block();
      for (size_t j=i+1; j<net.pin_list.size(); j++) {
        block_ptr1 = (block_al_t *)net.pin_list[j].get_block();
        ost << "line([" << block_ptr0->dllx() + net.pin_list[i].x_offset() << "," << block_ptr1->dllx() + net.pin_list[j].x_offset()
               << "],[" << block_ptr0->dlly() + net.pin_list[i].y_offset() << "," << block_ptr1->dlly() + net.pin_list[j].y_offset() << "],'lineWidth', 0.5)\n";
      }
    }
  }
   */
  ost.close();

  return true;
}

void placer_al_t::expansion_legalization() {
  double b_left, b_right, b_bottom, b_top;
  b_left = right();
  b_right = left();
  b_top = bottom();
  b_bottom = top();
  int max_height = 0;
  int max_width = 0;
  for (auto &&block: block_list) {
    if (block.is_movable()) {
      if (block.dx() < b_left) {
        b_left = block.dx();
      }
      if (block.dx() > b_right) {
        b_right = block.dx();
      }
      if (block.dy() < b_bottom) {
        b_bottom = block.dy();
      }
      if (block.dy() > b_top) {
        b_top = block.dy();
      }
      if (block.height() > max_height) {
        max_height = block.height();
      }
      if (block.width() > max_width) {
        max_width = block.width();
      }
    }
  }
  double box_width = b_right -b_left;
  double box_height = b_top - b_bottom;

  double mod_region_width = (right() - left())*sqrt(_filling_rate);
  double mod_region_height = (top() - bottom())*sqrt(_filling_rate);
  double mod_left = (left() + right())/2.0 - mod_region_width/2.0;
  double mod_bottom = (bottom() + top())/2.0 - mod_region_height/2.0;

  for (auto &&block: block_list) {
    if (block.is_movable()) {
      block.set_center_dx(mod_left + (block.dx() - b_left)/box_width * mod_region_width);
      block.set_center_dy(mod_bottom + (block.dy() - b_bottom)/box_height * mod_region_height);
    }
  }

  /*
  double gravity_center_x = 0;
  double gravity_center_y = 0;
  for (auto &&block: blockList) {
    if (block.is_movable()) {
      gravity_center_x += block.dx();
      gravity_center_y += block.dy();
    }
  }
  gravity_center_x /= blockList.size();
  gravity_center_y /= blockList.size();
  for (auto &&block: blockList) {
    if (block.is_movable()) {
      block.set_center_dx(block.dx() - (gravity_center_x - (right() - left())/2.0));
      block.set_center_dy(block.dy() - (gravity_center_y - (top() - bottom())/2.0));
    }
  }
   */

}

void placer_al_t::update_block_in_bin() {
  double leftmost=bin_list[0][0].left(), bottommost=bin_list[0][0].bottom();
  int X_bin_list_size = bin_list.size(), Y_bin_list_size = bin_list[0].size();
  int L,B,R,T; // left, bottom, right, top of the cell in which bin
  for (auto &&bin_column: bin_list) {
    for (auto &&bin: bin_column) {
      bin.CIB.clear();
    }
  }
  for (size_t i=0; i<block_list.size(); i++) {
    block_list[i].bin.clear();
    L = floor((block_list[i].dllx() - leftmost)/bin_width);
    B = floor((block_list[i].dlly() - bottommost)/bin_height);
    R = floor((block_list[i].durx() - leftmost)/bin_width);
    T = floor((block_list[i].dury() - bottommost)/bin_height);
    if (R >= X_bin_list_size) {
      R = X_bin_list_size - 1;
    }
    if (T >= Y_bin_list_size) {
      T = Y_bin_list_size - 1;
    }
    if (L < 0) {
      L = 0;
    }
    if (B < 0) {
      B = 0;
    }
    for (int x=L; x<=R; x++) {
      for (int y=B; y<=T; y++) {
        bin_list[x][y].CIB.push_back(i);
        bin_index tmp_bin_loc(x,y);
        block_list[i].bin.push_back(tmp_bin_loc);
      }
    }
  }
}

bool placer_al_t::check_legal() {
  double leftmost = virtual_bin_boundary.dllx(), bottommost = virtual_bin_boundary.dlly();
  for (auto &&block: block_list) {
    if (!block.is_movable()) continue;
    // 1. check overlap with boundaries
    for (auto &&boundary: boundary_list) {
      if (block.is_overlap(boundary)) {
        return false;
      }
    }
    // 2. check overlap with cells
    std::vector< int > temp_physical_neighbor_set;
    int L,B,R,T; // left, bottom, right, top of the cell in which bin
    L = floor((block.dllx() - leftmost) / bin_width);
    B = floor((block.dlly() - bottommost) / bin_height);
    R = floor((block.durx() - leftmost) / bin_width);
    T = floor((block.dury() - bottommost) / bin_height);
    for (int y = B - 1; y <= T + 1; y++) {
      // find all cells around cell i, and put all these cells into the set "temp_physical_neighbor_set", there is a reason to do so is because a cell might appear in different bins
      if ((y >= 0) && (y < bin_list_size_y)) {
        for (int x = L - 1; x <= R + 1; x++) {
          if ((x >= 0) && (x < bin_list_size_x)) {
            for (auto &&cell_num_in_bin: bin_list[x][y].CIB) {
              if (block.num() == (size_t)cell_num_in_bin) {
                continue;
              } else {
                temp_physical_neighbor_set.push_back(cell_num_in_bin);
              }
            }
          }
        }
      }
    }
    for (auto &&block_num: temp_physical_neighbor_set) {
      if (block.is_overlap(block_list[block_num])) {
        return false;
      }
    }
    temp_physical_neighbor_set.clear(); // clear this set after calculate the velocity due to overlap of cells
  }
  return true;
}

void placer_al_t::integerize() {
  for (auto &&block: block_list) {
    block.set_dllx((int)block.dllx());
    block.set_dlly((int)block.dlly());
  }
}

void placer_al_t::update_velocity() {
  double rij, overlap = 0;
  //double maxv1 = 10, maxv2 = 20; // for layout
  double maxv1 = (_circuit->ave_width() + _circuit->ave_height())/20, maxv2 = 2*maxv1;
  for (auto &&block: block_list) {
    block.vx = 0;
    block.vy = 0;
  }

  double leftmost = bin_list[0][0].left(), bottommost = bin_list[0][0].bottom();
  int Ybinlistsize = bin_list[0].size(), Xbinlistsize = bin_list.size();
  double Left, Right, Bottom, Top;

  int L,B,R,T; // left, bottom, right, top of the cell in which bin
  std::set<int> temp_physical_neighbor_set;
  for (size_t i=0; i<block_list.size(); i++) {
    if (!block_list[i].is_movable()) continue;
    Left = block_list[i].dllx(), Right = block_list[i].durx();
    Bottom = block_list[i].dlly(), Top = block_list[i].dury();
    L = floor((Left - leftmost)/bin_width);
    B = floor((Bottom - bottommost)/bin_height);
    R = floor((Right - leftmost)/bin_width);
    T = floor((Top - bottommost)/bin_height);
    for (int y=B-1; y<=T+1; y++) { // find all cells around cell i, and put all these cells into the set "temp_physical_neighbor_set", there is a reason to do so is because a cell might appear in different bins
      if ((y>=0)&&(y<Ybinlistsize)) {
        for (int x=L-1; x<=R+1; x++) {
          if ((x>=0)&&(x<Xbinlistsize)) {
            for (auto &&cell_num_in_bin: bin_list[x][y].CIB) {
              if (i==(size_t)cell_num_in_bin) continue;
              temp_physical_neighbor_set.insert(cell_num_in_bin);
            }
          }
        }
      }
    }

    for (auto &&block_num: temp_physical_neighbor_set) {
      overlap = block_list[i].overlap_area(block_list[block_num]);
      if ((block_list[i].dx() == block_list[block_num].dx()) && (block_list[i].dy() == block_list[block_num].dy())) {
        if (i<(size_t)block_num) {
          block_list[i].vx += 1*block_list[i].area();
          block_list[i].vy += 1*block_list[i].area();
        } else {
          block_list[i].vx -= 1*block_list[i].area();
          block_list[i].vy -= 1*block_list[i].area();
        }
      }
      else {
        rij = sqrt(pow(block_list[i].dx() - block_list[block_num].dx(), 2) +
                   pow(block_list[i].dy() - block_list[block_num].dy(), 2));
        block_list[i].vx += maxv1 * overlap * (block_list[i].dx() - block_list[block_num].dx()) / rij; // the maximum velocity of each cell is 5 at maxmimum
        block_list[i].vy += maxv1 * overlap * (block_list[i].dy() - block_list[block_num].dy()) / rij;
      }
    }
    temp_physical_neighbor_set.clear(); // clear this set after calculate the velocity due to overlap of cells

    for (size_t j=0; j<boundary_list.size(); j++) {
      overlap = block_list[i].overlap_area(boundary_list[j]);
      rij = sqrt(pow(block_list[i].dx() - boundary_list[j].dx(), 2) + pow(block_list[i].dy() - boundary_list[j].dy(), 2));

      //blockList[i].vx += maxv2*overlap*(blockList[i].dx() - boundary_list[j].dx())/rij;
      //blockList[i].vy += maxv2*overlap*(blockList[i].dy() - boundary_list[j].dy())/rij;

      if ((boundary_list[j].dx() > left())&&(boundary_list[j].dx() < right())) {
        block_list[i].vy += maxv2*overlap*(block_list[i].dy() - boundary_list[j].dy())/rij;
      }
      else if ((block_list[j].dy() > bottom())&&(block_list[j].dy() < top())) {
        block_list[i].vx += maxv2*overlap*(block_list[i].dx() - boundary_list[j].dx())/rij;
      }
      else {
        block_list[i].vx += maxv2*overlap*(block_list[i].dx() - boundary_list[j].dx())/rij;
        block_list[i].vy += maxv2*overlap*(block_list[i].dy() - boundary_list[j].dy())/rij;
      }
    }
    block_list[i].vx = block_list[i].vx/block_list[i].area();
    block_list[i].vy = block_list[i].vy/block_list[i].area();
    block_list[i].modif_vx();
    block_list[i].modif_vy();
  }
}

void placer_al_t::update_velocity_force_damping() {
  double rij, overlap = 0, areai;
  double maxv1 = 5, maxv2 = 30;
  for (auto &&block: block_list) {
    block.vx = 0;
    block.vy = 0;
  }

  double leftmost = bin_list[0][0].left(), bottommost = bin_list[0][0].bottom();
  int Ybinlistsize = bin_list[0].size(), Xbinlistsize = bin_list.size();
  double Left, Right, Bottom, Top;

  int L,B,R,T; // left, bottom, right, top of the cell in which bin
  std::set<int> temp_physical_neighbor_set;
  for (size_t i=0; i<block_list.size(); i++) {
    if (!block_list[i].is_movable()) continue;
    Left = block_list[i].dllx(), Right = block_list[i].durx();
    Bottom = block_list[i].dlly(), Top = block_list[i].dury();
    L = floor((Left - leftmost)/bin_width);
    B = floor((Bottom - bottommost)/bin_height);
    R = floor((Right - leftmost)/bin_width);
    T = floor((Top - bottommost)/bin_height);
    for (int y=B-1; y<=T+1; y++) { // find all cells around cell i, and put all these cells into the set "temp_physical_neighbor_set", there is a reason to do so is because a cell might appear in different bins
      if ((y>=0)&&(y<Ybinlistsize)) {
        for (int x=L-1; x<=R+1; x++) {
          if ((x>=0)&&(x<Xbinlistsize)) {
            for (auto &&cell_num_in_bin: bin_list[x][y].CIB) {
              if (i==(size_t)cell_num_in_bin) continue;
              temp_physical_neighbor_set.insert(cell_num_in_bin);
            }
          }
        }
      }
    }

    double force_x = 0;
    double force_y = 0;
    for (auto &&block_num: temp_physical_neighbor_set) {
      overlap = block_list[i].overlap_area(block_list[block_num]);
      rij = sqrt(pow(block_list[i].dx() - block_list[block_num].dx(), 2) + pow(block_list[i].dy()-block_list[block_num].dy(), 2));
      force_x += maxv1*overlap*(block_list[i].dx() - block_list[block_num].dx())/rij;
      force_y += maxv1*overlap*(block_list[i].dy() - block_list[block_num].dy())/rij;
    }
    temp_physical_neighbor_set.clear(); // clear this set after calculate the velocity due to overlap of cells

    for (size_t j=0; j<boundary_list.size(); j++) {
      overlap = block_list[i].overlap_area(boundary_list[j]);
      rij = sqrt(pow(block_list[i].dx() - boundary_list[j].dx(), 2) + pow(block_list[i].dy() - boundary_list[j].dy(), 2));
      if ((boundary_list[j].dx() > left())&&(boundary_list[j].dx() < right())) {
        force_y += maxv2*overlap*(block_list[i].dy() - boundary_list[j].dy())/rij;
      }
      else if ((block_list[j].dy() > bottom())&&(block_list[j].dy() < top())) {
        force_x += maxv2*overlap*(block_list[i].dx() - boundary_list[j].dx())/rij;
      }
      else {
        force_x += maxv2*overlap*(block_list[i].dx() - boundary_list[j].dx())/rij;
        force_y += maxv2*overlap*(block_list[i].dy() - boundary_list[j].dy())/rij;
      }

    }
    areai = block_list[i].area();
    force_x = force_x/areai;
    force_y = force_y/areai;
    block_list[i].modif_vx();
    block_list[i].modif_vy();
  }
}

void placer_al_t::update_position() {
  for (auto &&block:block_list) {
    block.update_loc(time_step);
  }
}

void placer_al_t::diffusion_legalization() {
  for (int i=0; i< iteration_limit_diffusion; i++) {
    update_block_in_bin();
    update_velocity();
    update_position();
  }
  update_block_in_bin();
}

bool placer_al_t::legalization() {
  update_block_in_bin();
  time_step = 1;
  for (int i=0; i<max_legalization_iteration; i++) {
    if (check_legal()) {
      integerize();
      if (check_legal()) {
        std::cout << i << " iterations\n";
        break;
      }
    }
    diffusion_legalization();
    if (time_step > 1) time_step -= 1;
    if (i==max_legalization_iteration-1) {
      std::cout << "Molecular-dynamic legalization finish\n";
      return false;
    }
    /*if (i%20 == 0) {
      std::string tmpFileName = "legalization" + std::to_string(i) + ".m";
      draw_block_net_list(tmpFileName);
    }*/
  }
  std::cout << "Molecular-dynamic legalization succeeds\n";
  return true;
}

void placer_al_t::diffusion_with_gravity() {
  double drift_v = -(_circuit->ave_width() + _circuit->ave_height())/300;
  for (int i=0; i<max_legalization_iteration; i++) {
    update_block_in_bin();
    update_velocity();
    for (auto &&block: block_list) {
      if (i%2==0) {
        block.add_gravity_vx(drift_v);
      } else {
        block.add_gravity_vy(drift_v);
      }
    }
    update_position();
  }
}

void placer_al_t::diffusion_with_gravity2() {
  for (auto &&block: block_list) {
    if (block.dllx() < left()) {
      block.set_dllx(left());
    }
    if (block.dlly() < bottom()) {
      block.set_dlly(bottom());
    }
    if (block.durx() > right()) {
      block.set_durx(right());
    }
    if (block.dury() > top()) {
      block.set_dury(top());
    }
  }

  std::vector< size_t > blockXOrder(block_list.size());
  for (size_t i=0; i<blockXOrder.size(); i++) {
    blockXOrder[i] = i;
  }
  for (size_t i=0; i<blockXOrder.size(); i++) {
    size_t minBlockNum = i;
    for (size_t j=i+1; j<blockXOrder.size(); j++) {
      if (block_list[blockXOrder[j]].dllx() < block_list[blockXOrder[minBlockNum]].dllx()) {
        minBlockNum = j;
      } else if (block_list[blockXOrder[j]].dllx() == block_list[blockXOrder[minBlockNum]].dllx()) {
        if (block_list[blockXOrder[j]].dlly() < block_list[blockXOrder[minBlockNum]].dlly()) {
          minBlockNum = j;
        }
      }
    }
    size_t tmpNum = blockXOrder[i];
    blockXOrder[i] = blockXOrder[minBlockNum];
    blockXOrder[minBlockNum] = tmpNum;
  }
  for (auto &&blockNum: blockXOrder) {
    std::cout << blockNum << " " << block_list[blockNum].dllx() << " " << block_list[blockNum].dlly() << "\n";
  }

  update_block_in_bin();

  /*for (auto &&blockNum: blockXOrder) {
    //while(false);
    if (isMoveLegal(&block_list[blockNum],-1,0)) {
      block_list[blockNum].move(-1,0);
    }
  }*/
}

bool placer_al_t::tetris_legalization() {
  //draw_block_net_list("before_tetris_legalization.m");

  // 1. move all blocks into placement region
  for (auto &&block: block_list) {
    if (block.dllx() < left()) {
      block.set_dllx(left());
    }
    if (block.dlly() < bottom()) {
      block.set_dlly(bottom());
    }
    if (block.durx() > right()) {
      block.set_durx(right());
    }
    if (block.dury() > top()) {
      block.set_dury(top());
    }
  }

  // 2. sort blocks based on their lower left corners
  std::vector< size_t > blockXOrder(block_list.size());
  for (size_t i=0; i<blockXOrder.size(); i++) {
    blockXOrder[i] = i;
  }
  for (size_t i=0; i<blockXOrder.size(); i++) {
    size_t minBlockNum = i;
    for (size_t j=i+1; j<blockXOrder.size(); j++) {
      if (block_list[blockXOrder[j]].dllx() < block_list[blockXOrder[minBlockNum]].dllx()) {
        minBlockNum = j;
      } else if (block_list[blockXOrder[j]].dllx() == block_list[blockXOrder[minBlockNum]].dllx()) {
        if (block_list[blockXOrder[j]].dlly() < block_list[blockXOrder[minBlockNum]].dlly()) {
          minBlockNum = j;
        }
      }
    }
    size_t tmpNum = blockXOrder[i];
    blockXOrder[i] = blockXOrder[minBlockNum];
    blockXOrder[minBlockNum] = tmpNum;
  }

  // 3. initialize the data structure to store row usage
  double aveHeight = 0;
  double minWidth = block_list[0].width();
  for (auto &&block: block_list) {
    if (block.height() > aveHeight) {
      aveHeight = block.height();
    }
    if (block.width() < minWidth) {
      minWidth = block.width();
    }
  }
  int totRowNum = (int)(std::round((top()-bottom())/aveHeight));

  std::vector< double > tetrisMap(totRowNum);
  for (auto &&tetrisRowFill: tetrisMap) {
    tetrisRowFill = left();
  }
  bool allRowOerflow = false;
  for (auto &&orderedBlockNum: blockXOrder) {
    double minDisplacement = right() - left() + top() - bottom();
    int targetRow = 0;
    allRowOerflow = true;
    for (auto &&rowLengthUsed: tetrisMap) {
      if (rowLengthUsed + minWidth < right()) {
        allRowOerflow = false;
        break;
      }
    }
    for (size_t i=0; i<tetrisMap.size(); i++) {
      if (!allRowOerflow) {
        if (tetrisMap[i] + block_list[orderedBlockNum].width() > right()) {
          continue;
        }
      }
      double tetrisllx = tetrisMap[i] + left();
      double tetrislly = i*aveHeight + bottom();
      double displacement = fabs(block_list[orderedBlockNum].dllx() - tetrisllx) + fabs(block_list[orderedBlockNum].dlly() - tetrislly);
      if (tetrisMap[i] > right()) {
        displacement += (tetrisMap[i] - right())*5;
      }
      if (displacement < minDisplacement) {
        minDisplacement = displacement;
        targetRow = i;
      }
    }
    block_list[orderedBlockNum].set_dllx(tetrisMap[targetRow]);
    block_list[orderedBlockNum].set_dlly(targetRow * aveHeight + bottom());
    tetrisMap[targetRow] += block_list[orderedBlockNum].width();
  }
  //draw_block_net_list("after_tetris.m");

  if (!check_legal()) {
    std::cout << "Tetris legalization finish\n";
    return false;
  }
  std::cout << "Tetris legalization succeeds\n";
  return true;
}

bool placer_al_t::tetris_legalization2() {
  //draw_block_net_list("before_tetris_legalization.m");
  std::cout << "Start tetris legalization" << std::endl;
  // 1. move all blocks into placement region
  for (auto &&block: block_list) {
    if (block.dllx() < left()) {
      block.set_dllx(left());
    }
    if (block.dlly() < bottom()) {
      block.set_dlly(bottom());
    }
    if (block.durx() > right()) {
      block.set_durx(right());
    }
    if (block.dury() > top()) {
      block.set_dury(top());
    }
  }

  // 2. sort blocks based on their lower left corners. Further optimization is doable here.
  struct indexLocPair{
    int num;
    double x;
    double y;
  };
  std::vector< indexLocPair > blockXOrder(block_list.size());
  for (size_t i=0; i<blockXOrder.size(); i++) {
    blockXOrder[i].num = i;
    blockXOrder[i].x = block_list[i].dllx();
    blockXOrder[i].y = block_list[i].dlly();
  }
  struct {
    bool operator()(indexLocPair &a, indexLocPair &b) {
      return (a.x < b.x) || ((a.x == b.x) && (a.y < b.y));
    }
  } customLess;
  std::sort(blockXOrder.begin(), blockXOrder.end(), customLess);

  // 3. initialize the data structure to store row usage
  int maxHeight = 0;
  int minWidth = block_list[0].width();
  int minHeight = block_list[0].height();
  for (auto &&block: block_list) {
    if (block.height() > maxHeight) {
      maxHeight = block.height();
    }
    if (block.height() < minHeight) {
      minHeight = block.height();
    }
    if (block.width() < minWidth) {
      minWidth = block.width();
    }
  }

  std::cout << "Building tetris space" << std::endl;
  //TetrisSpace tetrisSpace(left(), right(), bottom(), top(), maxHeight, minWidth);
  //TetrisSpace tetrisSpace(left(), right(), bottom(), top(), minHeight, minWidth);
  TetrisSpace tetrisSpace(left(), right(), bottom(), top(), (int)(std::ceil(minHeight/2.0)), minWidth);
  //TetrisSpace tetrisSpace(left(), right(), bottom(), top(), 1, minWidth);
  //tetrisSpace.show();
  std::cout << "Placing blocks:\n";
  int numPlaced = 0;
  int barWidth = 70;
  int approxNum = blockXOrder.size()/100;
  int quota = std::max(approxNum,1);
  double progress = 0;
  for (auto &&blockNum: blockXOrder) {
    //std::cout << "Placing block: " << blockNum.num << std::endl;
    Loc2D result = tetrisSpace.findBlockLocation(block_list[blockNum.num].dllx(), block_list[blockNum.num].dlly(), block_list[blockNum.num].width(), block_list[blockNum.num].height());
    //std::cout << "Loc to set: " << result.x << " " << result.y << std::endl;
    block_list[blockNum.num].set_dllx(result.x);
    block_list[blockNum.num].set_dlly(result.y);
    //draw_block_net_list("during_tetris.m");

    ++numPlaced;
    if (numPlaced%quota == 0) {
      progress = (double)(numPlaced)/blockXOrder.size();
      std::cout << "[";
      int pos = (int) (barWidth * progress);
      for (int i = 0; i < barWidth; ++i) {
        if (i < pos) std::cout << "=";
        else if (i == pos) std::cout << ">";
        else std::cout << " ";
      }
      std::cout << "] " << int(progress * 100.0) << " %\r";
      //std::cout << std::endl;
      std::cout.flush();
    }
  }
  for (int i = 0; i < barWidth; ++i) {
    if (i < barWidth) std::cout << "=";
    else if (i == barWidth) std::cout << ">";
    else std::cout << " ";
  }
  std::cout << "] " << 100 << " %\n";

  //draw_block_net_list("after_tetris.txt");
  if (!check_legal()) {
    std::cout << "Tetris legalization finish\n";
    return false;
  }
  std::cout << "Tetris legalization succeeds\n";
  return true;
}

bool placer_al_t::gravity_legalization() {
  update_block_in_bin();
  diffusion_with_gravity();
  time_step = 5;
  for (int i=0; i<max_legalization_iteration; i++) {
    if (check_legal()) {
      integerize();
      if (check_legal()) {
        std::cout << i << " iterations\n";
        break;
      }
    }
    diffusion_legalization();
    if (time_step > 1) time_step -= 1;
    if (i==max_legalization_iteration-1) {
      std::cout << "Gravity legalization finish\n";
      return false;
    }
  }
  std::cout << "Gravity legalization succeeds\n";
  return true;
}

bool placer_al_t::post_legalization_optimization() {

  return true;
}

bool placer_al_t::start_placement() {
  uniform_initialization();
  draw_block_net_list("ui.txt");
  eigen_cg_solver();
  std::cout << "Initial Placement Complete\n";
  report_hpwl();

  draw_block_net_list("cg_result.txt");
  /*if (!tetris_legalization2()) {
    report_hpwl();
    std::cout << "Legalization fail\n";
    return false;
  }*/

  post_legalization_optimization();
  std::cout << "Legalization Complete\n";
  report_hpwl();
  //draw_block_net_list();
  return true;
}

void placer_al_t::report_placement_result() {
  for (size_t i=0; i<block_list.size(); i++) {
    if (block_list[i].is_movable()) {
      _circuit->blockList[i].set_llx((int)block_list[i].dllx());
      _circuit->blockList[i].set_lly((int)block_list[i].dlly());
    }
  }
}

void placer_al_t::report_hpwl() {
  update_HPWL_x();
  update_HPWL_y();
  std::cout << "HPWL: " << HPWLX_new + HPWLY_new << "\n";
}
