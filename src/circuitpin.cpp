//
// Created by Yihang Yang on 2019-05-23.
//

#include "circuitpin.hpp"

pin_t::pin_t() {
  _block_name = "";
  _x_offset = 0;
  _y_offset = 0;
}

pin_t::pin_t(std::string &blockName, int xOffset, int yOffset)
              :_block_name(blockName), _x_offset(xOffset), _y_offset(yOffset) {}

bool pin_t::operator ==(const pin_t &rhs) const {
  return ((_block_name == rhs._block_name) && (_x_offset == rhs._x_offset) && (_y_offset == rhs._y_offset));
}
