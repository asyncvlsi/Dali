/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/

#ifndef DALI_DALI_COMMON_MISC_H_
#define DALI_DALI_COMMON_MISC_H_

#include <cassert>
#include <cmath>
#include <cstdlib>

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace dali {

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
  Rect(T llx, T lly, T urx, T ury)
      : llx_(llx), lly_(lly), urx_(urx), ury_(ury) {}
  bool operator==(Rect<T> const &rhs) const {
    return (llx_ == rhs.llx_)
        && (lly_ == rhs.llx_)
        && (urx_ == rhs.llx_)
        && (ury_ == rhs.llx_);
  }
  T LLX() const { return llx_; }
  T LLY() const { return lly_; }
  T URX() const { return urx_; }
  T URY() const { return ury_; }
  T Height() const { return ury_ - lly_; }
  T Width() const { return urx_ - llx_; }
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
  explicit IndexLocPair(
      int num_init = 0,
      T x_init = 0,
      T y_init = 0
  ) : num(num_init),
      x(x_init),
      y(y_init) {}
  bool operator<(const IndexLocPair &rhs) const {
    return (x < rhs.x) || ((x == rhs.x) && (y < rhs.y));
  }
};

template<class T>
struct PtrLocPair {
  T *ptr;
  int x;
  int y;
  explicit PtrLocPair(T *ptr_init = nullptr, int x_init = 0, int y_init = 0)
      : ptr(ptr_init), x(x_init), y(y_init) {}
  bool operator<(const PtrLocPair &rhs) const {
    return (x < rhs.x) || ((x == rhs.x) && (y < rhs.y));
  }
};

template<class T>
struct Seg {
  T lo;
  T hi;
  explicit Seg(T lo_init = 0, T hi_init = 0) : lo(lo_init), hi(hi_init) {}
  T Span() const {
    return hi - lo;
  }
  bool Overlap(Seg<T> const &rhs) {
    return (lo < rhs.hi) && (hi > rhs.lo);
  }
  Seg<T> *Joint(Seg<T> const &rhs) {
    Seg<T> *res = nullptr;
    if (Overlap(rhs)) {
      res = new Seg<T>();
      res->lo = std::max(lo, rhs.lo);
      res->hi = std::min(hi, rhs.hi);
    }
    return res;
  }
};

struct IndexVal {
  int col;
  double val;
  IndexVal(int col_init, double val_init) : col(col_init), val(val_init) {}
};

typedef Seg<int> SegI;

}

#endif //DALI_DALI_COMMON_MISC_H_
