//
// Created by Yihang Yang on 2019-07-25.
//

#ifndef DALI_SRC_COMMON_MISC_H_
#define DALI_SRC_COMMON_MISC_H_

#include <cassert>
#include <cmath>

#include <stdexcept>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>

#include "global.h"

template<class T>
struct Value2D {
  T x;
  T y;
  explicit Value2D(T x_init = 0, T y_init = 0) : x(x_init), y(y_init) {}

  // assignment operator modifies object, therefore non-const
  Value2D &operator=(const Value2D &a) = default;

  // add operator. doesn't modify object. therefore const.
  Value2D operator+(const Value2D &a) const {
    return Value2D(a.x + x, a.y + y);
  }

  Value2D operator+=(const Value2D &a) {
    x += a.x;
    y += a.y;
    return *this;
  }

  void Incre(const Value2D &a) {
    x += a.x;
    y += a.y;
  }

  // equality comparison. doesn't modify object. therefore const.
  bool operator==(const Value2D &a) const {
    return (x == a.x && y == a.y);
  }

  void Normalize() {
    T amp = sqrt(x * x + y * y);
    x /= amp;
    y /= amp;
  }

  Value2D operator*(const double scale) const {
    return Value2D(x * scale, y * scale);
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

typedef Value2D<double> double2d;
typedef Value2D<int> int2d;

template<class T>
struct Rect {
 private:
  T llx_, lly_, urx_, ury_;
 public:
  Rect() : llx_(0), lly_(0), urx_(0), ury_(0) {}
  Rect(T llx, T lly, T urx, T ury) : llx_(llx), lly_(lly), urx_(urx), ury_(ury) {}
  bool operator==(Rect<T> const &rhs) const {
    return (llx_ == rhs.llx_) && (lly_ == rhs.llx_) && (urx_ == rhs.llx_) && (ury_ == rhs.llx_);
  }
  T LLX() const { return llx_; }
  T LLY() const { return lly_; }
  T URX() const { return urx_; }
  T URY() const { return ury_; }
  void SetValue(T llx, T lly, T urx, T ury) {
    llx_ = llx;
    lly_ = lly;
    urx_ = urx;
    ury_ = ury;
  }
};

typedef Rect<double> RectD;
typedef Rect<int> RectI;

template<class T>
struct IndexLocPair {
  int num;
  T x;
  T y;
  explicit IndexLocPair(int num_init = 0, T x_init = 0, T y_init = 0) : num(num_init), x(x_init), y(y_init) {}
  bool operator<(const IndexLocPair &rhs) const {
    return (x < rhs.x) || ((x == rhs.x) && (y < rhs.y));
  }
};

void Assert(bool e, const std::string &error_message);
void Warning(bool e, const std::string &warning_message);
void StrSplit(std::string &line, std::vector<std::string> &res);
int FindFirstDigit(std::string &str);

class NotImplementedException : public std::logic_error {
 public:
  NotImplementedException() : std::logic_error("Function not yet implemented.") {};
};

#endif //DALI_SRC_COMMON_MISC_H_
