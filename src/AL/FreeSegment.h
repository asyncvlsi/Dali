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
  FreeSegment *_next = nullptr; // Pointer to next space range
  FreeSegment *_prev = nullptr; // Pointer to previous space range
public:
  explicit FreeSegment(int initStart = 0, int initEnd = 0);
  bool setPrev(FreeSegment *preFreeSegptr);
  bool setNext(FreeSegment *nextFreeSegptr);
  bool useSpace(int startLoc, int endLoc);
  int start() const;
  int end() const;
};


#endif //HPCC_FREESEGMENT_H
