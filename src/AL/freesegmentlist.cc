//
// Created by Yihang Yang on 2019-07-17.
//

#include "freesegmentlist.h"

FreeSegmentList::FreeSegmentList() {
  _head = 0;
  _tail = 0;
  _minWidth = 0;
  _size = 0;
}

FreeSegmentList::FreeSegmentList(int initStart, int initEnd, int minWidth): _minWidth(minWidth) {
  _head = new FreeSegment(initStart, initEnd);
  _tail = _head;
  _size = 1;
}

FreeSegmentList::~FreeSegmentList() {
  clear();
}

size_t FreeSegmentList::size() {
  return _size;
}

int FreeSegmentList::left() {
  if (_head == nullptr) {
    std::cout << "Empty linked list, left() not available\n";
    assert(_head != nullptr);
  }
  return _head->start();
}

int FreeSegmentList::right() {
  if (_tail == nullptr) {
    std::cout << "Empty linked list, right() not available\n";
    assert(_tail != nullptr);
  }
  return _tail->end();
}

FreeSegment* FreeSegmentList::head() {
  return _head;
}

FreeSegment* FreeSegmentList::tail(){
  return _tail;
}

int FreeSegmentList::minWidth() {
  return _minWidth;
}

void FreeSegmentList::append(FreeSegment *segList) {
  if (segList == nullptr) {
    std::cout << "push nullptr to linked list?\n";
    assert(segList != nullptr);
  } else {
    if (_head == nullptr) {
      _head = segList;
      _tail = segList;
    } else {
      segList->setPrev(_tail);
      _tail->setNext(segList);
      _tail = segList;
    }
    ++_size;
    while (_tail->next() != nullptr) {
      _tail = _tail->next();
      ++_size;
    }
  }
}

bool FreeSegmentList::emplace_back(int start, int end) {
  auto* seg_ptr = new FreeSegment(start, end);
  if (_head == nullptr) {
    _head = seg_ptr;
    _tail = seg_ptr;
  } else {
    if (start < right()) {
      std::cout << "Illegal segment emplace back, start: " << start
                << " is required to be no less than the right end of current linked list: " << right() << "\n";
      return false;
    }
    _tail->linkSingleSeg(seg_ptr);
    _tail = seg_ptr;
  }
  ++_size;
  return true;
}

void FreeSegmentList::push_back(FreeSegment* seg) {
  if (seg == nullptr) {
    std::cout << "push nullptr to linked list?\n";
    assert(seg != nullptr);
  }
  if (seg->next() != nullptr) {
    std::cout << "argument has more than one nodes inside, please use method append() instead of push_back\n";
    assert(seg->next() == nullptr);
  }
    if (_head == nullptr) {
      _head = seg;
      _tail = seg;
    } else {
      seg->setPrev(_tail);
      _tail->setNext(seg);
      _tail = seg;
    }
    ++_size;
}

void FreeSegmentList::copyFrom(FreeSegmentList &originList) {
  this->clear();
  if (originList.head() == nullptr) {
    return;
  }
  for (FreeSegment* current = originList.head(); current != nullptr; current = current->next()) {
    emplace_back(current->start(), current->end());
  }
  _minWidth = originList.minWidth();
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
  _head = nullptr;
  _tail = nullptr;
  _size = 0;
  _minWidth = 0;
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
        segANDRowResult->append(curSeg_ptr->singleSegAnd(seg_ptr));
      }
    }
  }*/
  return result;
}

bool FreeSegmentList::applyMask(FreeSegmentList &maskRow) {
  if (_head == nullptr) {
    return false;
  }
  FreeSegmentList tmpResult;
  for (FreeSegment* currentMaskSeg = maskRow.head(); currentMaskSeg != nullptr; currentMaskSeg = currentMaskSeg->next()) {
    for (FreeSegment *current = head(); current != nullptr; current = current->next()) {

    }
  }
  return true;
}

void FreeSegmentList::show() {
  if (_head == nullptr) {
    std::cout << "Empty list, nothing to display\n";
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
