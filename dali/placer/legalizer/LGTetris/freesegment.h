//
// Created by yihan on 7/13/19.
//

#ifndef DALI_FREESEGMENT_H
#define DALI_FREESEGMENT_H

#include <cassert>

#include <algorithm>
#include <iostream>

#include "dali/common/logging.h"

namespace dali {

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
};

inline bool FreeSegment::SetPrev(FreeSegment *preFreeSeg_ptr) {
  prev_ = preFreeSeg_ptr;
  return true;
}

inline bool FreeSegment::SetNext(FreeSegment *nextFreeSeg_ptr) {
  next_ = nextFreeSeg_ptr;
  return true;
}

inline FreeSegment *FreeSegment::Next() {
  return next_;
}

inline FreeSegment *FreeSegment::Prev() {
  return prev_;
}

inline void FreeSegment::SetSpan(int startLoc, int endLoc) {
  if (startLoc > endLoc) {
    BOOST_LOG_TRIVIAL(info) << "Cannot set the span of a segment with start larger than End, Start" << startLoc
                            << " End: " << endLoc << std::endl;
    assert(startLoc <= endLoc);
  }
  start_ = startLoc;
  end_ = endLoc;
}

inline int FreeSegment::Start() const {
  return start_;
}

inline int FreeSegment::End() const {
  return end_;
}

inline int FreeSegment::Length() const {
  return end_ - start_;
}

inline bool FreeSegment::IsOverlap(FreeSegment *seg) const {
  if ((Length() == 0) || (seg->Length() == 0)) {
    BOOST_LOG_TRIVIAL(info) << "Length 0 segment?!\n";
    return false;
  }
  bool notOverlap = (end_ <= seg->Start()) || (start_ >= seg->End());
  return !notOverlap;
}

inline bool FreeSegment::IsTouch(FreeSegment *seg) const {
  if ((Length() == 0) || (seg->Length() == 0)) {
    BOOST_LOG_TRIVIAL(info) << "Length 0 segment?!\n";
    return false;
  }
  return (end_ == seg->Start()) || (start_ == seg->End());
}

inline bool FreeSegment::IsDominate(FreeSegment *seg) const {
  /****
   * If this FreeSegment is on the right hand side of seg, and has common overlap length 0, return true
   *
   * example: |---seg---| ... |---this seg---|, return true
   * example: |---seg---|---this seg---|, return true
   * else return false
   * ****/
  return (start_ >= seg->End());
}

inline bool FreeSegment::IsContain(FreeSegment *seg) const {
  /****
   * If this FreeSegment contains seg, return true
   * true condition:
   *    start_ <= seg->Start() && end_ >= seg->End()
   * example: |---this seg---|
   *          |---seg---|
   *          return true
   * ****/
  return (start_ <= seg->Start()) && (end_ >= seg->End());
}

inline bool FreeSegment::IsSameStartEnd(FreeSegment *seg) const {
  return (start_ == seg->Start()) && (end_ == seg->End());
}

inline FreeSegment *FreeSegment::SingleSegAnd(FreeSegment *seg) {
  if (!IsOverlap(seg)) {
    return nullptr;
  }
  auto *result = new FreeSegment(std::max(start_, seg->Start()), std::min(end_, seg->End()));
  return result;
}

}

#endif //DALI_FREESEGMENT_H
