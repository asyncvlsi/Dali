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
#ifndef DALI_PLACER_WELL_LEGALIZER_LGBLKAUX_H_
#define DALI_PLACER_WELL_LEGALIZER_LGBLKAUX_H_

#include <cfloat>

#include "dali/circuit/block.h"
#include "dali/common/misc.h"

namespace dali {

/** Auxiliary legalization locations cached on each block. */
class LgBlkAux : public BlockAux {
 public:
  explicit LgBlkAux(Block* blk_ptr);

  /** Cache the block's current location as its initial location. */
  void StoreCurLocAsInitLoc();

  /** Cache the block's current location as its greedy legalization location. */
  void StoreCurLocAsGreedyLoc();

  /** Cache the block's current location as its QP legalization location. */
  void StoreCurLocAsQPLoc();

  /** Cache the block's current location as its consensus location. */
  void StoreCurLocAsConsLoc();

  /** Restore the block to its initial location. */
  void RecoverInitLoc();

  /** Restore the block to its greedy legalization location. */
  void RecoverGreedyLoc();

  /** Restore the block to its QP legalization location. */
  void RecoverQPLoc();

  /** Restore the block to its consensus location. */
  void RecoverConsLoc();

  void RecoverInitLocX();
  void RecoverGreedyLocX();
  void RecoverQPLocX();
  void RecoverConsLocX();

  /** Store one sub-cell legalization location and weight. */
  void SetSubCellLoc(int id, double loc, double weight);

  /** Compute weighted average location from sub-cell locations. */
  void ComputeAverageLoc();

  /** Return sub-cell locations. */
  std::vector<double>& SubLocs();

  /** Return cached weighted average location. */
  double AverageLoc();

  double2d InitLoc();
  double2d GreedyLoc();
  double2d QPLoc();
  double2d ConsLoc();

 private:
  double2d init_loc_;    // location before legalization
  double2d greedy_loc_;  // location from the greedy legalization algorithm
  double2d qp_loc_;      // location from quadratic programming
  double2d cons_loc_;    // location from the consensus algorithm

  std::vector<double> sub_locs_;  // locations from different sub-cells
  std::vector<double> weights_;   // weights of clusters they belong to
  double average_loc_ = DBL_MAX;

  std::vector<int> stretch_length_;
  double tot_stretch_length = 0;
};

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_LGBLKAUX_H_
