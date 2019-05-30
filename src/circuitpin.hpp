//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_CIRCUITPIN_HPP
#define HPCC_CIRCUITPIN_HPP

#include <string>
#include <iostream>
#include <cassert>
#include "circuitblock.hpp"

class pin_t {
protected:
  int _x_offset;
  int _y_offset;

  /* the following entries are derived data */
  block_t* _block; // pointer pointing to the block entry in block_list for HPWL calculation in net

public:
  pin_t();
  pin_t(int xOffset, int yOffset, block_t *block);

  bool operator ==(const pin_t &rhs) const;
  friend std::ostream& operator<<(std::ostream& os, const pin_t &pin) {
    os << pin.name() << " (" << pin._x_offset << ", " << pin._y_offset << ")";
    return os;
  }

  std::string name() const;
  void set_x_offset(int xOffset);
  void set_y_offset(int yOffset);
  int x_offset();
  int y_offset();
  void set_block_point(block_t *block);
  block_t* get_block();
  int abs_x();
  int abs_y();
};


#endif //HPCC_CIRCUITPIN_HPP
