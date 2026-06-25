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
#ifndef DALI_CIRCUIT_BLOCKTYPE_H_
#define DALI_CIRCUIT_BLOCKTYPE_H_

#include <optional>
#include <unordered_map>
#include <vector>

#include "dali/circuit/pin.h"
#include "dali/common/logging.h"
#include "dali/common/misc.h"

namespace dali {

class BlockTypeWell;

/**
 * Physical master cell/macro used by block instances.
 *
 * A block type stores placement dimensions, pins, and optional N/P-well
 * rectangles for gridded-cell legalization. Well regions are expected to be
 * paired and abutted so downstream legalizers can infer stretch boundaries.
 */
class BlockType {
 public:
  explicit BlockType(std::string const* name_ptr);

  /** Return the block type name. */
  [[nodiscard]] const std::string& Name() const { return *name_ptr_; }

  /** Return true if this type has a pin named pin_name. */
  [[nodiscard]] bool IsPinExisting(std::string const& pin_name) const {
    return pin_name_id_map_.find(pin_name) != pin_name_id_map_.end();
  }

  /** Return the id of pin_name. Exits if the pin does not exist. */
  int GetPinId(std::string const& pin_name) const;

  /** Create a pin and return it so callers can fill geometry. */
  Pin* AddPin(std::string const& pin_name, bool is_input);

  /** Create a pin with a simple x/y offset. */
  void AddPin(std::string const& pin_name, double x_offset, double y_offset);

  /** Return the pin named pin_name, or nullptr if absent. */
  Pin* GetPinPtr(std::string const& pin_name);

  /** Set width in Dali grid units and update area. */
  void SetWidth(int width);

  /** Return width in Dali grid units. */
  int Width() const { return width_; }

  /** Set height in Dali grid units and update area. */
  void SetHeight(int height);

  /** Set width/height in Dali grid units and update area. */
  void SetSize(int width, int height);

  /** Return height in Dali grid units. */
  int Height() const { return height_; }

  /** Return area in grid-unit squared. */
  long long Area() const { return area_; }

  /** Return pins for this block type. */
  std::vector<Pin>& PinList() { return pin_list_; }

  /** Log block-type information for debugging. */
  void Report() const;

  /** Recompute cached area from width and height. */
  void UpdateArea();

  /** Return true when N/P-well rectangles are available. */
  bool HasWellInfo() const { return has_well_info_; }

  void AddNwellRect(int llx, int lly, int urx, int ury);

  void AddPwellRect(int llx, int lly, int urx, int ury);

  void AddWellRect(bool is_n, int llx, int lly, int urx, int ury);

  void SetExtraBottomExtension(int bot_extension);

  void SetExtraTopExtension(int top_extension);

  bool IsNwellAbovePwell(int region_id) const;

  int RegionCount() const;

  bool HasOddRegions() const;

  bool IsWellAbutted();

  bool IsCellHeightConsistent();

  void CheckLegality();

  int NwellHeight(int region_id, bool is_flipped = false) const;

  int PwellHeight(int region_id, bool is_flipped = false) const;

  int RegionHeight(int region_id, bool is_flipped = false) const;

  int AdjacentRegionEdgeDistance(int index, bool is_flipped = false) const;

  RectI& NwellRect(int index);

  RectI& PwellRect(int index);

  std::vector<RectI>& Nrects() { return n_rects_; }

  std::vector<RectI>& Prects() { return p_rects_; }

  void ReportWellInfo() const;

  /** Return first P-well height for legacy call sites. */
  int Pheight();

  /** Return first N-well height for legacy call sites. */
  int Nheight();

 private:
  std::string const* name_ptr_;
  int width_ = 0;
  int height_ = 0;
  long long area_ = 0;
  std::vector<Pin> pin_list_;
  std::unordered_map<std::string, int> pin_name_id_map_;

  bool has_well_info_ = false;
  std::vector<RectI> n_rects_;
  std::vector<RectI> p_rects_;
  int region_count_ = 0;
  int extra_bot_extension_ = 0;
  int extra_top_extension_ = 0;
};

}  // namespace dali

#endif  // DALI_CIRCUIT_BLOCKTYPE_H_
