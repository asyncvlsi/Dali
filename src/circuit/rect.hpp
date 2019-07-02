//
// Created by Yihang Yang on 2019-07-01.
//

#ifndef HPCC_RECT_HPP
#define HPCC_RECT_HPP

#include <string>
#include <iostream>

class rect_t {
private:
  int _llx, _lly, _urx, _ury;
public:
  rect_t(int llx, int lly, int urx, int ury);

  void report_rect_matlab(std::string color = "black") const;
};


#endif //HPCC_RECT_HPP
