//
// Created by Yihang Yang on 2019-07-25.
//

#ifndef HPCC_SRC_COMMON_MISC_H_
#define HPCC_SRC_COMMON_MISC_H_

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>
#include "global.h"

struct Value2D {
  double x;
  double y;
  explicit Value2D(double x=0, double y=0): x(x), y(y) {}

  // assignment operator modifies object, therefore non-const
  Value2D& operator=(const Value2D& a) = default;

  // addop. doesn't modify object. therefore const.
  Value2D operator+(const Value2D& a) const {
    return Value2D(a.x+x, a.y+y);
  }

  Value2D operator+=(const Value2D& a) {
    x += a.x;
    y += a.y;
    return *this;
  }

  // equality comparison. doesn't modify object. therefore const.
  bool operator==(const Value2D& a) const {
    return (x == a.x && y == a.y);
  }

  void Normalize() {
    double amp = sqrt(x*x + y*y);
    x /= amp;
    y /= amp;
  }

  Value2D& LinearScale(double f) {
    x *= f;
    y *= f;
    return *this;
  }
};

void Assert(bool e, const std::string &error_message);
void Warning(bool e, const std::string &warning_message);
void VerbosePrint(VerboseLevel verbose_level, std::stringstream &buf);

class NotImplementedException : public std::logic_error {
 public:
  NotImplementedException () : std::logic_error{"Function not yet implemented."} {}
};

#endif //HPCC_SRC_COMMON_MISC_H_
