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

/****
 * This class contains basic information of an I/O pin. In this class, users can
 * find many attributes like:
 *     name: name of an I/O pin
 *     index: internal index
 *     net: the net this I/O pin belongs to
 * and many other things including I/O pin usage and physical geometry.
 */
class IoPin {
 public:
  explicit IoPin(std::pair<const std::string, int> *name_id_pair_ptr);
  IoPin(std::pair<const std::string, int> *name_id_pair_ptr, double loc_x,
        double loc_y);
  IoPin(std::pair<const std::string, int> *name_id_pair_ptr,
        SignalDirection direction, PlaceStatus init_place_status, double loc_x,
        double loc_y);
  IoPin(double loc_x, double loc_y, BlockOrient orient, double llx, double lly,
        double urx, double ury);

  // get the name
  const std::string &Name() const;

  // get the index
  int Id() const;

  // set net pointer
  void SetNetPtr(Net *net_ptr);

  // get the pointer to the net this IOPIN belongs to
  Net *NetPtr() const;

  // get the net name
  const std::string &NetName() const;

  // set signal direction
  void SetSigDirection(SignalDirection direction);

  // get signal direction
  SignalDirection SigDirection() const;

  // get the string corresponding to the signal direction
  std::string SigDirectStr() const;

  // set signal use
  void SetSigUse(SignalUse use);

  // get signal use
  SignalUse SigUse() const;

  // get the string corresponding to the signal use
  std::string SigUseStr() const;

  // set layer pointer
  void SetLayerPtr(MetalLayer *layer_ptr);

  // get the metal layer used to create its physical geometry
  MetalLayer *LayerPtr() const;

  // get the metal layer name
  const std::string &LayerName() const;

  // set shape of its physical geometry
  void SetShape(double llx, double lly, double urx, double ury);

  // get the geometry
  RectD &GetShape();

  // is the shape set?
  bool IsShapeSet() const;

  // set the placement status before dali
  void SetInitPlaceStatus(PlaceStatus init_place_status);

  // is this IOPIN placed before dali?
  bool IsPrePlaced() const;

  // set placement status
  void SetPlaceStatus(PlaceStatus place_status);

  // is this IOPIN placed by dali?
  bool IsPlaced() const;

  // get placement status
  PlaceStatus GetPlaceStatus() const;

  // set the x location
  void SetLocX(double loc_x);

  // get the x location, it is supposed to be the center on a placement boundary
  double X() const;

  // set the y location
  void SetLocY(double loc_y);

  // get the y location, it is supposed to be the center on a placement boundary
  double Y() const;

  void SetLoc(double loc_x, double loc_y, PlaceStatus place_status = PLACED);

  // lower left x
  double LX(double spacing = 0) const;

  // upper right x
  double UX(double spacing = 0) const;

  // lower left y
  double LY(double spacing = 0) const;

  // upper right y
  double UY(double spacing = 0) const;

  // set final x
  void SetFinalX(int final_x);

  // get final x
  int FinalX() const;

  // set final y
  void SetFinalY(int final_y);

  // get final y
  int FinalY() const;

  // set orientation
  void SetOrient(BlockOrient orient);

  // get orientation
  BlockOrient GetOrient() const;

  void Report() const;

 private:
  std::pair<const std::string, int> *name_id_pair_ptr_;
  Net *net_ptr_;
  SignalDirection signal_direction_;
  SignalUse signal_use_;
  MetalLayer *layer_ptr_;
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
