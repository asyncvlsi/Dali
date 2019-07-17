//
// Created by yihan on 7/12/2019.
//

#ifndef HPCC_AVAILSPACE_H
#define HPCC_AVAILSPACE_H

#include <cassert>
#include <iostream>

class AvailSpace {
private:
  double _start;
  double _end;
public:
  explicit AvailSpace(double initStart = 0, double initEnd = 0);
  void set_start(double sta);
  void increStart(double delta);
  void set_end(double en);
  double start() const;
  double end() const;
  bool operator <(const AvailSpace &rhs) const;
  bool operator >(const AvailSpace &rhs) const;
  bool operator ==(const AvailSpace &rhs) const;
};


#endif //HPCC_AVAILSPACE_H
