//
// Created by Yihang Yang on 2019-07-17.
//

#ifndef HPCC_FREESEGMENTLIST_H
#define HPCC_FREESEGMENTLIST_H

#include "freesegment.h"

#include <cassert>

#include <iostream>

class FreeSegmentList {
private:
  FreeSegment* head_;
  FreeSegment* tail_;
  int min_width_;
  size_t size_;
  /***derived data entries***/
public:
  FreeSegmentList();
  explicit FreeSegmentList(int start, int stop, int min_width);
  ~FreeSegmentList();
  size_t size() const;
  int Left() const;
  int Right() const;
  FreeSegment* Head() const;
  FreeSegment* Tail() const;
  int MinWidth() const;
  void SetMinWidth(int initMinWidth);
  void Append(FreeSegment* segList);
  bool EmplaceBack(int start, int end);
  void PushBack(FreeSegment* seg);
  void Insert(FreeSegment* insertPosition, FreeSegment* segToInsert);
  bool Empty() const;
  void CopyFrom(FreeSegmentList &originList);
  void Clear();
  bool ApplyMask(FreeSegmentList &maskRow);
  void RemoveSeg(FreeSegment* seg_in_list);
  void RemoveShortSeg(int width);
  void UseSpace(int start, int length);
  bool IsSpaceAvail(int x_loc, int width);
  int MinDispLoc(int llx, int width);
  void Show();
};


#endif //HPCC_FREESEGMENTLIST_H