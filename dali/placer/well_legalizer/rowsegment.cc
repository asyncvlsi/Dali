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
#include "rowsegment.h"

#include "dali/placer/well_legalizer/blocksegment.h"
#include "dali/placer/well_legalizer/helper.h"

namespace dali {

void RowSegment::SetLLX(int lx) {
  lx_ = lx;
}

void RowSegment::SetURX(int ux) {
  lx_ = ux - width_;
}

void RowSegment::SetWidth(int width) {
  width_ = width;
}

void RowSegment::SetUsedSize(int used_size) {
  used_size_ = used_size;
}

int RowSegment::LLX() const {
  return lx_;
}

int RowSegment::URX() const {
  return lx_ + width_;
}

int RowSegment::Width() const {
  return width_;
}

int RowSegment::UsedSize() const {
  return used_size_;
}

std::vector<Block *> &RowSegment::Blocks() {
  return blk_list_;
}

/****
 * Add a given block to this segment.
 * Update used space and the block list
 * @param blk_ptr: a pointer to the block which needs to be added to this segment
 */
void RowSegment::AddBlock(Block *blk_ptr) {
  used_size_ += blk_ptr->Width();
  blk_list_.push_back(blk_ptr);
  double y_init = blk_ptr->LLY();
  BlockTypeWell *well_ptr = blk_ptr->TypePtr()->WellPtr();
  y_init = well_ptr->Pheight();
  blk_initial_location_[blk_ptr] = double2d(blk_ptr->LLX(), y_init);
}

void RowSegment::MinDisplacementLegalization() {
  std::sort(
      blk_list_.begin(),
      blk_list_.end(),
      [](const Block *blk0, const Block *blk1) {
        return (blk0->LLX() < blk1->LLX()) ||
            ((blk0->LLX() == blk1->LLX()) && (blk0->Id() < blk1->Id()));
      }
  );

  std::vector<BlkDispVar> vars;
  vars.reserve(blk_list_.size());
  for (Block *&blk_ptr: blk_list_) {
    vars.emplace_back(blk_ptr->Width(), blk_initial_location_[blk_ptr].x, 1.0);
    vars.back().blk_ptr = blk_ptr;
  }

  MinimizeQuadraticDisplacement(vars, LLX(), URX());
}

}
