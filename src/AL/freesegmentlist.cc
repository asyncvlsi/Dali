//
// Created by Yihang Yang on 2019-07-17.
//

#include "freesegmentlist.h"

FreeSegmentList::FreeSegmentList(int minWidth): _minWidth(minWidth) {
  _head = nullptr;
  _tail = nullptr;
  _size = 0;
}

FreeSegmentList::~FreeSegmentList() {
  clear();
}

size_t FreeSegmentList::size() {
  return _size;
}

void FreeSegmentList::push_back(FreeSegment* seg) {
  if (seg == nullptr) {
    std::cout << "push nullptr to linked list?\n";
    assert(seg != nullptr);
  } else {
    if (_head == nullptr) {
      _head = seg;
      _tail = seg;
    } else {
      seg->setPrev(_tail);
      _tail->setNext(seg);
      _tail = seg;
    }
    ++_size;
    while (_tail->next() != nullptr) {
      _tail = _tail->next();
      ++_size;
    }
  }
}

void FreeSegmentList::clear() {
  // clear the linked list
  FreeSegment* current = _head;
  FreeSegment* next = nullptr;
  while (current != nullptr) {
    next = current->next();
    free(current);
    current = next;
  }
}

FreeSegment* FreeSegmentList::ANDSingleSeg(FreeSegment* seg) {
  /****each free segment in seg will do AND with each single free segment in "this" row,
   * and OR the final result together
   * requirement: no overlap between free segments in seg and "this" row.
   * (this requirement is safe, if users only call APIs)***/
  FreeSegment* result = nullptr;
  /*for (FreeSegment* curSeg_ptr = seg; curSeg_ptr != nullptr; curSeg_ptr = curSeg_ptr->next()) {
    FreeSegment* segANDRowResult = nullptr;
    for (FreeSegment* seg_ptr = this; seg_ptr != nullptr; seg_ptr = seg_ptr->next()) {
      if (segANDRowResult == nullptr) {
        segANDRowResult = curSeg_ptr->singleSegAnd(seg_ptr);
      } else {
        segANDRowResult->push_back(curSeg_ptr->singleSegAnd(seg_ptr));
      }
    }
  }*/
  return result;
}

void FreeSegmentList::show() {
  if (_head == nullptr) {
    std::cout << "Empty list\n";
  } else {
    FreeSegment* current = _head;
    size_t count = 0;
    while (count < _size) {
      std::cout << "( " << current->start() << " " << current->end() << " )";
      current = current->next();
      if (current != nullptr) {
        std::cout << " -> ";
      } else {
        break;
      }
      ++count;
    }
    std::cout << "\n";
  }
}
