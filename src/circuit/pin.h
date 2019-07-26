//
// Created by Yihang Yang on 2019-05-23.
//

#ifndef HPCC_PIN_HPP
#define HPCC_PIN_HPP

#include <string>
#include <iostream>
#include <vector>
#include "common/misc.h"
#include "rect.h"

class Pin {
 private:
  /*** essential data entries ****/
  std::string name_; // the Name of this pin, for example "in", "out", "a", "b", etc.
  std::vector<Rect> rect_list_;

  /**** derived data ****/
  double x_offset_;
  double y_offset_;

 public:
  explicit Pin(std::string &name);

  const std::string *Name() const;
  void InitOffset();
  double XOffset() const;
  double YOffset() const;

  void AddRect(Rect &rect);
  void AddRect(int llx, int lly, int urx, int ury);

  friend std::ostream& operator<<(std::ostream& os, const Pin &pin) {
    os << pin.Name() << " (" << pin.x_offset_ << ", " << pin.y_offset_ << ")";
    return os;
  }
};


#endif //HPCC_PIN_HPP
