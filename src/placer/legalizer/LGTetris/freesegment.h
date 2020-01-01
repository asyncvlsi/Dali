//
// Created by yihan on 7/13/2019.
//

#ifndef HPCC_FREESEGMENT_H
#define HPCC_FREESEGMENT_H

#include <cassert>

#include <algorithm>
#include <iostream>

class FreeSegment {
 private:
  int start_;
  int end_;
  FreeSegment *prev_ = nullptr; // Pointer to previous free segment
  FreeSegment *next_ = nullptr; // Pointer to Next free segment
 public:
  explicit FreeSegment(int start = 0, int stop = 0);
  bool SetPrev(FreeSegment *preFreeSeg_ptr);
  bool SetNext(FreeSegment *nextFreeSeg_ptr);
  bool LinkSingleSeg(FreeSegment *seg_ptr);
  FreeSegment *Next();
  FreeSegment *Prev();
  void SetSpan(int startLoc, int endLoc);
  int Start() const;
  int End() const;
  int Length() const;
  bool IsOverlap(FreeSegment *seg) const;
  bool IsTouch(FreeSegment *seg) const;
  bool IsDominate(FreeSegment *seg) const;
  bool IsContain(FreeSegment *seg) const;
  bool IsSameStartEnd(FreeSegment *seg) const;
  FreeSegment *SingleSegAnd(FreeSegment *seg);
  FreeSegment *SingleSegOr(FreeSegment *seg);
  void Clear();

  friend std::ostream &operator<<(std::ostream &os, FreeSegment *seg) {
    if (seg == nullptr) {
      os << "Empty pointer?\n";
    } else {
      os << "( " << seg->start_ << " " << seg->end_ << " )\n";
      if (seg->Next() != nullptr) {
        os << seg->Next();
      }
    }
    return os;
  }
};

#endif //HPCC_FREESEGMENT_H
