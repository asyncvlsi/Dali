//
// Created by yihan on 7/13/2019.
//

#include "FreeSegment.h"

FreeSegment::FreeSegment(int initStart, int initEnd): _start(initStart), _end(initEnd) {}

bool FreeSegment::setPrev(FreeSegment *preFreeSegptr) {
  _prev = preFreeSegptr;
  return true;
}

bool FreeSegment::setNext(FreeSegment *nextFreeSegptr) {
  _next = nextFreeSegptr;
  return true;
}

bool FreeSegment::useSpace(int startLoc, int endLoc) {
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
  if (startLoc == _start) {
    _start = _end;
  }
  return true;
}

int FreeSegment::start() const {
  return  _start;
}

int FreeSegment::end() const {
  return  _end;
}