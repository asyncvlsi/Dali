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
#include "blocktype.h"

namespace dali {

BlockType::BlockType(
    const std::string *name_ptr,
    int width,
    int height
) : name_ptr_(name_ptr),
    width_(width),
    height_(height),
    area_((long int) width_ * (long int) height_),
    well_ptr_(nullptr) {}

BlockType::~BlockType() {
  delete m_well_ptr_;
}

int BlockType::GetPinId(std::string const &pin_name) const {
  auto ret = pin_name_id_map_.find(pin_name);
  if (ret != pin_name_id_map_.end()) {
    return ret->second;
  }
  return -1;
}

Pin *BlockType::AddPin(
    std::string const &pin_name,
    bool is_input
) {
  auto ret = pin_name_id_map_.find(pin_name);
  if (ret != pin_name_id_map_.end()) {
    DaliExpects(
        false,
        "Cannot add this pin in BlockType: " + Name()
            + ", because this pin exists in blk_pin_list already: "
            + pin_name
    );
  }
  pin_name_id_map_.insert(
      std::pair<std::string, int>(pin_name, pin_list_.size())
  );
  std::pair<const std::string, int> *name_num_ptr =
      &(*pin_name_id_map_.find(pin_name));
  pin_list_.emplace_back(name_num_ptr, this);
  pin_list_.back().SetIoType(is_input);
  return &pin_list_.back();
}

void BlockType::AddPin(
    std::string const &pin_name,
    double x_offset,
    double y_offset
) {
  auto ret = pin_name_id_map_.find(pin_name);
  if (ret != pin_name_id_map_.end()) {
    DaliExpects(
        false,
        "Cannot add this pin in BlockType: " + Name()
            + ", because this pin exists in blk_pin_list already: "
            + pin_name
    );
  }
  pin_name_id_map_.insert(
      std::pair<std::string, int>(pin_name, pin_list_.size())
  );
  std::pair<const std::string, int> *name_num_ptr =
      &(*pin_name_id_map_.find(pin_name));
  pin_list_.emplace_back(name_num_ptr, this, x_offset, y_offset);
}

Pin *BlockType::GetPinPtr(std::string const &pin_name) {
  auto res = pin_name_id_map_.find(pin_name);
  if (res != pin_name_id_map_.end()) {
    return &(pin_list_[res->second]);
  }
  return nullptr;
}

void BlockType::SetWell(BlockTypeWell *well_ptr) {
  DaliExpects(well_ptr != nullptr, "well_ptr is a nullptr?");
  well_ptr_ = well_ptr;
}

BlockTypeWell *BlockType::WellPtr() const {
  return well_ptr_;
}

void BlockType::SetMultiWell(BlockTypeMultiWell *m_well_ptr) {
  DaliExpects(m_well_ptr != nullptr, "m_well_ptr is a nullptr?");
  m_well_ptr_ = m_well_ptr;
}

BlockTypeMultiWell *BlockType::MultiWellPtr() const {
  return m_well_ptr_;
}

void BlockType::SetWidth(int width) {
  width_ = width;
  area_ = (long int) width_ * (long int) height_;
}

void BlockType::SetHeight(int height) {
  height_ = height;
  area_ = (long int) width_ * (long int) height_;
}

void BlockType::SetSize(int width, int height) {
  width_ = width;
  height_ = height;
  area_ = (long int) width_ * (long int) height_;
}

void BlockType::Report() const {
  BOOST_LOG_TRIVIAL(info)
    << "  BlockType name: " << Name() << "\n"
    << "    width, height: " << Width() << " "
    << Height() << "\n"
    << "    pin list:\n";
  for (auto &it: pin_name_id_map_) {
    BOOST_LOG_TRIVIAL(info)
      << "      " << it.first << " " << it.second << " ("
      << pin_list_[it.second].OffsetX() << ", "
      << pin_list_[it.second].OffsetY() << ")\n";
    pin_list_[it.second].Report();
  }
}

void BlockTypeWell::SetNwellRect(int lx, int ly, int ux, int uy) {
  is_n_set_ = true;
  n_rect_.SetValue(lx, ly, ux, uy);
  if (is_p_set_) {
    DaliExpects(n_rect_.LLY() == p_rect_.URY(), "N/P-well not abutted");
  } else {
    p_n_edge_ = n_rect_.LLY();
  }
}

const RectI &BlockTypeWell::NwellRect() {
  return n_rect_;
}

void BlockTypeWell::SetPwellRect(int lx, int ly, int ux, int uy) {
  is_p_set_ = true;
  p_rect_.SetValue(lx, ly, ux, uy);
  if (is_n_set_) {
    DaliExpects(n_rect_.LLY() == p_rect_.URY(), "N/P-well not abutted");
  } else {
    p_n_edge_ = p_rect_.URY();
  }
}

const RectI &BlockTypeWell::PwellRect() {
  return p_rect_;
}

void BlockTypeWell::SetWellRect(bool is_n, int lx, int ly, int ux, int uy) {
  if (is_n) {
    SetNwellRect(lx, ly, ux, uy);
  } else {
    SetPwellRect(lx, ly, ux, uy);
  }
}

bool BlockTypeWell::IsNpWellAbutted() const {
  if (is_p_set_ && is_n_set_) {
    return p_rect_.URY() == n_rect_.LLY();
  }
  return true;
}

void BlockTypeWell::Report() const {
  BOOST_LOG_TRIVIAL(info)
    << "  Well of BlockType: " << type_ptr_->Name() << "\n";
  if (is_n_set_) {
    BOOST_LOG_TRIVIAL(info)
      << "    Nwell: " << n_rect_.LLX() << "  " << n_rect_.LLY() << "  "
      << n_rect_.URX() << "  " << n_rect_.URY() << "\n";
  }
  if (is_p_set_) {
    BOOST_LOG_TRIVIAL(info)
      << "    Pwell: " << p_rect_.LLX() << "  " << p_rect_.LLY() << "  "
      << p_rect_.URX() << "  " << p_rect_.URY() << "\n";
  }
}

void BlockTypeMultiWell::AddNwellRect(int llx, int lly, int urx, int ury) {
  n_rects_.emplace_back(llx, lly, urx, ury);
}

void BlockTypeMultiWell::AddPwellRect(int llx, int lly, int urx, int ury) {
  p_rects_.emplace_back(llx, lly, urx, ury);
}

void BlockTypeMultiWell::AddWellRect(
    bool is_n, int llx, int lly, int urx, int ury
) {
  if (is_n) {
    AddNwellRect(llx, lly, urx, ury);
  } else {
    AddPwellRect(llx, lly, urx, ury);
  }
}

void BlockTypeMultiWell::SetExtraBottomExtension(int bot_extension) {
  extra_bot_extension_ = bot_extension;
}

void BlockTypeMultiWell::SetExtraTopExtension(int top_extension) {
  extra_top_extension_ = top_extension;
}

bool BlockTypeMultiWell::IsBottomWellN() const {
  DaliExpects(!n_rects_.empty() && !p_rects_.empty(), "No wells in cell?");
  return n_rects_[0].LLY() <= p_rects_[0].LLY();
}

int BlockTypeMultiWell::RowCount() const {
  return static_cast<int>(n_rects_.size());
}

bool BlockTypeMultiWell::CheckWellAbutment() const {
  int row_count = RowCount();
  bool is_bottom_well_N = IsBottomWellN();

  if (is_bottom_well_N) {
    for (int i = 0; i < row_count; ++i) {
      bool is_in_row_abutted = n_rects_[i].URY() == p_rects_[i].LLY();
      if (!is_in_row_abutted) return false;
      bool is_between_row_abutted = true;
      if (i > 0) {
        is_between_row_abutted = p_rects_[i - 1].URY() == n_rects_[i].LLY();
      }
      if (!is_between_row_abutted) return false;
    }
  }

  return true;
}

int BlockTypeMultiWell::NwellHeight(int index) const {
  DaliExpects(index < RowCount(), "Out of bound");
  return n_rects_[index].Height();
}

int BlockTypeMultiWell::PwellHeight(int index) const {
  DaliExpects(index < RowCount(), "Out of bound");
  return p_rects_[index].Height();
}

void BlockTypeMultiWell::Report() const {
  BOOST_LOG_TRIVIAL(info)
    << "  Well of BlockType: " << type_ptr_->Name() << "\n";
  int sz = RowCount();
  for (int i = 0; i < sz; ++i) {
    BOOST_LOG_TRIVIAL(info)
      << "    Pwell: " << p_rects_[i].LLX() << "  " << p_rects_[i].LLY() << "  "
      << p_rects_[i].URX() << "  " << p_rects_[i].URY() << "\n";
    BOOST_LOG_TRIVIAL(info)
      << "    Nwell: " << n_rects_[i].LLX() << "  " << n_rects_[i].LLY() << "  "
      << n_rects_[i].URX() << "  " << n_rects_[i].URY() << "\n";
  }
}

}
