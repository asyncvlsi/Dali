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
#ifndef DALI_CIRCUIT_COMPONENT_H_
#define DALI_CIRCUIT_COMPONENT_H_

#include <iostream>
#include <map>
#include <numeric>
#include <string>
#include <vector>

#include "dali/common/logging.h"
#include "dali/common/misc.h"
#include "enums.h"
#include "macro.h"

namespace dali {

class ComponentAux;

/**
 * Physical instance in a design.
 *
 * A component can represent a standard cell, macro, filler, well tap, or
 * generated helper instance. Its location is stored as the lower-left corner of
 * the placement bounding rectangle, matching DEF COMPONENT placement semantics.
 */
class Component {
 public:
  explicit Component(std::string const* name_ptr) : name_ptr_(name_ptr) {}

  /** Return the component instance name. */
  const std::string& Name() const { return *name_ptr_; }

  /** Return the macro/master that defines this component's size and pins. */
  Macro* MacroPtr() const { return macro_ptr_; }

  /** Return the component's internal design id. */
  int Id() const { return id_; }

  /** Return the component width in Dali grid units. */
  int Width() const { return macro_ptr_->Width(); }

  /** Set effective height and update the cached effective area. */
  void SetHeight(int height);

  /** Reset effective height and area from the component's macro. */
  void ResetHeight();

  /** Return the effective height in Dali grid units. */
  int Height() const { return eff_height_; }

  /** Return lower-left x coordinate in Dali grid units. */
  double LLX() const { return llx_; }

  /** Return lower-left y coordinate in Dali grid units. */
  double LLY() const { return lly_; }

  /** Return upper-right x coordinate in Dali grid units. */
  double URX() const { return llx_ + Width(); }

  /** Return upper-right y coordinate, including any stretch length. */
  double URY() const { return lly_ + Height() + tot_stretch_length; }

  /** Return center x coordinate in Dali grid units. */
  double X() const { return llx_ + Width() / 2.0; }

  /** Return center y coordinate in Dali grid units. */
  double Y() const { return lly_ + Height() / 2.0; }

  /** Return the ids of nets connected to this component. */
  std::vector<int>& NetList() { return nets_; }

  /** Return true if this component has a placed, fixed, or cover status. */
  bool IsPlaced() const {
    return place_status_ == PLACED || place_status_ == FIXED ||
           place_status_ == COVER;
  }

  /** Return the placement status. */
  PlaceStatus Status() const { return place_status_; }

  /** Return the placement status as a DEF-style string. */
  std::string StatusStr() const { return PlaceStatusStr(place_status_); }

  /** Return true for statuses Dali treats as movable: UNPLACED or PLACED. */
  bool IsMovable() const {
    return place_status_ == UNPLACED || place_status_ == PLACED;
  }

  /** Return true when this component is fixed or cover. */
  bool IsFixed() const { return !IsMovable(); }

  /** Return cached effective area in grid-unit squared. */
  long long Area() const { return eff_area_; }

  /** Return the current component orientation. */
  ComponentOrient Orient() const { return orient_; }

  /** Return true when the orientation mirrors the component. */
  bool IsFlipped() const;

  /** Return optional auxiliary placement data attached by a later flow. */
  ComponentAux* AuxPtr() const { return aux_ptr_; }

  /** Set the internal design id. */
  void SetId(size_t id) { id_ = id; }

  /** Set the component macro and reset effective dimensions from it. */
  void SetMacro(Macro* macro_ptr);

  /** Set lower-left location in Dali grid units. */
  void SetLoc(double lx, double ly);

  /** Set lower-left x coordinate in Dali grid units. */
  void SetLLX(double lx) { llx_ = lx; }

  /** Set lower-left y coordinate in Dali grid units. */
  void SetLLY(double ly) { lly_ = ly; }

  /** Set upper-right x coordinate while preserving width. */
  void SetURX(double ux) { llx_ = ux - Width(); }

  /** Set upper-right y coordinate while preserving effective height. */
  void SetURY(double uy) { lly_ = uy - Height(); }

  /** Set center x coordinate while preserving width. */
  void SetCenterX(double center_x) { llx_ = center_x - Width() / 2.0; }

