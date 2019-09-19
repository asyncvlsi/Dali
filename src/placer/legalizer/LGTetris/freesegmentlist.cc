//
// Created by Yihang Yang on 2019-07-17.
//

#include "freesegmentlist.h"

FreeSegmentList::FreeSegmentList() {
  _head = nullptr;
  _tail = nullptr;
  _minWidth = 0;
  _size = 0;
}

FreeSegmentList::FreeSegmentList(int initStart, int initEnd, int initMinWidth): _minWidth(initMinWidth) {
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

void FreeSegmentList::setminWidth(int initMinWidth) {
  _minWidth = initMinWidth;
}

void FreeSegmentList::append(FreeSegment *segList) {
  /****push a list of freesegment into the linked list with some sanity check****/
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
  /****push a single free segment into the linked list****/
  if (seg == nullptr) {
    std::cout << "push nullptr to linked list?\n";
    assert(seg != nullptr);
  }
  if (seg->next() != nullptr) {
    std::cout << "argument has more than one nodes inside, please use method append() instead of push_back\n";
    assert(seg->next() == nullptr);
  }
  if (empty()) {
    _head = seg;
    _tail = seg;
    ++_size;
  } else {
    if (right() > seg->start()) {
      std::cout << "Cannot push segment into list, because the right of list is: " << right()
                << " larger than the start of segment to push: " << seg->start() << std::endl;
      assert(right() <= seg->start());
    }
    if (right() < seg->start()) {
      seg->setPrev(_tail);
      _tail->setNext(seg);
      _tail = seg;
      ++_size;
    } else {
      _tail->setSpan(_tail->start(), seg->end());
    }
  }
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
  if (!originList.empty()) {
    for (FreeSegment *current = originList.head(); current != nullptr; current = current->next()) {
      emplace_back(current->start(), current->end());
    }
    _minWidth = originList.minWidth();
  }
}

void FreeSegmentList::clear() {
  // clear the linked list
  FreeSegment* current = _head;
  FreeSegment* next = nullptr;
  while (current != nullptr) {
    next = current->next();
    delete current;
    current = next;
  }
  _head = nullptr;
  _tail = nullptr;
  _size = 0;
  _minWidth = 0;
}

void FreeSegmentList::removeSeg(FreeSegment* segInList) { // don't remove, traverse the linked list is good enough
  /****remove a segment in the linked list
   * 1. if the linked list is empty, assert fault;
   * 2. if the linked list has length 1, clear this linked list;
   * 3. if the linked list has length larger than 1:
   *    a). if the segment to remove is the head, set the head to the next one;
   *    b). if the segment to remove is the tail, set the tail to the prev one;
   *    c). if the segment to remove has real prev and next, i.e., not nullptr, remove this segment,
   *        and connect its prev with its next****/
  if (segInList == nullptr) {
    std::cout << "Remove empty pointer? Be careful, you can only remove nodes in the linked list\n";
    assert(segInList != nullptr);
  }
  if (empty()) {
    std::cout << "Nothing to remove from an empty list!\n";
    assert(!empty());
  }
  FreeSegment* current = segInList;
  if (size() == 1) {
    delete current;
    _head = nullptr;
    _tail = nullptr;
    _size = 0;
    segInList = nullptr;
  } else {
    if (current == head()) {
      _head = current->next();
      _head->setPrev(nullptr);
      --_size;
      delete current;
    } else if (current == tail()) {
      _tail = current->prev();
      _tail->setNext(nullptr);
      --_size;
      delete current;
    } else {
      FreeSegment *prev = current->prev();
      FreeSegment *next = current->next();
      prev->setNext(next);
      next->setPrev(prev);
      --_size;
      delete current;
    }
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

void FreeSegmentList::removeShortSeg(int width) {
  /****to understand this member function, one needs to understand member function removeSeg()
   * the member function works in the following way:
   * traverse the linked list
   * 1. if the current segment has length less than the required length, remove it;
   * 2. if the current segment has length no less than the required length, skip;
   * 3. set current pointer to its next.****/
  if (empty()) return;
  for (FreeSegment* current = head(); current != nullptr;) {
    FreeSegment* next = current->next();
    if (current->length() < width) {
      removeSeg(current);
    }
    current = next;
  }
}

void FreeSegmentList::useSpace(int locToStart, int lengthToUse) {
  /****use a segment of this free segment:
 * 1. use all free space (remove this segment from the linked list level);
 * 2. use left part;
 * 3. right part (shrink this segment);
 * 4. use middle part (split)****/
  int endLoc = locToStart + lengthToUse;
  bool isSegFound = false;
  for (auto* current = head(); current != nullptr; current = current->next()) {
    if ((locToStart >= current->start()) && (endLoc <= current->end())) {
      isSegFound = true;
      if ((locToStart == current->start()) && (endLoc == current->end())) {
        removeSeg(current);
      } else if ((locToStart == current->start()) && (endLoc < current->end())) {
        current->setSpan(endLoc, current->end());
      } else if ((locToStart > current->start()) && (endLoc == current->end())) {
        current->setSpan(current->start(), locToStart);
      } else {
        auto* seg_ptr = new FreeSegment(endLoc, current->end());
        current->setSpan(current->start(), locToStart);
        insert(current, seg_ptr);
      }
    }
    // some issues here, not clearly understood
    if (isSegFound) break;
  }

  if (!isSegFound) {
    std::cout << "What? there are bugs in the code, the program should not reach here" << std::endl;
    assert(isSegFound);
  }
}

bool FreeSegmentList::IsSpaceAvail(double x_loc, double y_loc, int width, int height) {
  return true;
}

void FreeSegmentList::Show() {
  if (_head == nullptr) {
    std::cout << "Empty list, nothing to display" <<std::endl;
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
    std::cout << std::endl;
  }
}
