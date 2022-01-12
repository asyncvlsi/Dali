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
#ifndef DALI_DALI_PLACER_WELL_LEGALIZER_HELPER_H_
#define DALI_DALI_PLACER_WELL_LEGALIZER_HELPER_H_

#include <string>
#include <vector>

#include "stripe.h"

namespace dali {

void GenClusterTable(
    std::string const &name_of_file,
    std::vector<ClusterStripe> &col_list_
);

void CollectWellFillingRects(
    Stripe &stripe,
    int bottom_boundary,
    int top_boundary,
    std::vector<RectI> &n_rects, std::vector<RectI> &p_rects
);

void GenMATLABWellFillingTable(
    std::string const &base_file_name,
    std::vector<ClusterStripe> &col_list,
    int bottom_boundary,
    int top_boundary,
    int well_emit_mode = 0
);

/****
 * @brief A structure containing information of a block for minimizing displacement
 */
struct BlkDispVar {
  int w;                     // width of this block
  double x_0;                // initial location
  double e;                  // weight of initial location
  double x_a;                // anchor location
  double a;                  // weight of anchor location
  double x;                  // place to store final location
  Block *blk_ptr;            // pointer to the block
  BlkDispVar(int width, double x_init, double weight = 1.0) :
      w(width),
      x_0(x_init),
      e(weight),
      x_a(0),
      a(0),
      x(0),
      blk_ptr(nullptr) {}

  int Width() { return w; }
  double InitX() { return x_0; }
  double Weight() { return e; }
  double AnchorX() { return x_a; }
  double AnchorWeight() { return a; };
  double Solution() { return x; }
  Block *BlkPtr() { return blk_ptr; }

  void SetAnchor(double anchor, double anchor_weight) {
    x_a = anchor;
    a = anchor_weight;
    double sum_weight_anchor = e * x_0 + a * x_a;
    double sum_weight = e + a;
    e = sum_weight;
    x_0 = sum_weight_anchor / sum_weight;
  }

  void SetSolution(double x_new) { x = x_new; }
  void UpdateBlkLocation() { blk_ptr->SetLLX(x); }
};

void MinimizeQuadraticDisplacement(
    std::vector<BlkDispVar> &vars,
    int lower_limit,
    int upper_limit
);

}

#endif //DALI_DALI_PLACER_WELL_LEGALIZER_HELPER_H_
