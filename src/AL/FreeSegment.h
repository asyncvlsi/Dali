//
// Created by yihan on 7/13/2019.
//

#ifndef HPCC_FREESEGMENT_H
#define HPCC_FREESEGMENT_H

#include <iostream>
#include <cassert>

class FreeSegment {
private:
  int _start;
  int _end;
  FreeSegment *_prev = nullptr; // Pointer to previous free segment
  FreeSegment *_next = nullptr; // Pointer to next free segment
  int _minWidth;
public:
  explicit FreeSegment(int initStart = 0, int initEnd = 0, int minWidth = 0);
  bool setPrev(FreeSegment *preFreeSeg_ptr);
  bool setNext(FreeSegment *nextFreeSeg_ptr);
  FreeSegment *next() const;
  FreeSegment *prev() const;
  bool useSpace(int startLoc, int endLoc);
  int start() const;
  int end() const;
  int length() const;
};


#endif //HPCC_FREESEGMENT_H
