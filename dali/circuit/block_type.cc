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
#include "block_type.h"

namespace dali {

BlockType::BlockType(
    std::string const *name_ptr
) : name_ptr_(name_ptr) {}

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
      std::unordered_map<std::string, int>::value_type(
          pin_name,
          static_cast<int>(pin_list_.size())
      )
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
      std::unordered_map<std::string, int>::value_type(
          pin_name,
          static_cast<int>(pin_list_.size())
      )
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

void BlockType::SetWidth(int width) {
  width_ = width;
  UpdateArea();
}

void BlockType::SetHeight(int height) {
  height_ = height;
  UpdateArea();
}

void BlockType::SetSize(int width, int height) {
  width_ = width;
  height_ = height;
  UpdateArea();
}

void BlockType::Report() const {
  BOOST_LOG_TRIVIAL(info)
    << "  BlockType name: " << Name() << "\n"
    << "    width, height: " << Width() << " " << Height() << "\n"
    << "    pin list:\n";
  for (const auto &[name, id] : pin_name_id_map_) {
    BOOST_LOG_TRIVIAL(info)
      << "      " << name << " " << id
      << " (" << pin_list_[id].OffsetX() << ", " << pin_list_[id].OffsetY()
      << ")\n";
    pin_list_[id].Report();
  }
}

void BlockType::UpdateArea() {
  area_ = static_cast<long long>(width_) * static_cast<long long>(height_);
}

void BlockType::AddNwellRect(int llx, int lly, int urx, int ury) {
  has_well_info_ = true;
  n_rects_.emplace_back(llx, lly, urx, ury);
  region_count_ = static_cast<int>(std::max(n_rects_.size(), p_rects_.size()));
}

void BlockType::AddPwellRect(int llx, int lly, int urx, int ury) {
  has_well_info_ = true;
  p_rects_.emplace_back(llx, lly, urx, ury);
  region_count_ = static_cast<int>(std::max(n_rects_.size(), p_rects_.size()));
}

void BlockType::AddWellRect(
    bool is_n, int llx, int lly, int urx, int ury
) {
  if (is_n) {
    AddNwellRect(llx, lly, urx, ury);
  } else {
    AddPwellRect(llx, lly, urx, ury);
  }
}

void BlockType::SetExtraBottomExtension(int bot_extension) {
  extra_bot_extension_ = bot_extension;
}

void BlockType::SetExtraTopExtension(int top_extension) {
  extra_top_extension_ = top_extension;
}

bool BlockType::IsNwellAbovePwell(int region_id) const {
  DaliExpects(region_id < region_count_, "Index out of bound");
  return p_rects_[region_id].LLY() <= n_rects_[region_id].LLY();
}

int BlockType::RegionCount() const {
  return region_count_;
}

bool BlockType::HasOddRegions() const {
  return region_count_ & 1;
}

bool BlockType::IsWellAbutted() {
  int row_count = RegionCount();
  std::vector<int> y_edges;
  bool is_well_p = IsNwellAbovePwell(0);
  for (int i = 0; i < row_count; ++i) {
    if (is_well_p) {
      y_edges.push_back(p_rects_[i].LLY());
      y_edges.push_back(p_rects_[i].URY());
      y_edges.push_back(n_rects_[i].LLY());
      y_edges.push_back(n_rects_[i].URY());
    } else {
      y_edges.push_back(n_rects_[i].LLY());
      y_edges.push_back(n_rects_[i].URY());
      y_edges.push_back(p_rects_[i].LLY());
      y_edges.push_back(p_rects_[i].URY());
    }
    is_well_p = !is_well_p;
  }

  for (int i = 1; i < 2 * row_count - 1; i += 2) {
    if (y_edges[i] != y_edges[i + 1]) {
      return false;
    }
  }
  return true;
}

bool BlockType::IsCellHeightConsistent() {
  int cell_height = std::max(n_rects_.back().URY(), p_rects_.back().URY());
  int lef_height = Height();
  return cell_height == lef_height;
}

void BlockType::CheckLegality() {
  DaliExpects(n_rects_.size() == p_rects_.size(),
              "Nwell count is different from Pwell count " + Name());
  DaliExpects(IsWellAbutted(),
              "Wells are not abutted for cell " + Name());
  DaliExpects(IsCellHeightConsistent(),
              "Macro/well height inconsistency" + Name());
}

int BlockType::NwellHeight(int region_id, bool is_flipped) const {
  DaliExpects(region_id < region_count_, "Index out of bound");
  if (is_flipped) {
    region_id = region_count_ - 1 - region_id;
  }
  return n_rects_[region_id].Height();
}

int BlockType::PwellHeight(int region_id, bool is_flipped) const {
  DaliExpects(region_id < region_count_, "Index out of bound");
  if (is_flipped) {
    region_id = region_count_ - 1 - region_id;
  }
  return p_rects_[region_id].Height();
}

int BlockType::RegionHeight(int region_id, bool is_flipped) const {
  DaliExpects(region_id < region_count_, "Index out of bound");
  if (is_flipped) {
    region_id = region_count_ - 1 - region_id;
  }
  return p_rects_[region_id].Height() + n_rects_[region_id].Height();
}

/****
 * Distance of NP-edge between Region index and Region index+1
 * @param index
 * @param is_flipped
 * @return
 */
int BlockType::AdjacentRegionEdgeDistance(
    int index, bool is_flipped
) const {
  int row_cnt = RegionCount();
  DaliExpects(index + 1 < row_cnt, "Out of bound");
  if (is_flipped) {
    index = row_cnt - 2 - index;
  }
  if (n_rects_[index].LLY() > p_rects_[index].LLY()) {
    return n_rects_[index].Height() + n_rects_[index + 1].Height();
  } else {
    return p_rects_[index].Height() + p_rects_[index + 1].Height();
  }
}

RectI &BlockType::NwellRect(int index) {
  DaliExpects(index < RegionCount(), "Out of bound");
  return n_rects_[index];
}

RectI &BlockType::PwellRect(int index) {
  DaliExpects(index < RegionCount(), "Out of bound");
  return p_rects_[index];
}

void BlockType::ReportWellInfo() const {
  BOOST_LOG_TRIVIAL(info)
    << "  Well of BlockType: " << Name() << "\n";
  size_t sz = RegionCount();
  for (size_t i = 0; i < sz; ++i) {
    BOOST_LOG_TRIVIAL(info)
      << "    Pwell: " << p_rects_[i].LLX() << "  " << p_rects_[i].LLY()
      << "  " << p_rects_[i].URX() << "  " << p_rects_[i].URY() << "\n";
    BOOST_LOG_TRIVIAL(info)
      << "    Nwell: " << n_rects_[i].LLX() << "  " << n_rects_[i].LLY()
      << "  " << n_rects_[i].URX() << "  " << n_rects_[i].URY() << "\n";
  }
}

int BlockType::Pheight() {
  if (!p_rects_.empty()) {
    return p_rects_[0].URY();
  } else if (!n_rects_.empty()) {
    return n_rects_[0].LLY();
  }
  DaliExpects(false, "No rects found in well?");
}

int BlockType::Nheight() {
  if (!p_rects_.empty()) {
    return Height() - p_rects_[0].URY();
  } else if (!n_rects_.empty()) {
    return Height() - n_rects_[0].LLY();
  }
  DaliExpects(false, "No rects found in well?");
}

}
