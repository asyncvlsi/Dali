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
#include "freesegmentlist.h"

#include "dali/common/misc.h"

namespace dali {

FreeSegmentList::FreeSegmentList() {
  head_ = nullptr;
  tail_ = nullptr;
  min_width_ = 0;
  size_ = 0;
}

FreeSegmentList::FreeSegmentList(int start, int stop, int min_width)
    : min_width_(min_width) {
  head_ = new FreeSegment(start, stop);
  tail_ = head_;
  size_ = 1;
}

FreeSegmentList::~FreeSegmentList() {
  Clear();
}

void FreeSegmentList::Append(FreeSegment *segList) {
  /****push a list of freesegment into the linked list with some sanity check****/
  if (segList == nullptr) {
    BOOST_LOG_TRIVIAL(info) << "push nullptr to linked list?\n";
    assert(segList != nullptr);
  } else {
    if (head_ == nullptr) {
      head_ = segList;
      tail_ = segList;
    } else {
      segList->SetPrev(tail_);
      tail_->SetNext(segList);
      tail_ = segList;
    }
    ++size_;
    while (tail_->Next() != nullptr) {
      tail_ = tail_->Next();
      ++size_;
    }
  }
}

bool FreeSegmentList::EmplaceBack(int start, int end) {
  auto *seg_ptr = new FreeSegment(start, end);
  if (head_ == nullptr) {
    head_ = seg_ptr;
    tail_ = seg_ptr;
  } else {
    if (start < Right()) {
      BOOST_LOG_TRIVIAL(info) << "Illegal segment emplace back, Start: "
                              << start
                              << " is required to be no less than the Right End of current linked list: "
                              << Right()
                              << "\n";
      return false;
    }
    tail_->LinkSingleSeg(seg_ptr);
    tail_ = seg_ptr;
  }
  ++size_;
  return true;
}

void FreeSegmentList::PushBack(FreeSegment *seg) {
  /****push a single free segment into the linked list****/
  if (seg == nullptr) {
    BOOST_LOG_TRIVIAL(info) << "push nullptr to linked list?\n";
    assert(seg != nullptr);
  }
  if (seg->Next() != nullptr) {
    BOOST_LOG_TRIVIAL(info)
      << "argument has more than one nodes inside, please use method Append() instead of PushBack\n";
    assert(seg->Next() == nullptr);
  }
  if (Empty()) {
    head_ = seg;
    tail_ = seg;
    ++size_;
  } else {
    if (Right() > seg->Start()) {
      BOOST_LOG_TRIVIAL(info)
        << "Cannot push segment into list, because the Right of list is: "
        << Right()
        << " larger than the Start of segment to push: " << seg->Start()
        << std::endl;
      assert(Right() <= seg->Start());
    }
    if (Right() < seg->Start()) {
      seg->SetPrev(tail_);
      tail_->SetNext(seg);
      tail_ = seg;
      ++size_;
    } else {
      tail_->SetSpan(tail_->Start(), seg->End());
    }
  }
}

void FreeSegmentList::Insert(FreeSegment *insertPosition,
                             FreeSegment *segToInsert) {
  if (insertPosition == Tail()) {
    PushBack(segToInsert);
  } else {
    FreeSegment *next = insertPosition->Next();
    insertPosition->SetNext(segToInsert);
    segToInsert->SetNext(next);
    segToInsert->SetPrev(insertPosition);
    next->SetPrev(segToInsert);
    ++size_;
  }
}

bool FreeSegmentList::Empty() const {
  return (size() == 0);
}

void FreeSegmentList::CopyFrom(FreeSegmentList &originList) {
  this->Clear();
  if (!originList.Empty()) {
    for (FreeSegment *current = originList.Head(); current != nullptr;
         current = current->Next()) {
      EmplaceBack(current->Start(), current->End());
    }
    min_width_ = originList.MinWidth();
  }
}

void FreeSegmentList::Clear() {
  // Clear the linked list
  FreeSegment *current = head_;
  FreeSegment *next = nullptr;
  while (current != nullptr) {
    next = current->Next();
    delete current;
    current = next;
  }
  head_ = nullptr;
  tail_ = nullptr;
  size_ = 0;
  min_width_ = 0;
}

void FreeSegmentList::RemoveSeg(FreeSegment *seg_in_list) { // don't remove, traverse the linked list is good enough
  /****
  * remove a segment in the linked list
  * 1. if the linked list is empty or the provided segment pointer is nullptr, assert fault;
  * 2. if the linked list has length 1, clear this linked list;
  * 3. if the linked list has length larger than 1:
  *    a). if the segment to remove is the head, set the head to the next one;
  *    b). if the segment to remove is the tail, set the tail to the prev one;
  *    c). if the segment to remove has real prev and next, i.e., not nullptr, remove this segment,
  *        and connect its prev with its next
  ****/
  DaliExpects(seg_in_list != nullptr,
              "Remove Empty pointer? Be careful, you can only remove nodes in the linked list");
  DaliExpects(!Empty(), "Nothing to remove from an Empty list!");
  FreeSegment *current = seg_in_list;
  if (size() == 1) {
    head_ = nullptr;
    tail_ = nullptr;
  } else {
    if (current == Head()) {
      head_ = current->Next();
      head_->SetPrev(nullptr);
    } else if (current == Tail()) {
      tail_ = current->Prev();
      tail_->SetNext(nullptr);
    } else {
      FreeSegment *prev = current->Prev();
      FreeSegment *next = current->Next();
      prev->SetNext(next);
      next->SetPrev(prev);
    }
  }
  --size_;
  delete current;
}

bool FreeSegmentList::ApplyMask(FreeSegmentList &maskRow) {
  if (head_ == nullptr) {
    return false;
  }
  FreeSegmentList result;
  for (FreeSegment *currentMaskSeg = maskRow.Head(); currentMaskSeg != nullptr;
       currentMaskSeg = currentMaskSeg->Next()) {
    for (FreeSegment *current = Head(); current != nullptr;
         current = current->Next()) {
      if (current->IsDominate(currentMaskSeg)) break;
      FreeSegment *tmpSeg_ptr = currentMaskSeg->SingleSegAnd(current);
      if (tmpSeg_ptr != nullptr) {
        result.PushBack(tmpSeg_ptr);
      }
    }
  }
  CopyFrom(result);
  return true;
}

void FreeSegmentList::RemoveShortSeg(int width) {
  /****to understand this member function, one needs to understand member function removeSeg()
   * the member function works in the following way:
   * traverse the linked list
   * 1. if the current segment has length less than the required length, remove it;
   * 2. if the current segment has length no less than the required length, skip;
   * 3. set current pointer to its next.****/
  if (Empty()) return;
  for (FreeSegment *current = Head(); current != nullptr;) {
    FreeSegment *next = current->Next();
    if (current->Length() < width) {
      RemoveSeg(current);
    }
    current = next;
  }
}

void FreeSegmentList::UseSpace(int start, int length) {
  /****
   * It is for sure that [start, start+length] sits in one of the segments
   * To use a segment of this segment list:
   *    1. use all free space, then we need to remove this segment from the linked list;
   *    2. use left part (shrink this segment from left side);
   *    3. use right part (shrink this segment from right side);
   *    4. use middle part (split this segment into two segments)
   * ****/
  int end_loc = start + length;
  bool isSegFound = false;
  FreeSegment target(start, end_loc);
  for (auto *current = Head(); current != nullptr; current = current->Next()) {
    if (current->IsContain(&target)) {
      isSegFound = true;
      if (current->IsSameStartEnd(&target)) {
        RemoveSeg(current);
      } else if ((start == current->Start()) && (end_loc < current->End())) {
        current->SetSpan(end_loc, current->End());
      } else if ((start > current->Start()) && (end_loc == current->End())) {
        current->SetSpan(current->Start(), start);
      } else {
        auto *seg_ptr = new FreeSegment(end_loc, current->End());
        current->SetSpan(current->Start(), start);
        Insert(current, seg_ptr);
      }
    }
    if (isSegFound) break;
  }

  if (!isSegFound) {
    BOOST_LOG_TRIVIAL(info)
      << "What? there are bugs in the code, the program should not reach here"
      << std::endl;
    assert(isSegFound);
  }
}

bool FreeSegmentList::IsSpaceAvail(int x_loc, int width) {
  /****
   * In order to know if interval [x_loc, x_loc + width] is available, we just need to check if this segment sits in a FreeSegment in the linked-list
   * A speed-up algorithm is known, will implement it
   * ****/
  FreeSegment target(x_loc, x_loc + width);
  bool is_avail = false;
  //BOOST_LOG_TRIVIAL(info)   << Head() << "\n";
  for (auto *current = Head(); current != nullptr; current = current->Next()) {
    if (current->IsContain(&target)) {
      is_avail = true;
      break;
    }
    if (current->IsDominate(&target)) {
      break;
    }
  }
  return is_avail;
}

/****
 * We assume any segment in this list has a length longer than block width
 * ****/
int FreeSegmentList::MinDispLoc(int width) {
  DaliExpects(
      Head()->Length() >= width,
      "Segment length should be longer than block width"
  );
  return Head()->Start();
}

void FreeSegmentList::Show() {
  if (head_ == nullptr) {
    BOOST_LOG_TRIVIAL(info) << "Empty list, nothing to display" << std::endl;
  } else {
    BOOST_LOG_TRIVIAL(info) << "MinWidth: " << min_width_ << "  ";
    FreeSegment *current = head_;
    size_t count = 0;
    while (count < size_) {
      BOOST_LOG_TRIVIAL(info) << "( " << current->Start() << " "
                              << current->End() << " )";
      current = current->Next();
      if (current != nullptr) {
        BOOST_LOG_TRIVIAL(info) << " -> ";
      } else {
        break;
      }
      ++count;
    }
    BOOST_LOG_TRIVIAL(info) << std::endl;
  }
}

}
