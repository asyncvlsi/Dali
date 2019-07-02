//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_PIN_HPP
#define HPCC_PIN_HPP

#include <string>
#include <iostream>
#include <cassert>
#include "blocktype.hpp"
#include "rect.hpp"

class pin_t {
protected:
  /*** essential data entries ****/
  block_type_t* _block_type; // pointer pointing to the type of block which this pin belongs to
  std::string _pin_name; // the name of this pin, for example "in", "out", "a", "b", etc.
  std::vector< rect_t > _rect_v; // the array of rectangle(s) consisting of this pin

  /**** derived data entries ****/
  /**** this is just to simplify the location of a pin,
   * we just choose the middle point of the first rectangle as the location of this pin, at least for now
   */
  int _x_offset;
  int _y_offset;

public:
  pin_t(block_type_t *blockType, std::string &pinName, int xOffset=0, int yOffset=0);

  bool operator ==(const pin_t &rhs) const;
  friend std::ostream& operator<<(std::ostream& os, const pin_t &pin) {
    os << pin.name() << " (" << pin._x_offset << ", " << pin._y_offset << ")";
    return os;
  }

  std::string block_name() const;
  std::string name() const;
  void set_x_offset(int xOffset);
  void set_y_offset(int yOffset);
  int x_offset();
  int y_offset();
  void set_block_point(block_type_t *blockType);
  block_type_t* get_block();
};


#endif //HPCC_PIN_HPP
