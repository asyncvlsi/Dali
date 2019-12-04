//
// Created by Yihang Yang on 2019-07-01.
//

#include "rect.h"

bool Rect::operator==(Rect &rhs) const {
  return (llx_ == rhs.llx_) && (lly_ == rhs.llx_) && (urx_ == rhs.llx_) && (ury_ == rhs.llx_);
}

double Rect::LLX() const {
  return llx_;
}

double Rect::LLY() const {
  return lly_;
}

double Rect::URX() const {
  return urx_;
}

double Rect::URY() const {
  return ury_;
}

void Rect::ReportRectMatlab(const std::string &color) const {
  std::cout << "rectangle('Position',[" << llx_ << " " << lly_ << " " << urx_ - llx_ << " " << ury_ - lly_ << "],'LineWidth',1, 'EdgeColor','" << color << "')\n";
}
