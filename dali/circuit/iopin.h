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
#ifndef DALI_CIRCUIT_IOPIN_H_
#define DALI_CIRCUIT_IOPIN_H_

#include <string>

#include "dali/common/logging.h"
#include "dali/common/misc.h"
#include "enums.h"
#include "layer.h"
#include "net.h"

namespace dali {

class Net;

/** DEF I/O pin with net, signal metadata, placement status, and geometry. */
class IoPin {
 public:
  explicit IoPin(std::pair<const std::string, int>* name_id_pair_ptr);
  IoPin(std::pair<const std::string, int>* name_id_pair_ptr, double loc_x,
        double loc_y);
  IoPin(std::pair<const std::string, int>* name_id_pair_ptr,
        SignalDirection direction, PlaceStatus init_place_status, double loc_x,
        double loc_y);
  IoPin(double loc_x, double loc_y, BlockOrient orient, double llx, double lly,
        double urx, double ury);

  /** Return the I/O pin name. */
  const std::string& Name() const;

  /** Return the I/O pin id. */
  int Id() const;

  /** Attach the net this I/O pin belongs to. */
  void SetNetPtr(Net* net_ptr);

  /** Return the net this I/O pin belongs to. */
  Net* NetPtr() const;

  /** Return the connected net name. */
  const std::string& NetName() const;

  /** Set DEF signal direction. */
  void SetSigDirection(SignalDirection direction);

  /** Return DEF signal direction. */
  SignalDirection SigDirection() const;

  /** Return DEF signal direction as a string. */
  std::string SigDirectStr() const;

  /** Set DEF signal use. */
  void SetSigUse(SignalUse use);

  /** Return DEF signal use. */
  SignalUse SigUse() const;

  /** Return DEF signal use as a string. */
  std::string SigUseStr() const;

  /** Attach the metal layer used by this pin shape. */
  void SetLayerPtr(MetalLayer* layer_ptr);

  /** Return the metal layer used by this pin shape. */
  MetalLayer* LayerPtr() const;

  /** Return the metal layer name. */
  const std::string& LayerName() const;

  /** Set pin shape in local coordinates and cache oriented variants. */
  void SetShape(double llx, double lly, double urx, double ury);

  /** Return the N-orientation pin shape. */
  RectD& GetShape();

  /** Return true when pin geometry is available. */
  bool IsShapeSet() const;

  /** Set placement status before Dali modifies this pin. */
  void SetInitPlaceStatus(PlaceStatus init_place_status);

  /** Return true when the pin was already placed before Dali. */
  bool IsPrePlaced() const;

  /** Set current placement status. */
  void SetPlaceStatus(PlaceStatus place_status);

  /** Return true when the pin currently has a placed/fixed/cover status. */
  bool IsPlaced() const;

  /** Return current placement status. */
  PlaceStatus GetPlaceStatus() const;

  /** Set x location in Dali grid units. */
  void SetLocX(double loc_x);

  /** Return x location, normally the center point on a placement boundary. */
  double X() const;

  /** Set y location in Dali grid units. */
  void SetLocY(double loc_y);

  /** Return y location, normally the center point on a placement boundary. */
  double Y() const;

  /** Set location and placement status together. */
  void SetLoc(double loc_x, double loc_y, PlaceStatus place_status = PLACED);

  /** Return lower x boundary with optional spacing expansion. */
  double LX(double spacing = 0) const;

  /** Return upper x boundary with optional spacing expansion. */
  double UX(double spacing = 0) const;

  /** Return lower y boundary with optional spacing expansion. */
  double LY(double spacing = 0) const;

  /** Return upper y boundary with optional spacing expansion. */
  double UY(double spacing = 0) const;

  /** Set final x coordinate in database units. */
  void SetFinalX(int final_x);

  /** Return final x coordinate in database units. */
  int FinalX() const;

  /** Set final y coordinate in database units. */
  void SetFinalY(int final_y);

  /** Return final y coordinate in database units. */
  int FinalY() const;

  /** Set final orientation. */
  void SetOrient(BlockOrient orient);

  /** Return final orientation. */
  BlockOrient GetOrient() const;

  /** Log I/O pin information for debugging. */
  void Report() const;

 private:
  std::pair<const std::string, int>* name_id_pair_ptr_;
  Net* net_ptr_;
  SignalDirection signal_direction_;
  SignalUse signal_use_;
  MetalLayer* layer_ptr_;
  std::vector<RectD> rects_;  // rectangles for all orientations
  bool is_shape_set_ = false;
  PlaceStatus init_place_status_;
  PlaceStatus place_status_;
  double x_;         // grid unit
  double y_;         // grid unit
  int final_x_ = 0;  // database unit
  int final_y_ = 0;  // database unit
  BlockOrient orient_;

  // set shape of its physical geometry, and compute rects for different
  // orientations
  void SetRect(double llx, double lly, double urx, double ury);
};

}  // namespace dali

#endif  // DALI_CIRCUIT_IOPIN_H_
