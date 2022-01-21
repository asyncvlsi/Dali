/*******************************************************************************
 *
 * Copyright (c) 2022 Yihang Yang
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
#ifndef DALI_DALI_PLACER_WELL_LEGALIZER_LGBLKAUX_H_
#define DALI_DALI_PLACER_WELL_LEGALIZER_LGBLKAUX_H_

#include "dali/circuit/block.h"
#include "dali/common/misc.h"

namespace dali {

class LgBlkAux : public BlockAux {
 public:
  explicit LgBlkAux(Block *blk_ptr);
  void StoreCurLocAsInitLoc();
  void StoreCurLocAsGreedyLoc();
  void StoreCurLocAsQPLoc();
  void StoreCurLocAsConsLoc();

  void RecoverInitLoc();
  void RecoverGreedyLoc();
  void RecoverQPLoc();
  void RecoverConsLoc();

  void RecoverInitLocX();
  void RecoverGreedyLocX();
  void RecoverQPLocX();
  void RecoverConsLocX();

  void SetSubCellLoc(int id, double loc, double weight);
  void ComputeAverageLoc();
  std::vector<double> &SubLocs();
  double AverageLoc();

  double2d InitLoc();
  double2d GreedyLoc();
  double2d QPLoc();
  double2d ConsLoc();
 private:
  double2d init_loc_;     // location before legalization
  double2d greedy_loc_;   // location from the greedy legalization algorithm
  double2d qp_loc_;       // location from quadratic programming
  double2d cons_loc_;     // location from the consensus algorithm

  std::vector<double> sub_locs_; // locations from different sub-cells
  std::vector<double> weights_;  // weights of clusters they belong to
  double average_loc_ = DBL_MAX;

  std::vector<int> stretch_length_;
  double tot_stretch_length = 0;
};

}

#endif //DALI_DALI_PLACER_WELL_LEGALIZER_LGBLKAUX_H_
