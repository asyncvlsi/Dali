//
// Created by yihan on 7/13/2019.
//

#ifndef HPCC_FREESEGMENT_H
#define HPCC_FREESEGMENT_H

#include <iostream>
#include <cassert>
#include <algorithm>

class FreeSegment {
private:
  int _start;
  int _end;
  FreeSegment* _prev = nullptr; // Pointer to previous free segment
  FreeSegment* _next = nullptr; // Pointer to next free segment
public:
  explicit FreeSegment(int initStart = 0, int initEnd = 0);
  bool setPrev(FreeSegment* preFreeSeg_ptr);
  bool setNext(FreeSegment* nextFreeSeg_ptr);
  bool concatSingleSeg(FreeSegment* seg_ptr);
  FreeSegment* next() const;
  FreeSegment* prev() const;
  bool useSpace(int startLoc, int endLoc);
  void setStart(int startLoc);
  int start() const;
  void setEnd(int endLoc);
  int end() const;
  int length() const;
  bool isOverlap(FreeSegment* seg) const;
  FreeSegment* singleSegAnd(FreeSegment* seg);
  FreeSegment* singleSegOr(FreeSegment* seg);
  void clear();

  friend std::ostream& operator<<(std::ostream& os, const FreeSegment* seg) {
    if (seg == nullptr) {
      os << "Empty pointer?\n";
    } else {
      os << "( " << seg->_start << " " << seg->_end << " )\n";
      if (seg->next() != nullptr) {
        os << seg->next();
      }
    }
    return os;
  }
};


#endif //HPCC_FREESEGMENT_H
