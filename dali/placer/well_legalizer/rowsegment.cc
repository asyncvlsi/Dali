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

namespace dali {

void RowSegment::SetLoc(int lx, int ly) {
  lx_ = lx;
  ly_ = ly;
  DaliExpects(false, "do not use this API");
}

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
  blk_initial_location_.emplace_back(blk_ptr->LLX(), y_init);
}

void RowSegment::MinDisplacementLegalization() {
  std::sort(
      blk_list_.begin(),
      blk_list_.end(),
      [](const Block *blk_ptr0, const Block *blk_ptr1) {
        return blk_ptr0->X() < blk_ptr1->X();
      }
  );

  std::vector<BlockSegment> segments;

  DaliExpects(blk_list_.size() == blk_initial_location_.size(),
              "Block number does not equal initial location number\n");

  size_t sz = blk_list_.size();
  int lower_bound = lx_;
  int upper_bound = lx_ + width_;
  for (size_t i = 0; i < sz; ++i) {
    // create a segment which contains only this block
    Block *blk_ptr = blk_list_[i];
    double init_x = blk_initial_location_[i].x;
    if (init_x < lower_bound) {
      init_x = lower_bound;
    }
    if (init_x + blk_ptr->Width() > upper_bound) {
      init_x = upper_bound - blk_ptr->Width();
    }
    segments.emplace_back(blk_ptr, init_x);

    // if this new segment is the only segment, do nothing
    size_t seg_sz = segments.size();
    if (seg_sz == 1) continue;

    // check if this segment overlap with the previous one, if yes, merge these two segments
    // repeats until this is no overlap or only one segment left

    BlockSegment *cur_seg = &(segments[seg_sz - 1]);
    BlockSegment *prev_seg = &(segments[seg_sz - 2]);
    while (prev_seg->IsNotOnLeft(*cur_seg)) {
      prev_seg->Merge(*cur_seg, lower_bound, upper_bound);
      segments.pop_back();

      seg_sz = segments.size();
      if (seg_sz == 1) break;
      cur_seg = &(segments[seg_sz - 1]);
      prev_seg = &(segments[seg_sz - 2]);
    }
  }

  //int count = 0;
  for (auto &seg: segments) {
    seg.UpdateBlockLocation();
    //count += seg.blk_list.size();
    //seg.Report();
  }
}

}
