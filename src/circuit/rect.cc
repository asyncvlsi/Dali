//
// Created by Yihang Yang on 2019-07-01.
//

#include "rect.h"

Rect::Rect(int llx, int lly, int urx, int ury) : llx_(llx), lly_(lly), urx_(urx), ury_(ury) {}

bool Rect::operator==(Rect &rhs) const {
  return (llx_ == rhs.llx_) && (lly_ == rhs.llx_) && (urx_ == rhs.llx_) && (ury_ == rhs.llx_);
}

int Rect::llx() const {
  return llx_;
}

int Rect::lly() const {
  return lly_;
}

int Rect::urx() const {
  return urx_;
}

int Rect::ury() const {
  return ury_;
}

void Rect::ReportRectMatlab(const std::string &color) const {
  std::cout << "rectangle('Position',[" << llx_ << " " << lly_ << " " << urx_ - llx_ << " " << ury_ - lly_ << "],'LineWidth',1, 'EdgeColor','" << color << "')\n";
}
