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
#ifndef DALI_PLACER_LEGALIZER_TETRIS_LEGALIZER_H_
#define DALI_PLACER_LEGALIZER_TETRIS_LEGALIZER_H_

#include <vector>

#include "dali/circuit/block.h"
#include "dali/common/misc.h"
#include "dali/placer/legalizer/tetris_legalizer/tetris_space.h"
#include "dali/placer/placer.h"

namespace dali {

/**
 * Tetris-style detailed legalizer for reasonably placed standard cells.
 *
 * The input is expected to be inside the placement boundary with limited local
 * density spikes, typically after global placement.
 */
class TetrisLegalizer : public Placer {
 private:
  int max_iteration_;
  int current_iteration_;
  bool flipped_;
  std::vector<BlockInitialLocation> index_loc_list_;

 public:
  TetrisLegalizer();

  /** Initialize internal legalization state. */
  void InitLegalizer();

  /** Set maximum legalization iterations. */
  void SetMaxItr(int max_iteration);

  /** Reset current legalization iteration. */
  void ResetItr() { current_iteration_ = 0; }

  /** Shift cells quickly after a failed legalization point. */
  void FastShift(int failure_point);

  /** Flip placement direction for another legalization attempt. */
  void FlipPlacement();

  /** Run the Tetris legalization pass. */
  bool TetrisLegal();

  /** Run the placer interface entry point. */
  bool StartPlacement() override;
};

}  // namespace dali

#endif  // DALI_PLACER_LEGALIZER_TETRIS_LEGALIZER_H_
