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
#ifndef DALI_TIMING_STARPIMODELRCESTIMATOR_H_
#define DALI_TIMING_STARPIMODELRCESTIMATOR_H_

#include <phydb/datatype.h>
#include <phydb/timing/abstractrcestimator.h>

namespace dali {
class StarPiModelEstimator : protected phydb::AbstractRcEstimator {
 public:
  explicit StarPiModelEstimator(
      phydb::PhyDB *phydb_ptr
  ) : AbstractRcEstimator(phydb_ptr) {}
  ~StarPiModelEstimator() override = default;
  void PushNetRCToManager() override;
 private:
  int32_t distance_micron_ = 0;
  bool edge_pushed_to_spef_manager_ = false;
  phydb::Layer *horizontal_layer_ = nullptr;
  phydb::Layer *vertical_layer_ = nullptr;

  void AddEdgesToManager();
  void FindFirstHorizontalAndVerticalMetalLayer();
  void GetResistanceAndCapacitance(
      phydb::Point2D<int32_t> &driver_loc,
      phydb::Point2D<int32_t> &load_loc,
      double &resistance,
      double &capacitance
  );
};
}

#endif //DALI_TIMING_STARPIMODELRCESTIMATOR_H_
