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
#ifndef DALI_PLACER_IOPLACER_IOBOUNDARYSPACE_H_
#define DALI_PLACER_IOPLACER_IOBOUNDARYSPACE_H_

#include <string>
#include <vector>

#include "dali/circuit/iopin.h"

namespace dali {

/**
 * A structure for storing IOPINs on a boundary segment for a given metal layer.
 */
struct IoPinCluster {
  IoPinCluster(bool is_horizontal_init,
               double boundary_loc_init,
               double lo_init,
               double span_init);
  ~IoPinCluster();

  bool is_horizontal; // is this cluster on a horizontal or vertical boundary
  double boundary_loc; // the location of the boundary this cluster is on
  double low; // the low position of this cluster
  double span; // the width or height of this cluster
  bool is_uniform_mode =
      true; // uniformly distribute IOPINs in this cluster or not
  std::vector<IoPin *> iopin_ptr_list; // IOPINs  in this cluster

  double Low() const;
  double High() const;
  void UniformLegalize();
  void GreedyLegalize();
  void Legalize();
};

/**
 * A structure for storing IOPINs on a boundary for a given metal layer.
 * If there is no blockage on the boundary, then it only contains one IoPinCluster.
 */
struct IoBoundaryLayerSpace {
  IoBoundaryLayerSpace(
      bool is_horizontal_init,
      double boundary_loc_init,
      MetalLayer *metal_layer_init
  );
  ~IoBoundaryLayerSpace();
  bool is_horizontal;
  double boundary_loc;
  MetalLayer *metal_layer;
  RectD default_horizontal_shape; // unit in micron
  RectD default_vertical_shape; // unit in micron
  bool is_using_horizontal = true;
  std::vector<IoPin *> iopin_ptr_list;
  std::vector<IoPinCluster> pin_clusters;

  void AddCluster(double low, double span);

  void ComputeDefaultShape(double manufacturing_grid);
  void UpdateIoPinShapeAndLayer();
  void UniformAssignIoPinToCluster();
  void GreedyAssignIoPinToCluster();
  void AssignIoPinToCluster();
};

/**
 * A structure for storing IOPINs on a boundary for all possible metal layer.
 */
struct IoBoundarySpace {
  friend class IoPlacer;
 public:
  std::vector<IoBoundaryLayerSpace> layer_spaces_;

  IoBoundarySpace(bool is_horizontal, double boundary_loc);
  void AddLayer(MetalLayer *metal_layer);
  void SetIoPinLimit(int32_t limit);
  bool AutoPlaceIoPin();
 private:
  int32_t iopin_limit_ = 0;
  bool is_iopin_limit_set_ = false;
  bool is_horizontal_;
  double boundary_loc_ = 0;
  double manufacturing_grid_;
};

}

#endif //DALI_PLACER_IOPLACER_IOBOUNDARYSPACE_H_
