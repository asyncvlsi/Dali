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
#ifndef DALI_DALI_CIRCUIT_BLOCKTYPE_H_
#define DALI_DALI_CIRCUIT_BLOCKTYPE_H_

#include <map>
#include <vector>

#include "dali/common/logging.h"
#include "dali/common/misc.h"
#include "pin.h"

/****
 * This header file contains class BlockType and BlockTypeWell. The former contains
 * some basic physical information of a kind of gate, and the later contains
 * N/P-well physical information.
 * ****/

namespace dali {

class BlockTypeWell;

/****
 * The class BlockType is an abstraction of a kind of gate, like INV, NAND, NOR
 * Here are the attributes of BlockType:
 *     name: the name of this kind of gate
 *     width: the width of its placement and routing boundary
 *     height: the height of its placement and routing boundary
 *     well: this is for gridded cell placement, N/P-well shapes are needed for alignment in a local cluster
 *     pin: a list of cell pins with shapes and offsets
 */
class BlockType {
 public:
  BlockType(const std::string *name_ptr, int width, int height);

  const std::string &Name() const { return *name_ptr_; }

  // check if a pin with a given name exists in this BlockType or not
  bool IsPinExisting(std::string const &pin_name) const {
    return pin_name_id_map_.find(pin_name) != pin_name_id_map_.end();
  }

  // return the index of a pin with a given name
  int GetPinId(std::string const &pin_name) const;

  // return a pointer to a newly allocated location for a Pin with a given name
  // if this member function is used to create pins, one needs to set pin shapes using the return pointer
  Pin *AddPin(std::string const &pin_name, bool is_input);

  // add a pin with a given name and x/y offset
  void AddPin(std::string const &pin_name, double x_offset, double y_offset);

  // get the pointer to the pin with the given name
  // if such a pin does not exist, the return value is nullptr
  Pin *GetPinPtr(std::string const &pin_name);

  // set the N/P-well information for this BlockType
  void SetWell(BlockTypeWell *well_ptr);

  // get the pointer to the well of this BlockType
  BlockTypeWell *WellPtr() const { return well_ptr_; }

  // set the width of this BlockType and update its area
  void SetWidth(int width);

  // get the width of this BlockType
  int Width() const { return width_; }

  // set the height of this BlockType and update its area
  void SetHeight(int height);

  void SetSize(int width, int height);

  // get the height of this BlockType
  int Height() const { return height_; }

  // get the area of this BlockType
  long int Area() const { return area_; }

  // get the pointer to the list of cell pins
  std::vector<Pin> &PinList() { return pin_list_; }

  // report the information of this BlockType for debugging purposes
  void Report() const;

 private:
  const std::string *name_ptr_;
  int width_, height_;
  long int area_;
  BlockTypeWell *well_ptr_ = nullptr;
  std::vector<Pin> pin_list_;
  std::map<std::string, int> pin_name_id_map_;
};

/****
 * This struct BlockTypeWell provides the N/P-well geometries for a BlockType.
 * Assumptions:
 *  1. BlockType has 1 or 2 well regions, each of which contains at least one
 *     N-well or P-well rectangle.
 *  2. If both N-well and P-well in the same region are present, they must be
 *     abutted. This is for debugging purposes, also for compact physical layout.
 *  3. N-well rectangles in both regions must be present, and they must be abutted.
 *     Otherwise, the well legalizer does not know how to stretch this kind of cell.
 *     If a technology node has no N-well, create a fake N-well.
 *     If the actual N-well rectangles in both regions are not abutted, make them abutted.
 *     +-----------------+
 *     |                 |
 *     |                 |
 *     |    P-well       |
 *     |                 |
 *     |                 |
 *     +-----------------+  n_p_edge of Region1
 *     |                 |
 *     |                 |
 *     |    N-well       |
 *     |                 |
 *     |                 |
 *     +-----------------+  stretch mark
 *     |                 |
 *     |                 |
 *     |    N-well       |
 *     |                 |
 *     |                 |
 *     +-----------------+  n_p_edge of Region0
 *     |                 |
 *     |                 |
 *     |    P-well       |
 *     |                 |
 *     |                 |
 *     +-----------------+
 * ****/
class BlockTypeWell {
 public:
  explicit BlockTypeWell(BlockType *type_ptr) : type_ptr_(type_ptr) {}

  // get the pointer to the BlockType this well belongs to
  BlockType *BlkTypePtr() const { return type_ptr_; }

  // set the rect of N-well
  void SetNwellRect(int lx, int ly, int ux, int uy);

  // set the rect of N-well 1
  void SetNwellRect1(int lx, int ly, int ux, int uy);

  // get the rect of N-well
  const RectI &NwellRect() { return n_rect_; }

  // get the rect of N-well 1
  const RectI &NwellRect1() { return n_rect1_; }

  // set the rect of P-well
  void SetPwellRect(int lx, int ly, int ux, int uy);

  // set the rect of P-well 1
  void SetPwellRect1(int lx, int ly, int ux, int uy);

  // get the rect of P-well
  const RectI &PwellRect() { return p_rect_; }

  // get the rect of P-well
  const RectI &PwellRect1() { return p_rect1_; }

  // get the P/N well boundary
  int PnBoundary() const { return p_n_edge_; }

  // get the height of N-well
  int Nheight() const { return stretch_edge_ - p_n_edge_; }

  // get the height of P-well
  int Pheight() const { return p_n_edge_; }

  // get the P/N well boundary
  int PnBoundary1() const { return p_n_edge1_; }

  // get the height of N-well
  int Nheight1() const { return n_rect1_.Height(); }

  // get the height of P-well
  int Pheight1() const { return type_ptr_->Height() - n_rect1_.URY(); }

  // set the rect of N or P well
  void SetWellRect(bool is_n, int lx, int ly, int ux, int uy);

  // set the rect of N or P well
  void SetWellRect(bool is_first, bool is_n, int lx, int ly, int ux, int uy);

  // check if N-well is abutted with P-well, if both exist
  bool IsNpWellAbutted() const;

  // check if this cell has a double well or not
  bool IsSingleWell() const;

  // check if the N/P-well shapes are legal or not
  void CheckLegal() const;

  // report the information of N/P-well for debugging purposes
  void Report() const;

 private:
  BlockType *type_ptr_; // pointer to BlockType
  bool is_n_set_ = false; // whether N-well 0 shape is set or not
  bool is_p_set_ = false; // whether P-well 0 shape is set or not
  RectI n_rect_; // N-well rect 0
  RectI p_rect_; // P-well rect 0
  int p_n_edge_ = 0; // cached N/P-well boundary 0

  int stretch_edge_ = 0;

  bool is_n_set1_ = false; // whether N-well 1 shape is set or not
  bool is_p_set1_ = false; // whether P-well 1 shape is set or not
  RectI n_rect1_; // N-well rect 1
  RectI p_rect1_; // P-well rect 1
  int p_n_edge1_ = 0; // cached N/P-well boundary 1
};

}

#endif //DALI_DALI_CIRCUIT_BLOCKTYPE_H_
