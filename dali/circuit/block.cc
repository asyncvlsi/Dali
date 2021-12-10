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
#include "block.h"

#include "dali/common/helper.h"

namespace dali {

Block::Block() :
    type_ptr_(nullptr),
    name_id_pair_ptr_(nullptr),
    llx_(0),
    lly_(0),
    place_status_(UNPLACED),
    orient_(N),
    aux_ptr_(nullptr) {}

Block::Block(
    BlockType *type_ptr,
    std::pair<const std::string, int> *name_id_pair_ptr,
    int llx,
    int lly,
    bool movable,
    BlockOrient orient
) : type_ptr_(type_ptr),
    name_id_pair_ptr_(name_id_pair_ptr),
    llx_(llx),
    lly_(lly),
    orient_(orient) {
  DaliExpects(name_id_pair_ptr != nullptr, "nullptr for name and index?");
  DaliExpects(type_ptr != nullptr, "nullptr for type");
  eff_height_ = type_ptr_->Height();
  eff_area_ = type_ptr_->Area();
  aux_ptr_ = nullptr;
  if (movable) {
    place_status_ = UNPLACED;
  } else {
    place_status_ = FIXED;
  }
}

Block::Block(
    BlockType *type_ptr,
    std::pair<const std::string, int> *name_id_pair_ptr,
    int llx,
    int lly,
    PlaceStatus place_state,
    BlockOrient orient
) : type_ptr_(type_ptr),
    name_id_pair_ptr_(name_id_pair_ptr),
    llx_(llx),
    lly_(lly),
    place_status_(place_state),
    orient_(orient) {
  DaliExpects(name_id_pair_ptr != nullptr, "nullptr for name and index?");
  DaliExpects(type_ptr != nullptr, "nullptr for type");
  eff_height_ = type_ptr_->Height();
  eff_area_ = type_ptr_->Area();
  aux_ptr_ = nullptr;
}

void Block::SetHeight(int height) {
  eff_height_ = height;
  eff_area_ = eff_height_ * type_ptr_->Width();
}

void Block::ResetHeight() {
  eff_height_ = type_ptr_->Height();
  eff_area_ = type_ptr_->Area();
}

bool Block::IsFlipped() const {
  return orient_ == FN || orient_ == FS || orient_ == FW || orient_ == FE;
}

void Block::SetType(BlockType *type_ptr) {
  DaliExpects(type_ptr != nullptr, "Set BlockType to nullptr?");
  type_ptr_ = type_ptr;
  eff_height_ = type_ptr_->Height();
  eff_area_ = type_ptr_->Area();
}

void Block::SetLoc(double lx, double ly) {
  llx_ = lx;
  lly_ = ly;
}

void Block::SetPlacementStatus(PlaceStatus place_status) {
  place_status_ = place_status;
}

void Block::SetOrient(BlockOrient orient) {
  orient_ = orient;
}

void Block::SetAux(BlockAux *aux) {
  aux_ptr_ = aux;
}

void Block::SwapLoc(Block &blk) {
  double tmp_x = llx_;
  double tmp_y = lly_;
  llx_ = blk.LLX();
  lly_ = blk.LLY();
  blk.SetLLX(tmp_x);
  blk.SetLLY(tmp_y);
}

void Block::IncreaseX(double displacement, double upper, double lower) {
  llx_ += displacement;
  double real_upper = upper - Width();
  if (llx_ < lower) {
    llx_ = lower;
  } else if (llx_ > real_upper) {
    llx_ = real_upper;
  }
}

void Block::IncreaseY(double displacement, double upper, double lower) {
  lly_ += displacement;
  double real_upper = upper - Height();
  if (lly_ < lower) {
    lly_ = lower;
  } else if (lly_ > real_upper) {
    lly_ = real_upper;
  }
}

double Block::OverlapArea(const Block &blk) const {
  double overlap_area = 0;
  if (IsOverlap(blk)) {
    double llx, urx, lly, ury;
    llx = std::max(LLX(), blk.LLX());
    urx = std::min(URX(), blk.URX());
    lly = std::max(LLY(), blk.LLY());
    ury = std::min(URY(), blk.URY());
    overlap_area = (urx - llx) * (ury - lly);
  }
  return overlap_area;
}

void Block::SetStretchLength(size_t index, int length) {
  size_t sz = stretch_length_.size();
  DaliExpects(index < sz, "Out of bound");
  if (IsFlipped()) {
    index = sz - 1 - index;
  }
  stretch_length_[index] = length;
}

std::vector<int> &Block::StretchLengths() {
  return stretch_length_;
}

int Block::CumulativeStretchLength(size_t index) {
  if (TypePtr()->MultiWellPtr()->RowCount() == 1) return 0;

  size_t sz = stretch_length_.size();
  DaliExpects(index <= sz, "Out of bound");

  int res = 0;
  for (size_t i = 0; i < index; ++i) {
    res += stretch_length_[i];
  }
  return res;
}

void Block::Report() {
  BOOST_LOG_TRIVIAL(info)
    << "  block name: " << Name() << "\n"
    << "    block type: " << TypePtr()->Name() << "\n"
    << "    width and height: " << Width() << " " << Height() << "\n"
    << "    lower left corner: " << llx_ << " " << lly_ << "\n"
    << "    movable: " << IsMovable() << "\n"
    << "    orientation: " << OrientStr(orient_) << "\n"
    << "    assigned primary key: " << Id() << "\n";
}

void Block::ReportNet() {
  BOOST_LOG_TRIVIAL(info) << Name() << " connects to:\n";
  for (auto &net_num: net_list_) {
    BOOST_LOG_TRIVIAL(info) << net_num << "  ";
  }
  BOOST_LOG_TRIVIAL(info) << "\n";
}

void Block::ExportWellToMatlabPatchRect(std::ofstream &ost) {
  std::vector<RectI> n_well_shapes;
  std::vector<RectI> p_well_shapes;
  BlockTypeWell *well = TypePtr()->WellPtr();
  if (well != nullptr) {
    n_well_shapes.push_back(well->NwellRect());
    p_well_shapes.push_back(well->PwellRect());
  }
  BlockTypeMultiWell *multi_well = TypePtr()->MultiWellPtr();
  if (multi_well != nullptr) {
    for (size_t i = 0; i < multi_well->RowCount(); ++i) {
      n_well_shapes.push_back(multi_well->NwellRect(i));
      p_well_shapes.push_back(multi_well->PwellRect(i));
    }
  }

  size_t sz = n_well_shapes.size();
  for (size_t i = 0; i < sz; ++i) {
    int length = CumulativeStretchLength(i);
    if (Orient() == N) {
      RectD n_rect(
          LLX() + n_well_shapes[i].LLX(),
          LLY() + (n_well_shapes[i].LLY() + length),
          LLX() + n_well_shapes[i].URX(),
          LLY() + (n_well_shapes[i].URY() + length)
      );
      RectD p_rect(
          LLX() + p_well_shapes[i].LLX(),
          LLY() + (p_well_shapes[i].LLY() + length),
          LLX() + p_well_shapes[i].URX(),
          LLY() + (p_well_shapes[i].URY() + length)
      );
      SaveMatlabPatchRegion(ost, n_rect, p_rect);
    } else if (Orient() == FS) {
      RectD n_rect(
          LLX() + n_well_shapes[i].LLX(),
          URY() - (n_well_shapes[i].URY() + length),
          LLX() + n_well_shapes[i].URX(),
          URY() - (n_well_shapes[i].LLY() + length)
      );
      RectD p_rect(
          LLX() + p_well_shapes[i].LLX(),
          URY() - (p_well_shapes[i].URY() + length),
          LLX() + p_well_shapes[i].URX(),
          URY() - (p_well_shapes[i].LLY() + length)
      );
      SaveMatlabPatchRegion(ost, n_rect, p_rect);
    } else {
      BOOST_LOG_TRIVIAL(debug)
        << "Orientation not supported "
        << __FILE__ << " : " << __LINE__ << " : " << __FUNCTION__ << "\n";
    }
  }
}

}
