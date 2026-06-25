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
#ifndef DALI_PLACER_IO_PLACER_IO_BOUNDARY_SPACE_H_
#define DALI_PLACER_IO_PLACER_IO_BOUNDARY_SPACE_H_

#include <string>
#include <vector>

#include "dali/circuit/io_pin.h"

namespace dali {

/** I/O pins assigned to one continuous boundary segment on one metal layer. */
struct IoPinCluster {
  IoPinCluster(bool is_horizontal_init, double boundary_loc_init,
               double lo_init, double span_init);
  ~IoPinCluster();

  bool is_horizontal;   // is this cluster on a horizontal or vertical boundary
  double boundary_loc;  // the location of the boundary this cluster is on
  double low;           // the low position of this cluster
  double span;          // the width or height of this cluster
  bool is_uniform_mode =
      true;  // uniformly distribute IOPINs in this cluster or not
  std::vector<IoPin*> iopin_ptr_list;  // IOPINs  in this cluster

  /** Return low coordinate of the cluster interval. */
  double Low() const;

  /** Return high coordinate of the cluster interval. */
  double High() const;

  /** Legalize pins using uniform spacing. */
  void UniformLegalize();

  /** Legalize pins greedily around their current locations. */
  void GreedyLegalize();

  /** Legalize pins using the configured cluster mode. */
  void Legalize();
};

/** Boundary resources for one metal layer, split into available pin clusters.
 */
struct IoBoundaryLayerSpace {
  IoBoundaryLayerSpace(bool is_horizontal_init, double boundary_loc_init,
                       MetalLayer* metal_layer_init);
  ~IoBoundaryLayerSpace();
  bool is_horizontal;
  double boundary_loc;
  MetalLayer* metal_layer;
  RectD default_horizontal_shape;  // unit in micron
  RectD default_vertical_shape;    // unit in micron
  bool is_using_horizontal = true;
  std::vector<IoPin*> iopin_ptr_list;
  std::vector<IoPinCluster> pin_clusters;

  /** Add an available boundary segment. */
  void AddCluster(double low, double span);

  /** Compute default pin shapes from layer and manufacturing-grid rules. */
  void ComputeDefaultShape(double manufacturing_grid);

  /** Apply default shape and layer to assigned I/O pins. */
  void UpdateIoPinShapeAndLayer();

  /** Assign pins uniformly to available clusters. */
  void UniformAssignIoPinToCluster();

  /** Assign pins greedily to available clusters. */
  void GreedyAssignIoPinToCluster();

  /** Assign pins to clusters using the configured mode. */
  void AssignIoPinToCluster();
};

/** I/O placement resources for one die boundary across all allowed layers. */
struct IoBoundarySpace {
  friend class IoPlacer;

 public:
  std::vector<IoBoundaryLayerSpace> layer_spaces_;

  IoBoundarySpace(bool is_horizontal, double boundary_loc);

  /** Add a metal layer resource on this boundary. */
  void AddLayer(MetalLayer* metal_layer);

  /** Limit the number of pins assigned to this boundary. */
  void SetIoPinLimit(int limit);

  /** Place pins currently assigned to this boundary. */
  bool PlaceAssignedPins();

 private:
  int iopin_limit_ = 0;
  bool is_iopin_limit_set_ = false;
  bool is_horizontal_;
  double boundary_loc_ = 0;
  double manufacturing_grid_;
};

}  // namespace dali

#endif  // DALI_PLACER_IO_PLACER_IO_BOUNDARY_SPACE_H_
