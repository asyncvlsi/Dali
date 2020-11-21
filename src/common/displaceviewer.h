//
// Created by yihang on 3/11/20.
//

#ifndef DALI_SRC_COMMON_DISPLACEVIEWER_H_
#define DALI_SRC_COMMON_DISPLACEVIEWER_H_

#include <fstream>
#include <string>
#include <vector>

#include "misc.h"

namespace dali {

template<class T>
struct DisplaceViewer {
 private:
  int sz_;
  std::vector<T> x_;
  std::vector<T> y_;
  std::vector<T> u_;
  std::vector<T> v_;

 public:
  DisplaceViewer() = default;

  void SetSize(int sz) {
    sz_ = sz;
    x_.resize(sz);
    y_.resize(sz);
    u_.resize(sz);
    v_.resize(sz);
  }

  void SetXY(int i, T x_val, T y_val) {
    x_[i] = x_val;
    y_[i] = y_val;
  }
  void SetUV(int i, T u_val, T v_val) {
    u_[i] = u_val;
    v_[i] = v_val;
  }
  void SetXYFromDifference(int i, T x_prime, T y_prime) {
    u_[i] = x_prime - x_[i];
    v_[i] = y_prime - y_[i];
  }

  void SaveDisplacementVector(std::string const &name_of_file) {
    std::ofstream ost(name_of_file.c_str());
    Assert(ost.is_open(), "Cannot open output file: " + name_of_file);
    for (int i = 0; i < sz_; ++i) {
      ost << x_[i] << "  " << y_[i] << "  " << u_[i] << "  " << v_[i] << "\n";
    }
    ost.close();
  }
};

}

#endif //DALI_SRC_COMMON_DISPLACEVIEWER_H_
