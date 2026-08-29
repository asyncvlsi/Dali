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

/**
 * @file
 * Star and pi RC models for timing-driven placement.
 *
 * Implements PhyDB's AbstractRcEstimator so a timer can ask for net parasitics
 * derived from the current placement. Placement supplies geometry only; the
 * timing engine itself lives outside Dali.
 */
#ifndef DALI_TIMING_STAR_PI_MODEL_ESTIMATOR_H_
#define DALI_TIMING_STAR_PI_MODEL_ESTIMATOR_H_

#include <phydb/datatype.h>
#include <phydb/timing/abstractrcestimator.h>

#include <string>

namespace dali {

/**
 * Whether a net's RC should be extracted, given what is known about it.
 *
 * Extraction walks a net twice -- once to create parasitic edges and once to
 * set resistance and capacitance on them -- and the two walks must cover
 * exactly the same nets, because setting R on an edge that was never created
 * aborts. Deciding once, here, is what keeps them in agreement; the two loops
 * previously carried separate copies of the condition and drifted apart.
 *
 * A net is extracted when it maps to ACT and something drives it. Component
 * pins and I/O pins are both endpoints: a design input port drives the net it
 * attaches to, and an output port is one of the loads a driver reaches.
 *
 * I/O endpoints are handled here but not yet reachable. PhyDB records a driver
 * only from a component pin, so every port-driven net still reports -1 and is
 * skipped -- 129 nets on bd_pipeline, including 64 `mask[*]` nets with eight
 * loads each, roughly 451 unextracted segments. Two things are needed and only
 * one of them works: binding a port to its ACT pin, which resolves through the
 * timing netlist's virtual-driver name (the port name with a trailing `$`) and
 * runs clean; and giving that pin a node in the parasitics graph, which makes
 * the timer segfault inside its own incremental analysis. Bound-but-not-in-the
 * graph exits 0, bound-and-in-the-graph exits 139, which isolates the failure to
 * the parasitics side rather than the binding. Until the timer accepts port
 * nodes, an I/O pin has no node and is skipped as an endpoint.
 *
 * `driver_pin_id` is a position within one of the net's two pin lists, chosen
 * by `driver_is_io_pin`: the component pins, or the I/O pins when a design
 * input port drives the net. A net with neither reports -1 and has no wire to
 * extract, which is the only case excluded here.
 */
bool ShouldExtractNetRC(bool has_act_net_ptr, bool driver_is_io_pin,
                        int driver_pin_id, int component_pin_count,
                        int io_pin_count);

class StarPiModelEstimator : protected phydb::AbstractRcEstimator {
 public:
  explicit StarPiModelEstimator(phydb::PhyDB* phydb_ptr)
      : AbstractRcEstimator(phydb_ptr) {}
  ~StarPiModelEstimator() override = default;
  void PushNetRCToManager() override;

  /**
   * Lowest routing layer the estimate is allowed to use.
   *
   * The estimator models every net on the first horizontal and first vertical
   * routing layer it finds, which is LEF order. In Sky130 that is `li`, local
   * interconnect meant for pin access and very short intra-cell hops, at
   * roughly fifty times the sheet resistance of `met1`. A router would not put
   * signal nets there, so charging every net to `li` overstates wire delay
   * badly -- measured at 18.9 ns against 0.45 ns on the same design.
   *
   * The choice was invisible while every layer carried the same resistance;
   * it only matters once the technology model distinguishes them. Defaults to
   * 0, which preserves the previous behaviour exactly.
   */
  /**
   * The configuration actually in force here, and the layers it resolved to.
   *
   * Dali's own member is not evidence: the two disagreed for the whole of
   * Amendments L, N and O, because the setter wrote the member and the
   * estimator that was already built kept its own. The resolved names are the
   * stronger witness of the two -- they are what the RC numbers were charged
   * to, and they are empty until something has actually asked for an RC.
   */
  int MinRoutingLayer() const { return min_routing_layer_; }
  std::string ResolvedHorizontalLayerName() const {
    return horizontal_layer_ == nullptr ? std::string() : horizontal_layer_->GetName();
  }
  std::string ResolvedVerticalLayerName() const {
    return vertical_layer_ == nullptr ? std::string() : vertical_layer_->GetName();
  }

  void SetMinRoutingLayer(int layer_index) {
    min_routing_layer_ = layer_index;
    horizontal_layer_ = nullptr;
    vertical_layer_ = nullptr;
  }

 private:
  int distance_micron_ = 0;
  int min_routing_layer_ = 0;
  bool edge_pushed_to_spef_manager_ = false;
  phydb::Layer* horizontal_layer_ = nullptr;
  phydb::Layer* vertical_layer_ = nullptr;

  void AddEdgesToManager();
  void FindFirstHorizontalAndVerticalMetalLayer();
  void GetResistanceAndCapacitance(phydb::Point2D<int>& driver_loc,
                                   phydb::Point2D<int>& load_loc,
                                   double& resistance, double& capacitance);
};
}  // namespace dali

#endif  // DALI_TIMING_STAR_PI_MODEL_ESTIMATOR_H_
