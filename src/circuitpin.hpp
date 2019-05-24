//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_CIRCUITPIN_HPP
#define HPCC_CIRCUITPIN_HPP

#include <string>
#include <iostream>
#include "circuitblock.hpp"

class pin_t {
private:
  std::string _block_name;
  int _x_offset;
  int _y_offset;

  /* the following entries are derived data */
  block_t* _block; // pointer pointing to the block entry in block_list for HPWL calculation in net

public:
  pin_t();
  pin_t(std::string &blockName, int xOffset, int yOffset);

  bool operator ==(const pin_t &rhs) const;
  friend std::ostream& operator<<(std::ostream& os, const pin_t &pin) {
    os << pin._block_name << " (" << pin._x_offset << ", " << pin._y_offset << ")";
    return os;
  }
  void set_x_offset(int xOffset);
  void set_y_offset(int yOffset);
  int x_offset();
  int y_offset();
  void set_block_point(block_t *block);
  block_t* get_block();
};


#endif //HPCC_CIRCUITPIN_HPP
