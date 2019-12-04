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
  Rect(): llx_(0), lly_(0), urx_(0), ury_(0) {}
  Rect(double llx, double lly, double urx, double ury): llx_(llx), lly_(lly), urx_(urx), ury_(ury) {}
  bool operator==(Rect &rhs) const;
  double LLX() const;
  double LLY() const;
  double URX() const;
  double URY() const;
  void SetValue(double llx, double lly, double urx, double ury) {llx_=llx;lly_=lly;urx_=urx;ury_=ury;}
  void ReportRectMatlab(const std::string &color = "black") const;
};


#endif //HPCC_RECT_HPP
