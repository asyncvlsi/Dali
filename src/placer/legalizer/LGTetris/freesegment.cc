//
// Created by yihan on 7/13/2019.
//

#include "freesegment.h"

FreeSegment::FreeSegment(int start, int stop): start_(start), end_(stop) {
  assert(start <= stop);
}

bool FreeSegment::SetPrev(FreeSegment* preFreeSeg_ptr) {
  prev_ = preFreeSeg_ptr;
  return true;
}

bool FreeSegment::SetNext(FreeSegment* nextFreeSeg_ptr) {
  next_ = nextFreeSeg_ptr;
  return true;
}

bool FreeSegment::LinkSingleSeg(FreeSegment *seg_ptr) {
  if (seg_ptr == nullptr) {
    std::cout << "Want to link to an Empty pointer?\n";
    assert(seg_ptr != nullptr);
  }
  if ((this->Next() != nullptr) || (seg_ptr->Next() != nullptr)) {
    std::cout << "This member function is not for concatenating multi nodes linked list\n";
    assert((this->Next() == nullptr) && (seg_ptr->Next() == nullptr));
  }
  return (SetNext(seg_ptr) && seg_ptr->SetPrev(this));
}

FreeSegment* FreeSegment::Next() {
  return next_;
}

FreeSegment* FreeSegment::Prev() {
  return  prev_;
}

void FreeSegment::SetSpan(int startLoc, int endLoc) {
  if (startLoc > endLoc) {
    std::cout << "Cannot set the span of a segment with start larger than End, Start" << startLoc
              << " End: " << endLoc << std::endl;
    assert(startLoc <= endLoc);
  }
  start_ = startLoc;
  end_ = endLoc;
}

int FreeSegment::Start() const {
  return  start_;
}

int FreeSegment::End() const {
  return  end_;
}

int FreeSegment::Length() const {
  return end_ - start_;
}

bool FreeSegment::IsOverlap(FreeSegment* seg) const {
  if ((Length() == 0) || (seg->Length() == 0)) {
    std::cout << "Length 0 segment?!\n";
    return false;
  }
  bool notOverlap = (end_ <= seg->Start()) || (start_ >= seg->End());
  return !notOverlap;
}

bool FreeSegment::IsTouch(FreeSegment* seg) const {
  if ((Length() == 0) || (seg->Length() == 0)) {
    std::cout << "Length 0 segment?!\n";
    return false;
  }
  return (end_ == seg->Start()) || (start_ == seg->End());
}

bool FreeSegment::IsDominate(FreeSegment* seg) const {
  /****
   * If this FreeSegment is on the right hand side of seg, and has common overlap length 0, return true
   *
   * example: |---seg---| ... |---this seg---|, return true
   * example: |---seg---|---this seg---|, return true
   * else return false
   * ****/
  return (start_ >= seg->End());
}

bool FreeSegment::IsContain(FreeSegment* seg) const {
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

FreeSegment* FreeSegment::SingleSegAnd(FreeSegment* seg) {
  if (!IsOverlap(seg)) {
    return nullptr;
  }
  auto *result = new FreeSegment(std::max(start_, seg->Start()), std::min(end_, seg->End()));
  return result;
}

FreeSegment* FreeSegment::SingleSegOr(FreeSegment* seg) {
  if ((Length() == 0) && (seg->Length() == 0)) {
    std::cout << "What?! two segments with Length 0 for OR operation\n";
    return nullptr;
  }
  auto *result = new FreeSegment;
  if (start_ > seg->End() || end_ < seg->Start()) { // no overlap, no touch
    int firstStart, firstEnd, secondStart, secondEnd;
    if (start_ < seg->Start()) {
      firstStart = start_;
      firstEnd = end_;
      secondStart = seg->Start();
      secondEnd = seg->End();
    } else {
      firstStart = seg->Start();
      firstEnd = seg->End();
      secondStart = start_;
      secondEnd = end_;
    }
    result->SetSpan(firstStart, firstEnd);
    auto *secondSeg = new FreeSegment(secondStart, secondEnd);
    result->SetNext(secondSeg);
  } else if (start_ == seg->End()) { // start_ touches the End of seg
    result->SetSpan(seg->Start(), end_);
  } else if (end_ == seg->Start()) { // end_ touches the Start of seg
    result->SetSpan(start_, seg->End());
  } else { // non-zero overlap
    result->SetSpan(std::min(start_, seg->Start()), std::max(end_, seg->End()));
  }
  return result;
}

void FreeSegment::Clear() {
  FreeSegment* current = this;
  FreeSegment* next = nullptr;
  while (current != nullptr) {
    next = current->Next();
    delete current;
    current = next;
  }
}