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
  explicit LgBlkAux(Block *blk_ptr) : BlockAux(blk_ptr) {}
  void StoreCurLocAsInitLoc();
  void StoreCurLocAsGreedyLoc();
  void StoreCurLocAsQPLoc();
  void StoreCurLocAsConsLoc();
  void StoreCurLoc(size_t index);
  void AllocateCacheLocs(size_t sz);

  void RecoverInitLoc();
  void RecoverGreedyLoc();
  void RecoverQPLoc();
  void RecoverConsLoc();
  void RecoverLoc(size_t index);

  double2d InitLoc();
  double2d GreedyLoc();
  double2d QPLoc();
  double2d ConsLoc();
  std::vector<double2d> &CachedLocs();

 private:
  double2d init_loc_;     // location before legalization
  double2d greedy_loc_;   // location from the greedy legalization algorithm
  double2d qp_loc_;       // location from quadratic programming
  double2d cons_loc_;     // location from the consensus algorithm
  std::vector<double2d> cached_locs_; // other cached locations for future usage

  std::vector<int> stretch_length_;
  double tot_stretch_length = 0;
};

}

#endif //DALI_DALI_PLACER_WELL_LEGALIZER_LGBLKAUX_H_
