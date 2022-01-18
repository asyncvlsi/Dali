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

#ifndef DALI_DALI_COMMON_DISPLACEVIEWER_H_
#define DALI_DALI_COMMON_DISPLACEVIEWER_H_

#include <fstream>
#include <string>
#include <vector>

#include "logging.h"
#include "misc.h"

namespace dali {

template<class T>
struct DisplaceViewer {
 private:
  size_t sz_ = 0;
  std::vector<T> x_;
  std::vector<T> y_;
  std::vector<T> u_;
  std::vector<T> v_;

 public:
  DisplaceViewer() = default;

  void SetSize(size_t sz) {
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
    DaliExpects(ost.is_open(), "Cannot open output file: " + name_of_file);
    for (int i = 0; i < sz_; ++i) {
      ost << x_[i] << "  " << y_[i] << "  " << u_[i] << "  " << v_[i] << "\n";
    }
    ost.close();
  }
};

}

#endif //DALI_DALI_COMMON_DISPLACEVIEWER_H_
