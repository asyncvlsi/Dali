//
// Created by Yihang Yang on 2019-07-25.
//

#ifndef HPCC_SRC_COMMON_MISC_H_
#define HPCC_SRC_COMMON_MISC_H_

#include <cassert>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include "global.h"
#include <cmath>
#include <stdexcept>

struct Value2D {
  double x;
  double y;
  explicit Value2D(double x_init=0, double y_init=0): x(x_init), y(y_init) {}

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

  void Incre(const Value2D& a) {
    x += a.x;
    y += a.y;
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

  Value2D operator*(const double scale) const {
    return Value2D(x*scale, y*scale);
  }

  Value2D operator*=(const double scale) {
    x *= scale;
    y *= scale;
    return *this;
  }

  void Init() {
    x = 0;
    y = 0;
  }
};

struct Loc2D {
  int x;
  int y;
  explicit Loc2D(int initX, int initY): x(initX), y(initY) {};
};

void Assert(bool e, const std::string &error_message);
void Warning(bool e, const std::string &warning_message);
void StrSplit(std::string &line, std::vector<std::string> &res);
int FindFirstDigit(std::string &str);

class NotImplementedException: public std::logic_error {
 public:
  NotImplementedException(): std::logic_error("Function not yet implemented.") {};
};

#endif //HPCC_SRC_COMMON_MISC_H_
