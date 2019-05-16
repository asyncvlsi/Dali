//
// Created by Yihang Yang on 2019-03-26.
//

#include <fstream>
#include <iostream>
#include "circuit_t.hpp"

circuit_t::circuit_t() {
  CELL_NUM = 0;
  TERMINAL_NUM = 0;
  LEFT = 0;
  RIGHT = 0;
  BOTTOM = 0;
  TOP = 0;
  AVE_CELL_AREA = 0;
  AVE_WIDTH = 0;
  AVE_HEIGHT = 0;
  WEPSI = 0;
  HEPSI = 0;
  cg_precision = 0.05;
  HPWL_intra_linearSolver_precision = 0.05;
  HPWLX_new = 0;
  HPWLY_new = 0;
  HPWLX_old = 1e30;
  HPWLY_old = 1e30;
  HPWLx_converge = false;
  HPWLy_converge = false;
  HPWL_inter_linearSolver_precision = 0.1;
  TARGET_FILLING_RATE = 1;
  GRID_NUM = 100;
  GRID_BIN_WIDTH = 0;
  GRID_BIN_HEIGHT = 0;
  ALPHA = 0.01;
  BETA = 1.06;
  std_cell_height = 0;
  real_LEFT = 0;
  real_RIGHT = 0;
  real_BOTTOM = 0;
  real_TOP = 0;
}
