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
#include "blkpairnets.h"

namespace dali {

void BlkPairNets::ClearX() {
  e00x = 0;
  e01x = 0;
  e10x = 0;
  e11x = 0;
  b0x = 0;
  b1x = 0;
}
void BlkPairNets::WriteX() {
  if (blk_num0 != blk_num1) {
    it01x.valueRef() = e01x;
    it10x.valueRef() = e10x;
  }
}

void BlkPairNets::ClearY() {
  e00y = 0;
  e01y = 0;
  e10y = 0;
  e11y = 0;
  b0y = 0;
  b1y = 0;
}

void BlkPairNets::WriteY() {
  if (blk_num0 != blk_num1) {
    it01y.valueRef() = e01y;
    it10y.valueRef() = e10y;
  }
}

}