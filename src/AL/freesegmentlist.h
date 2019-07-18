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
public:
  explicit FreeSegmentList(int minWidth = 0);
  ~FreeSegmentList();
  size_t size();
  void push_back(FreeSegment* seg);
  void clear();
  FreeSegment* ANDSingleSeg(FreeSegment* seg);
  void show();
};


#endif //HPCC_FREESEGMENTLIST_H
