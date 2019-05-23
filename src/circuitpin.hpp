//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_CIRCUITPIN_HPP
#define HPCC_CIRCUITPIN_HPP

#include <string>
#include <iostream>

class pin_t {
private:
  std::string _block_name;
  int _x_offset;
  int _y_offset;
public:
  pin_t();
  pin_t(std::string &blockName, int xOffset, int yOffset);
  bool operator ==(const pin_t &rhs) const;
  friend std::ostream& operator<<(std::ostream& os, const pin_t &pin) {
    os << pin._block_name << " (" << pin._x_offset << ", " << pin._y_offset << ")";
    return os;
  }
};


#endif //HPCC_CIRCUITPIN_HPP
