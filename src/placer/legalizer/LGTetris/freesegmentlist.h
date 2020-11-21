//
// Created by Yihang Yang on 2019-07-17.
//

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
  int min_width_;
  size_t size_;
  /***derived data entries***/
 public:
  FreeSegmentList();
  FreeSegmentList(int start, int stop, int min_width);
  ~FreeSegmentList();
  size_t size() const {
    return size_;
  }
  int Left() const {
    if (head_ == nullptr) {
      BOOST_LOG_TRIVIAL(info) << "Empty linked list, Left() not available\n";
      assert(head_ != nullptr);
    }
    return head_->Start();
  }
  int Right() const {
    if (tail_ == nullptr) {
      BOOST_LOG_TRIVIAL(info) << "Empty linked list, Right() not available\n";
      assert(tail_ != nullptr);
    }
    return tail_->End();
  }
  FreeSegment *Head() const {return head_;}
  FreeSegment *Tail() const {return tail_;}
  int MinWidth() const {return min_width_;}
  void SetMinWidth(int initMinWidth) {min_width_ = initMinWidth;}
  void Append(FreeSegment *segList);
  bool EmplaceBack(int start, int end);
  void PushBack(FreeSegment *seg);
  void Insert(FreeSegment *insertPosition, FreeSegment *segToInsert);
  bool Empty() const;
  void CopyFrom(FreeSegmentList &originList);
  void Clear();
  bool ApplyMask(FreeSegmentList &maskRow);
  void RemoveSeg(FreeSegment *seg_in_list);
  void RemoveShortSeg(int width);
  void UseSpace(int start, int length);
  bool IsSpaceAvail(int x_loc, int width);
  int MinDispLoc(int llx, int width);
  void Show();
};

}

#endif //DALI_FREESEGMENTLIST_H
