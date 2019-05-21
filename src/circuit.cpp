//
// Created by Yihang Yang on 2019-03-26.
//

#include <fstream>
#include <iostream>
#include "circuit.hpp"

circuit_t::circuit_t() {
  CELL_NUM = 0;
  TERMINAL_NUM = 0;
  LEFT = 0;
  RIGHT = 0;
  BOTTOM = 0;
  TOP = 0;
  TARGET_FILLING_RATE = 1;
  WHITE_SPACE_NODE_RATE = 1;
}

#include "circuit_io.cpp"
