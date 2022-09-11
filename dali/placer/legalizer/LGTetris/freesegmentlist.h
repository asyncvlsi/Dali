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
#ifndef DALI_FREESEGMENTLIST_H
#define DALI_FREESEGMENTLIST_H

#include "freesegment.h"

#include <cassert>

#include <iostream>

namespace dali {

class FreeSegmentList {
 private:
  FreeSegment *head_;
  FreeSegment *tail_;
  int32_t min_width_;
  size_t size_;
  /***derived data entries***/
 public:
  FreeSegmentList();
  FreeSegmentList(int32_t start, int32_t stop, int32_t min_width);
  ~FreeSegmentList();
  size_t size() const {
    return size_;
  }
  int32_t Left() const {
    if (head_ == nullptr) {
      BOOST_LOG_TRIVIAL(info) << "Empty linked list, Left() not available\n";
      assert(head_ != nullptr);
    }
    return head_->Start();
  }
  int32_t Right() const {
    if (tail_ == nullptr) {
      BOOST_LOG_TRIVIAL(info) << "Empty linked list, Right() not available\n";
      assert(tail_ != nullptr);
    }
    return tail_->End();
  }
  FreeSegment *Head() const { return head_; }
  FreeSegment *Tail() const { return tail_; }
  int32_t MinWidth() const { return min_width_; }
  void SetMinWidth(int32_t initMinWidth) { min_width_ = initMinWidth; }
  void Append(FreeSegment *segList);
  bool EmplaceBack(int32_t start, int32_t end);
  void PushBack(FreeSegment *seg);
  void Insert(FreeSegment *insertPosition, FreeSegment *segToInsert);
  bool Empty() const;
  void CopyFrom(FreeSegmentList &originList);
  void Clear();
  bool ApplyMask(FreeSegmentList &maskRow);
  void RemoveSeg(FreeSegment *seg_in_list);
  void RemoveShortSeg(int32_t width);
  void UseSpace(int32_t start, int32_t length);
  bool IsSpaceAvail(int32_t x_loc, int32_t width);
  int32_t MinDispLoc(int32_t width);
  void Show();
};

}

#endif //DALI_FREESEGMENTLIST_H
