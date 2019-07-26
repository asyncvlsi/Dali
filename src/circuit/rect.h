//
// Created by Yihang Yang on 2019-07-01.
//

#ifndef HPCC_RECT_HPP
#define HPCC_RECT_HPP

#include <string>
#include <iostream>

class Rect {
private:
  int llx_, lly_, urx_, ury_;
public:
  Rect(int llx, int lly, int urx, int ury);
  bool operator==(Rect &rhs) const;
  int llx() const;
  int lly() const;
  int urx() const;
  int ury() const;
  void ReportRectMatlab(const std::string &color = "black") const;
};


#endif //HPCC_RECT_HPP