  /** Set center y coordinate while preserving effective height. */
  void SetCenterY(double center_y) { lly_ = center_y - Height() / 2.0; }

  /** Set the placement status. */
  void SetPlacementStatus(PlaceStatus place_status);

  /** Set the component orientation. */
  void SetOrient(ComponentOrient orient);

  /** Attach auxiliary placement data owned by a later flow. */
  void SetAux(ComponentAux* aux);

  /** Swap only the lower-left location with another component. */
  void SwapLoc(Component& component);

  /** Increase x coordinate by displacement. */
  void IncreaseX(double displacement) { llx_ += displacement; }

  /** Increase y coordinate by displacement. */
  void IncreaseY(double displacement) { lly_ += displacement; }

  /** Increase x coordinate by displacement and clamp within [lower, upper]. */
  void IncreaseX(double displacement, double upper, double lower);

  /** Increase y coordinate by displacement and clamp within [lower, upper]. */
  void IncreaseY(double displacement, double upper, double lower);

  /** Decrease x coordinate by displacement. */
  void DecreaseX(double displacement) { llx_ -= displacement; }

  /** Decrease y coordinate by displacement. */
  void DecreaseY(double displacement) { lly_ -= displacement; }

  /** Return true when this component overlaps another component. */
  bool IsOverlap(const Component& component) const {
    return !(LLX() > component.URX() || component.LLX() > URX() ||
             LLY() > component.URY() || component.LLY() > URY());
  }

  bool IsOverlap(const RectI& rect) const {
    return !(LLX() > rect.URX() || rect.LLX() > URX() || LLY() > rect.URY() ||
             rect.LLY() > URY());
  }

  /** Return true when this component overlaps another component pointer. */
  bool IsOverlap(const Component* component) const {
    return IsOverlap(*component);
  }

  /** Return the overlap area with another component. */
  double OverlapArea(const Component& component) const;

  /** Set stretch length for a region boundary. */
  void SetStretchLength(size_t index, int length);

  /** Return all stretch lengths. */
  std::vector<int>& StretchLengths();

  int CumulativeStretchLength(size_t index);

  /** Log detailed component information for debugging. */
  void Report();

  /** Log the nets connected to this component. */
  void ReportNet();

  /** Write this component's well geometry as MATLAB patch rectangles. */
  void ExportWellToMatlabPatchRect(std::ofstream& ost);

 protected:
  Macro* macro_ptr_ = nullptr;
  // Points to the canonical name owned by Design's component registry.
  std::string const* name_ptr_ = nullptr;
  int id_ = 0;
  double llx_ =
      0;  // lower x coordinate, data type double, for global placement
  double lly_ = 0;         // lower y coordinate
  std::vector<int> nets_;  // the list of nets connected to this component
  PlaceStatus place_status_ =
      UNPLACED;  // placement status, i.e, PLACED, FIXED, UNPLACED
  ComponentOrient orient_ = N;  // orientation, normally, N or FS
  ComponentAux* aux_ptr_ =
      nullptr;  // points to auxiliary information if needed

  // cached height, also used to store effective height, the unit is grid value
  // in the y-direction
  int eff_height_ = 0;
  long long eff_area_ = 0;  // cached effective area

  std::vector<int> stretch_length_;  // TODO : move these two attributes to
                                     // LegalizerComponentAux
  double tot_stretch_length = 0;
};

class ComponentAux {
 public:
  explicit ComponentAux(Component* component_ptr)
      : component_ptr_(component_ptr) {
    component_ptr->SetAux(this);
  }

  /** Return the component that owns this auxiliary data. */
  Component* GetComponentPtr() const { return component_ptr_; }

 protected:
  Component* component_ptr_;
};

struct ComponentInitialLocation {
  Component* component_ptr;
  double x;
  double y;
  explicit ComponentInitialLocation(Component* component_ptr_init = nullptr,
                                    double x_init = 0, double y_init = 0)
      : component_ptr(component_ptr_init), x(x_init), y(y_init) {}
};

}  // namespace dali

#endif  // DALI_CIRCUIT_COMPONENT_H_
