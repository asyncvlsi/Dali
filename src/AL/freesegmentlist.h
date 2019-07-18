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
  size_t size() const;
  int left() const;
  int right() const;
  FreeSegment* head() const;
  FreeSegment* tail() const;
  int minWidth() const;
  void append(FreeSegment* segList);
  bool emplace_back(int start, int end);
  void push_back(FreeSegment* seg);
  void insert(FreeSegment* insertPosition, FreeSegment* segToInsert);
  bool empty() const;
  void copyFrom(FreeSegmentList &originList);
  void clear();
  bool applyMask(FreeSegmentList &maskRow);
  void removeSeg(FreeSegment* &segInList);
  void removeShortSeg(int minWidth);
  void useSpace(int locToStart, int lengthToUse);
  void show();
};


#endif //HPCC_FREESEGMENTLIST_H
