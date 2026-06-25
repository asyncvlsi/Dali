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

#ifndef DALI_CIRCUIT_LAYER_H_
#define DALI_CIRCUIT_LAYER_H_

#include <string>

#include "dali/common/logging.h"
#include "dali/common/misc.h"
#include "enums.h"

namespace dali {

/** Shared minimum-width and minimum-spacing data for technology layers. */
class Layer {
 public:
  Layer();
  Layer(double width, double spacing);

  /** Set minimum layer width. */
  void SetWidth(double width);

  /** Return minimum layer width. */
  double Width() const { return width_; }

  /** Set same-layer minimum spacing. */
  void SetSpacing(double spacing);

  /** Return same-layer minimum spacing. */
  double Spacing() const { return spacing_; }

 protected:
  double width_;
  double spacing_;
};

/** Routing metal layer parameters used mainly by I/O placement. */
class MetalLayer : public Layer {
 public:
  explicit MetalLayer(std::pair<const std::string, int>* name_id_pair_ptr);
  MetalLayer(double width, double spacing,
             std::pair<const std::string, int>* name_id_pair_ptr,
             MetalDirection direction = HORIZONTAL);

  /** Return the metal layer name. */
  const std::string& Name() const;

  /** Return the metal layer id. */
  int Id() const;

  /** Set minimum metal area. */
  void SetMinArea(double min_area);

  /** Return minimum metal area. */
  double MinArea() const;

  /** Return minimum height implied by minimum area and width. */
  double MinHeight() const;

  // set the pitch of this layer, this can be different from min_width +
  // min_spacing
  void SetPitch(double x_pitch, double y_pitch);

  // automatically compute pitches from min_width and min_spacing
  void SetPitchUsingWidthSpacing();

  // get the pitch along x direction
  double PitchX() const;

  // get the pitch along y direction
  double PitchY() const;

  // set direction of this metal layer
  void SetDirection(MetalDirection direction);

  // get direction of this metal layer
  MetalDirection Direction() const;

  // get the string for the direction of this layer
  std::string DirectionStr() const;

  // print information of this layer
  void Report() const;

 private:
  std::pair<const std::string, int>* name_id_pair_ptr_;
  double min_area_;
  double x_pitch_;
  double y_pitch_;
  MetalDirection direction_;
};

/** N/P-well layer parameters used by well legalization. */
class WellLayer : public Layer {
 public:
  WellLayer();
  WellLayer(double width, double spacing, double opposite_spacing,
            double max_plug_dist, double overhang);

  // set all parameters
  void SetParams(double width, double spacing, double opposite_spacing,
                 double max_plug_dist, double overhang);

  // set opposite spacing
  void SetOppositeSpacing(double opposite_spacing);

  // get opposite spacing
  double OppositeSpacing() const;

  // set max plug distance
  void SetMaxPlugDist(double max_plug_dist);

  // get max plug distance
  double MaxPlugDist() const;

  // set overhang
  void SetOverhang(double overhang);

  // get overhang
  double Overhang() const;

  // print information of this layer
  void Report() const;

 private:
  double opposite_spacing_;
  double max_plug_dist_;
  double overhang_;
};

}  // namespace dali

#endif  // DALI_CIRCUIT_LAYER_H_
