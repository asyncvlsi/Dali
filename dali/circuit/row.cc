/*******************************************************************************
 *
 * Copyright (c) 2023 Yihang Yang
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
#include "row.h"

#include <algorithm>

namespace dali {

void GeneralRowSegment::SetLX(int lx) { lx_ = lx; }

void GeneralRowSegment::SetWidth(int width) { width_ = width; }

void GeneralRowSegment::AddBlock(Block *blk_ptr) {
  blocks_.emplace_back(blk_ptr);
}

int GeneralRowSegment::LX() const { return lx_; }

int GeneralRowSegment::UX() const { return lx_ + width_; }

int GeneralRowSegment::Width() const { return width_; }

std::vector<Block *> &GeneralRowSegment::Blocks() { return blocks_; }

void GeneralRowSegment::SortBlocks() {
  std::sort(blocks_.begin(), blocks_.end(),
            [](const Block *blk_ptr0, const Block *blk_ptr1) {
              return blk_ptr0->LLX() < blk_ptr1->LLX();
            });
}

void GeneralRow::SetLY(int ly) { ly_ = ly; }

void GeneralRow::SetHeight(int height) { height_ = height; }

void GeneralRow::SetOrient(bool is_orient_N) { is_orient_N_ = is_orient_N; }

void GeneralRow::SetPwellHeight(int p_well_height) {
  p_well_height_ = p_well_height;
}

void GeneralRow::SetNwellHeight(int n_well_height) {
  n_well_height_ = n_well_height;
}

int GeneralRow::LY() const { return ly_; }

int GeneralRow::Height() const { return height_; }

bool GeneralRow::IsOrientN() const { return is_orient_N_; }

int GeneralRow::PwellHeight() const { return p_well_height_; }

int GeneralRow::NwellHeight() const { return n_well_height_; }

std::vector<GeneralRowSegment> &GeneralRow::RowSegments() {
  return row_segments_;
}

}  // namespace dali
