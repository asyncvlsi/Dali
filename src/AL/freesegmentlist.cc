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

size_t FreeSegmentList::size() const {
  return _size;
}

int FreeSegmentList::left() const {
  if (_head == nullptr) {
    std::cout << "Empty linked list, left() not available\n";
    assert(_head != nullptr);
  }
  return _head->start();
}

int FreeSegmentList::right() const {
  if (_tail == nullptr) {
    std::cout << "Empty linked list, right() not available\n";
    assert(_tail != nullptr);
  }
  return _tail->end();
}

FreeSegment* FreeSegmentList::head() const {
  return _head;
}

FreeSegment* FreeSegmentList::tail() const {
  return _tail;
}

int FreeSegmentList::minWidth() const {
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

void FreeSegmentList::insert(FreeSegment* insertPosition, FreeSegment* segToInsert) {
  if (insertPosition == tail()) {
    push_back(segToInsert);
  } else {
    FreeSegment *next = insertPosition->next();
    insertPosition->setNext(segToInsert);
    segToInsert->setNext(next);
    segToInsert->setPrev(insertPosition);
    next->setPrev(segToInsert);
    ++_size;
  }
}

bool FreeSegmentList::empty() const {
  return (size() == 0);
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

void FreeSegmentList::removeSeg(FreeSegment* &segInList) {
  if (segInList == nullptr) {
    std::cout << "Remove empty pointer? Be careful, you can only remove nodes in the linked list\n";
    assert(segInList != nullptr);
  }
  if (empty()) {
    std::cout << "Nothing to remove from an empty list!\n";
    assert(!empty());
  }
  FreeSegment* current = segInList;
  if (current == head()) {
    _head = current->next();
    _head->setPrev(nullptr);
    --_size;
    segInList = _head;
    free(current);
  } else if (current == tail()) {
    _tail = current->prev();
    _tail->setNext(nullptr);
    --_size;
    segInList = _tail;
    free(current);
  } else {
    FreeSegment* prev = current->prev();
    FreeSegment* next = current->next();
    prev->setNext(next);
    next->setPrev(prev);
    --_size;
    segInList = prev;
    free(current);
  }
}

bool FreeSegmentList::applyMask(FreeSegmentList &maskRow) {
  if (_head == nullptr) {
    return false;
  }
  FreeSegmentList result;
  for (FreeSegment* currentMaskSeg = maskRow.head(); currentMaskSeg != nullptr; currentMaskSeg = currentMaskSeg->next()) {
    for (FreeSegment *current = head(); current != nullptr; current = current->next()) {
      if (current->dominate(currentMaskSeg)) break;
      FreeSegment* tmpSeg_ptr = currentMaskSeg->singleSegAnd(current);
      if (tmpSeg_ptr != nullptr) {
        result.push_back(tmpSeg_ptr);
      }
    }
  }
  copyFrom(result);
  return true;
}

void FreeSegmentList::removeShortSeg(int minWidth) {
  if (empty()) return;
  for (FreeSegment* current = head(); current != nullptr; current = current->next()) {
    if (current->length() < minWidth) {
      removeSeg(current);
    }
  }
}

void FreeSegmentList::useSpace(int locToStart, int lengthToUse) {
  /****use a segment of this free segment:
 * 1. use all free space (remove this segment from the linked list level),
 * 2. use left part, use right part (shrink this segment),
 * 3. use middle part (split)****/
  int endLoc = locToStart + lengthToUse;
  bool isSegFound = false;
  for (auto* current = head(); current != nullptr; current = current->next()) {
    if ((locToStart >= current->start()) && (endLoc <= current->end())) {
      isSegFound = true;
      if ((locToStart == current->start()) && (endLoc == current->end())) {
        removeSeg(current);
      } else if ((locToStart == current->start()) && (endLoc < current->end())) {
        current->setStart(endLoc);
      } else if ((locToStart > current->start()) && (endLoc == current->end())) {
        current->setEnd(locToStart);
      } else {
        current->setEnd(locToStart);
        auto* seg_ptr = new FreeSegment(endLoc, current->end());
        insert(current, seg_ptr);
      }
    }
  }
  if (!isSegFound) {
    std::cout << "What? there is a bug in the code, you should not reach here\n";
    assert(isSegFound);
  }
}

void FreeSegmentList::show() {
  if (_head == nullptr) {
    std::cout << "Empty list, nothing to display\n";
  } else {
    std::cout << "minWidth: " << _minWidth << "  ";
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
