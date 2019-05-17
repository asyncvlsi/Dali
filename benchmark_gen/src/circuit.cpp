//
// Created by Yihang Yang on 2019-05-16.
//

#include <fstream>
#include <iostream>
#include "circuit.hpp"

circuit_t::circuit_t() {
  LEFT = 0;
  RIGHT = 0;
  BOTTOM = 0;
  TOP = 0;
  TARGET_FILLING_RATE = 1;
}

#include "circuit_random_gen.cpp"
#include "circuit_io.cpp"
