//
// Created by yihan on 7/12/2019.
//

#include "availspace.h"

AvailSpace::AvailSpace(double initStart, double initEnd): _start(initStart), _end(initEnd){}

void AvailSpace::set_start(double sta) {
  _start =sta;
}

void AvailSpace::increStart(double delta){
  if (delta < 0) {
    std::cout << "Error! The beginning of avail space can only increase\n";
    assert(delta > 0);
  }
  _start += delta;
}

void AvailSpace::set_end(double en) {
  _end = en;
}

double AvailSpace::start() const {
  return  _start;
}

double AvailSpace::end() const {
  return  _end;
}

bool AvailSpace::operator <(const AvailSpace &rhs) const {
  return this->_start < rhs._start;
}

bool AvailSpace::operator >(const AvailSpace &rhs) const {
  return this->_start > rhs._start;
}

bool AvailSpace::operator ==(const AvailSpace &rhs) const {
  return this->_start == rhs._start && this->_end == rhs._end;
}