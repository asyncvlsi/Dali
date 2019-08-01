//
// Created by Yihang Yang on 2019-07-01.
//

#include "rect.h"

Rect::Rect(double llx, double lly, double urx, double ury) : llx_(llx), lly_(lly), urx_(urx), ury_(ury) {}

bool Rect::operator==(Rect &rhs) const {
  return (llx_ == rhs.llx_) && (lly_ == rhs.llx_) && (urx_ == rhs.llx_) && (ury_ == rhs.llx_);
}

double Rect::llx() const {
  return llx_;
}

double Rect::lly() const {
  return lly_;
}

double Rect::urx() const {
  return urx_;
}

double Rect::ury() const {
  return ury_;
}

void Rect::ReportRectMatlab(const std::string &color) const {
  std::cout << "rectangle('Position',[" << llx_ << " " << lly_ << " " << urx_ - llx_ << " " << ury_ - lly_ << "],'LineWidth',1, 'EdgeColor','" << color << "')\n";
}
