//
// Created by yihan on 7/13/2019.
//

#include "FreeSegment.h"

FreeSegment::FreeSegment(int initStart, int initEnd, int minWidth): _start(initStart), _end(initEnd), _minWidth(minWidth) {}

bool FreeSegment::setPrev(FreeSegment* preFreeSeg_ptr) {
  _prev = preFreeSeg_ptr;
  return true;
}

bool FreeSegment::setNext(FreeSegment* nextFreeSeg_ptr) {
  _next = nextFreeSeg_ptr;
  return true;
}

FreeSegment* FreeSegment::next() const {
  return _next;
}

FreeSegment* FreeSegment::prev() const {
  return  _prev;
}

bool FreeSegment::useSpace(int startLoc, int endLoc) {
  /****use a segment of this free segment:
   * 1. use all free space (remove this segment),
   * 2. use left part, use right part (shrink this segment),
   * 3. use middle part (split)****/
  if (startLoc < _start) {
    std::cout << "Error!\n";
    std::cout << "Invalid input, startLoc should be >= the starting point of a free segment\n";
    assert(startLoc >= _start);
  }
  if (endLoc > _end) {
    std::cout << "Error!\n";
    std::cout << "Invalid input, endLoc should be <= the start point of a free segment\n";
    assert(endLoc <= _end);
  }
  if ((startLoc == _start) && (endLoc == _end)) {
    _prev = _next;
    delete this; // this might be bad
  }

  return true;
}

void FreeSegment::setStart(int startLoc) {
  _start = startLoc;
}

int FreeSegment::start() const {
  return  _start;
}

void FreeSegment::setEnd(int endLoc) {
  _end = endLoc;
}

int FreeSegment::end() const {
  return  _end;
}

bool FreeSegment::setTail(FreeSegment *seg) {
  if (tail == nullptr) {
    std::cout << "Set tail-ptr to a nullptr?!\n";
    return false;
  }
  _tail = seg;
  return true;
}

FreeSegment* FreeSegment::tail() {
  return _tail;
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
    result->setStart(firstStart);
    result->setEnd(firstEnd);

    auto *secondSeg = new FreeSegment(secondStart, secondEnd);
    result->setNext(secondSeg);
  } else if (_start == seg->end()) { // _start touches the end of seg
    result->setStart(seg->start());
    result->setEnd(_end);
  } else if (_end == seg->start()) { // _end touches the start of seg
    result->setStart(_start);
    result->setEnd(seg->end());
  } else { // non-zero overlap
    result->setStart(std::min(_start, seg->start()));
    result->setEnd(std::max(_end, seg->end()));
  }
  return result;
}

FreeSegment* FreeSegment::SegAnd(FreeSegment* seg) {
  /****each free segment in seg will do AND with each single free segment in "this" row,
   * and OR the final result together
   * requirement: no overlap between free segments in seg and "this" row.
   * (this requirement is safe, if users only call APIs)***/
  FreeSegment* result = nullptr;
  for (FreeSegment* curSeg_ptr = seg; curSeg_ptr != nullptr; curSeg_ptr = curSeg_ptr->next()) {
    FreeSegment* segANDRowResult = nullptr;
    for (FreeSegment* seg_ptr = this; seg_ptr != nullptr; seg_ptr = seg_ptr->next()) {
      if (segANDRowResult == nullptr) {
        segANDRowResult = curSeg_ptr->singleSegAnd(seg_ptr);
      } else {
        segANDRowResult->push_back(curSeg_ptr->singleSegAnd(seg_ptr));
      }
    }
  }
}

FreeSegment* FreeSegment::SegOr(FreeSegment* seg) {
  /****each free segment in seg will do OR with each single free segment in "this" row,
   * and OR the final result together
   * requirement: no overlap between free segments in seg and "this" row.
   * (this requirement is safe, if users only call APIs)***/
}

void FreeSegment::push_back(FreeSegment* seg) {
  FreeSegment* tail_ptr = tail();
  tail_ptr->setNext(seg);
  setTail(seg);
}  

void FreeSegment::clear() {
  // clear things after this node
  FreeSegment* current = this;
  FreeSegment* next = nullptr;
  while (current != nullptr) {
    next = current->next();
    free(current);
    current = next;
  }
}