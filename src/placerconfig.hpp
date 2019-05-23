//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_PLACER_HPP
#define HPCC_PLACER_HPP


class placer_config {
  int LEFT, RIGHT, BOTTOM, TOP;
  // Boundaries of the chip, calculated in global_placer.hpp, used in conjugate_gradient.hpp
  float TARGET_FILLING_RATE, WHITE_SPACE_NODE_RATE;
  // target density of cells, movable cells density in each bin cannot exceed this density constraint
};


#endif //HPCC_PLACER_HPP
