//
// Created by Yihang Yang on 2019-07-01.
//

#include "rect.hpp"

rect_t::rect_t(int llx, int lly, int urx, int ury) : _llx(llx), _lly(lly), _urx(urx), _ury(ury) {}

void rect_t::report_rect_matlab(std::string color) const {
  std::cout << "rectangle('Position',[" << _llx << " " << _lly << " " << _urx - _llx << " " << _ury - _lly << "],'LineWidth',1, 'EdgeColor','" << color << "')\n";
}
