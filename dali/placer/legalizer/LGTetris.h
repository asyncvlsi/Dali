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
#ifndef DALI_PLACER_LEGALIZER_LGTETRIS_H_
#define DALI_PLACER_LEGALIZER_LGTETRIS_H_

#include <vector>

#include "dali/circuit/block.h"
#include "dali/common/misc.h"
#include "dali/placer/legalizer/LGTetris/tetrisspace.h"
#include "dali/placer/placer.h"

/****
 * The default setting of this legalizer expects a reasonably good placement as an input;
 * "Reasonably good" means:
 *  1. All blocks are inside placement region
 *  2. There are few or no location with extremely high block density
 *  3. Maybe something else should be added here
 * There is a setting of this legalizer, which can handle bad placement input really well,
 * but who wants bad input? The Global placer and detailed placer should have done their job well;
 * ****/

namespace dali {

class TetrisLegalizer : public Placer {
 private:
  int32_t max_iteration_;
  int32_t current_iteration_;
  bool flipped_;
  std::vector<BlkInitPair> index_loc_list_;
 public:
  TetrisLegalizer();
  void InitLegalizer();
  void SetMaxItr(int32_t max_iteration);
  void ResetItr() { current_iteration_ = 0; }
  void FastShift(int32_t failure_point);
  void FlipPlacement();
  bool TetrisLegal();
  bool StartPlacement() override;
};

}

#endif //DALI_PLACER_LEGALIZER_LGTETRIS_H_
