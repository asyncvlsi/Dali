//
// Created by yihan on 7/13/2019.
//

#include "freesegment.h"

FreeSegment::FreeSegment(int initStart, int initEnd): _start(initStart), _end(initEnd) {
  assert(initStart <= initEnd);
}

bool FreeSegment::setPrev(FreeSegment* preFreeSeg_ptr) {
  _prev = preFreeSeg_ptr;
  return true;
}

bool FreeSegment::setNext(FreeSegment* nextFreeSeg_ptr) {
  _next = nextFreeSeg_ptr;
  return true;
}

bool FreeSegment::linkSingleSeg(FreeSegment *seg_ptr) {
  if (seg_ptr == nullptr) {
    std::cout << "Want to link to an empty pointer?\n";
    assert(seg_ptr != nullptr);
  }
  if ((this->next() != nullptr) || (seg_ptr->next() != nullptr)) {
    std::cout << "This member function is not for concatenating multi nodes linked list\n";
    assert((this->next() == nullptr) && (seg_ptr->next() == nullptr));
  }
  return (setNext(seg_ptr) && seg_ptr->setPrev(this));
}

FreeSegment* FreeSegment::next() {
  return _next;
}

FreeSegment* FreeSegment::prev() {
  return  _prev;
}

void FreeSegment::setSpan(int startLoc, int endLoc) {
  if (startLoc > endLoc) {
    std::cout << "Cannot set the span of a segment with start larger than end, start" << startLoc
              << " end: " << endLoc << std::endl;
    assert(startLoc <= endLoc);
  }
  _start = startLoc;
  _end = endLoc;
}

int FreeSegment::start() const {
  return  _start;
}

int FreeSegment::end() const {
  return  _end;
}

int FreeSegment::length() const {
  return _end - _start;
}

bool FreeSegment::isOverlap(FreeSegment* seg) const {
  if ((length() == 0) || (seg->length() == 0)) {
    std::cout << "Length 0 segment?!\n";
    return false;
  }
  bool notOverlap = (_end <= seg->start()) || (_start >= seg->end());
  return !notOverlap;
}

bool FreeSegment::isTouch(FreeSegment* seg) const {
  if ((length() == 0) || (seg->length() == 0)) {
    std::cout << "Length 0 segment?!\n";
    return false;
  }
  return (_end == seg->start()) || (_start == seg->end());
}

bool FreeSegment::dominate(FreeSegment* seg) const {
  return (_start >= seg->end());
}

FreeSegment* FreeSegment::singleSegAnd(FreeSegment* seg) {
  if (!isOverlap(seg)) {
    return nullptr;
  }
  auto *result = new FreeSegment(std::max(_start, seg->start()), std::min(_end, seg->end()));
  return result;
}

FreeSegment* FreeSegment::singleSegOr(FreeSegment* seg) {
  if ((length() == 0) && (seg->length() == 0)) {
    std::cout << "What?! two segments with length 0 for OR operation\n";
    return nullptr;
  }
  auto *result = new FreeSegment;
  if (_start > seg->end() || _end < seg->start()) { // no overlap, no touch
    int firstStart, firstEnd, secondStart, secondEnd;
    if (_start < seg->start()) {
      firstStart = _start;
      firstEnd = _end;
      secondStart = seg->start();
      secondEnd = seg->end();
    } else {
      firstStart = seg->start();
      firstEnd = seg->end();
      secondStart = _start;
      secondEnd = _end;
    }
    result->setSpan(firstStart,firstEnd);
    auto *secondSeg = new FreeSegment(secondStart, secondEnd);
    result->setNext(secondSeg);
  } else if (_start == seg->end()) { // _start touches the end of seg
    result->setSpan(seg->start(),_end);
  } else if (_end == seg->start()) { // _end touches the start of seg
    result->setSpan(_start,seg->end());
  } else { // non-zero overlap
    result->setSpan(std::min(_start, seg->start()), std::max(_end, seg->end()));
  }
  return result;
}

void FreeSegment::clear() {
  FreeSegment* current = this;
  FreeSegment* next = nullptr;
  while (current != nullptr) {
    next = current->next();
    delete current;
    current = next;
  }
}