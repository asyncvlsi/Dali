//
// Created by Yihang Yang on 2019-07-01.
//

#ifndef HPCC_RECT_HPP
#define HPCC_RECT_HPP

#include <string>
#include <iostream>

class Rect {
 private:
  double llx_, lly_, urx_, ury_;
 public:
  Rect(double llx, double lly, double urx, double ury);
  bool operator==(Rect &rhs) const;
  double llx() const;
  double lly() const;
  double urx() const;
  double ury() const;
  void ReportRectMatlab(const std::string &color = "black") const;
};


#endif //HPCC_RECT_HPP
