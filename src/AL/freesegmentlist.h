//
// Created by Yihang Yang on 2019-07-17.
//

#ifndef HPCC_FREESEGMENTLIST_H
#define HPCC_FREESEGMENTLIST_H

#include <iostream>
#include <cassert>
#include "freesegment.h"

class FreeSegmentList {
private:
  FreeSegment* _head;
  FreeSegment* _tail;
  int _minWidth;
  size_t _size;
  /***derived data entries***/
public:
  FreeSegmentList();
  explicit FreeSegmentList(int initStart, int initEnd, int minWidth = 0);
  ~FreeSegmentList();
  size_t size();
  int left();
  int right();
  FreeSegment* head();
  FreeSegment* tail();
  int minWidth();
  void append(FreeSegment *segList);
  bool emplace_back(int start, int end);
  void push_back(FreeSegment* seg);
  void copyFrom(FreeSegmentList &originList);
  void clear();
  FreeSegment* ANDSingleSeg(FreeSegment* seg);
  bool applyMask(FreeSegmentList &maskRow);
  void show();
};


#endif //HPCC_FREESEGMENTLIST_H
