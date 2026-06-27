/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#include "component.h"

#include <algorithm>

#include "dali/common/helper.h"

namespace dali {

void Component::SetHeight(int height) {
  eff_height_ = height;
  eff_area_ = eff_height_ * macro_ptr_->Width();
}

void Component::ResetHeight() {
  eff_height_ = macro_ptr_->Height();
  eff_area_ = macro_ptr_->Area();
}

bool Component::IsFlipped() const {
  return orient_ == FN || orient_ == FS || orient_ == FW || orient_ == FE;
}

void Component::SetMacro(Macro* macro_ptr) {
  DaliExpects(macro_ptr != nullptr, "Set Macro to nullptr?");
  macro_ptr_ = macro_ptr;
  eff_height_ = macro_ptr_->Height();
  eff_area_ = macro_ptr_->Area();
}

void Component::SetLoc(double lx, double ly) {
  llx_ = lx;
  lly_ = ly;
}

void Component::SetPlacementStatus(PlaceStatus place_status) {
  place_status_ = place_status;
}

void Component::SetOrient(ComponentOrient orient) { orient_ = orient; }

void Component::SetAux(ComponentAux* aux) { aux_ptr_ = aux; }

void Component::SwapLoc(Component& component) {
  double tmp_x = llx_;
  double tmp_y = lly_;
  llx_ = component.LLX();
  lly_ = component.LLY();
  component.SetLLX(tmp_x);
  component.SetLLY(tmp_y);
}

void Component::IncreaseX(double displacement, double upper, double lower) {
  llx_ += displacement;
  double real_upper = upper - Width();
  if (llx_ < lower) {
    llx_ = lower;
  } else if (llx_ > real_upper) {
    llx_ = real_upper;
  }
}

void Component::IncreaseY(double displacement, double upper, double lower) {
  lly_ += displacement;
  double real_upper = upper - Height();
  if (lly_ < lower) {
    lly_ = lower;
  } else if (lly_ > real_upper) {
    lly_ = real_upper;
  }
}

double Component::OverlapArea(const Component& component) const {
  double overlap_area = 0;
  if (IsOverlap(component)) {
    double llx, urx, lly, ury;
    llx = std::max(LLX(), component.LLX());
    urx = std::min(URX(), component.URX());
    lly = std::max(LLY(), component.LLY());
    ury = std::min(URY(), component.URY());
    overlap_area = (urx - llx) * (ury - lly);
  }
  return overlap_area;
}

void Component::SetStretchLength(size_t index, int length) {
  size_t sz = stretch_length_.size();
  DaliExpects(index < sz, "Out of bound");
  if (IsFlipped()) {
    index = sz - 1 - index;
  }
  stretch_length_[index] = length;
  total_stretch_length_ =
      std::accumulate(stretch_length_.begin(), stretch_length_.end(), 0);
}

std::vector<int>& Component::StretchLengths() { return stretch_length_; }

const std::vector<int>& Component::StretchLengths() const {
  return stretch_length_;
}

int Component::CumulativeStretchLength(size_t index) const {
  if (MacroPtr()->RegionCount() == 1) return 0;
  if (stretch_length_.empty()) return 0;

  size_t sz = stretch_length_.size();
  DaliExpects(index <= sz, "Out of bound");

  int res = 0;
  for (size_t i = 0; i < index; ++i) {
    res += stretch_length_[i];
  }
  return res;
}

void Component::Report() {
  LOG(info) << "  component name: " << Name() << "\n"
            << "    component macro: " << MacroPtr()->Name() << "\n"
            << "    width and height: " << Width() << " " << Height() << "\n"
            << "    lower left corner: " << llx_ << " " << lly_ << "\n"
            << "    movable: " << IsMovable() << "\n"
            << "    orientation: " << OrientStr(orient_) << "\n"
            << "    assigned primary key: " << Id() << "\n";
}

void Component::ReportNet() {
  LOG(info) << Name() << " connects to:\n";
  for (auto& net_num : nets_) {
    LOG(info) << net_num << "  ";
  }
  LOG(info) << "\n";
}

void Component::ExportWellToMatlabPatchRect(std::ofstream& ost) {
  std::vector<RectI> n_well_shapes;
  std::vector<RectI> p_well_shapes;
  if (MacroPtr()->HasWellInfo()) {
    auto& n_rects = MacroPtr()->Nrects();
    for (auto& rect : n_rects) {
      n_well_shapes.push_back(rect);
    }
    auto& p_rects = MacroPtr()->Prects();
    for (auto& rect : p_rects) {
      p_well_shapes.push_back(rect);
    }
  }

  size_t sz = n_well_shapes.size();
  for (size_t i = 0; i < sz; ++i) {
    int length = CumulativeStretchLength(i);
    if (Orient() == N) {
      RectD n_rect(LLX() + n_well_shapes[i].LLX(),
                   LLY() + (n_well_shapes[i].LLY() + length),
                   LLX() + n_well_shapes[i].URX(),
                   LLY() + (n_well_shapes[i].URY() + length));
      RectD p_rect(LLX() + p_well_shapes[i].LLX(),
                   LLY() + (p_well_shapes[i].LLY() + length),
                   LLX() + p_well_shapes[i].URX(),
                   LLY() + (p_well_shapes[i].URY() + length));
      SaveMatlabPatchRegion(ost, n_rect, p_rect);
    } else if (Orient() == FS) {
      RectD n_rect(LLX() + n_well_shapes[i].LLX(),
                   URY() - (n_well_shapes[i].URY() + length),
                   LLX() + n_well_shapes[i].URX(),
                   URY() - (n_well_shapes[i].LLY() + length));
      RectD p_rect(LLX() + p_well_shapes[i].LLX(),
                   URY() - (p_well_shapes[i].URY() + length),
                   LLX() + p_well_shapes[i].URX(),
                   URY() - (p_well_shapes[i].LLY() + length));
      SaveMatlabPatchRegion(ost, n_rect, p_rect);
    } else {
      LOG(debug) << "Orientation not supported " << __FILE__ << " : "
                 << __LINE__ << " : " << __FUNCTION__ << "\n";
    }
  }
}

}  // namespace dali
