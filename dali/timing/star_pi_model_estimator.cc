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
#include "star_pi_model_estimator.h"

#include "dali/common/logging.h"

namespace dali {

bool ShouldExtractNetRC(bool has_act_net_ptr, bool driver_is_io_pin,
                        int driver_pin_id, int component_pin_count,
                        int io_pin_count) {
  if (!has_act_net_ptr) return false;
  if (driver_pin_id < 0) return false;
  const int list_size = driver_is_io_pin ? io_pin_count : component_pin_count;
  return driver_pin_id < list_size;
}

#if PHYDB_USE_GALOIS
/**
 * The endpoints of one net: the driver, and everything it reaches.
 *
 * Built once and used by both extraction walks, because they must agree on
 * exactly which endpoint pairs exist -- the first creates the parasitic edges
 * the second sets resistance on, and setting R on an absent edge aborts.
 *
 * Component pins and I/O pins are both endpoints here. Keeping them apart, as
 * the two lists on a PhyDB net do, is what previously caused a net's port end
 * to contribute no wire at all.
 */
namespace {

struct NetEndpoints {
  bool usable = false;
  galois::eda::parasitics::Node *driver_node = nullptr;
  phydb::Point2D<int> driver_location;
  std::vector<galois::eda::parasitics::Node *> load_nodes;
  std::vector<phydb::Point2D<int>> load_locations;
};

/**
 * The parasitics node for a pin, or null when it has none.
 *
 * PhyDBPinToSpefNode aborts on a pin the parasitics manager does not know, so
 * anything that might not be in the graph is resolved through here instead.
 * I/O pins are the case that matters: they are endpoints of a net electrically,
 * but they are only in the parasitics graph if something put them there.
 */
galois::eda::parasitics::Node *SpefNodeOrNull(phydb::PhyDB *phy_db,
                                              phydb::PhydbPin pin) {
  void *act_pin = phy_db->GetTimingApi().PhydbCompPin2ActPtr(pin);
  if (act_pin == nullptr) return nullptr;
  return phy_db->GetParaManager()->findPin(act_pin);
}

NetEndpoints CollectNetEndpoints(phydb::PhyDB *phy_db, phydb::Net &net) {
  NetEndpoints endpoints;
  auto &timing_api = phy_db->GetTimingApi();
  auto &design = *(phy_db->GetDesignPtr());
  auto &io_pins = design.GetIoPinsRef();
  auto &net_pins = net.GetPinsRef();
  std::vector<int> &io_pin_ids = net.GetIoPinIdsRef();

  const int driver_id = net.GetDriverPinId();
  const bool driver_is_io = net.IsDriverIoPin();

  if (!ShouldExtractNetRC(true, driver_is_io, driver_id,
                          static_cast<int>(net_pins.size()),
                          static_cast<int>(io_pin_ids.size()))) {
    return endpoints;
  }

  if (driver_is_io) {
    phydb::IOPin &driver = io_pins[io_pin_ids[driver_id]];
    phydb::PhydbPin driver_pin(-1, io_pin_ids[driver_id]);
    endpoints.driver_node = SpefNodeOrNull(phy_db, driver_pin);
    // A port that drives the net but has no parasitics node cannot anchor a
    // wire, so the net has no extractable geometry rather than a bad one.
    if (endpoints.driver_node == nullptr) return endpoints;
    endpoints.driver_location = driver.GetLocation();
  } else {
    phydb::PhydbPin &driver = net_pins[driver_id];
    endpoints.driver_node = timing_api.PhyDBPinToSpefNode(driver);
    endpoints.driver_location =
        design.GetComponentPinLocation(driver.InstanceId(), driver.PinId());
  }

  for (int pin_id = 0; pin_id < static_cast<int>(net_pins.size()); ++pin_id) {
    if (!driver_is_io && pin_id == driver_id) continue;
    phydb::PhydbPin &load = net_pins[pin_id];
    endpoints.load_nodes.push_back(timing_api.PhyDBPinToSpefNode(load));
    endpoints.load_locations.push_back(
        design.GetComponentPinLocation(load.InstanceId(), load.PinId()));
  }
  for (int index = 0; index < static_cast<int>(io_pin_ids.size()); ++index) {
    if (driver_is_io && index == driver_id) continue;
    phydb::PhydbPin io_pin(-1, io_pin_ids[index]);
    // An I/O pin outside the parasitics graph -- power and ground, and today
    // every signal port -- has no node to attach wire to. See the I/O endpoint
    // note in the header for why that is still the case.
    galois::eda::parasitics::Node *node = SpefNodeOrNull(phy_db, io_pin);
    if (node == nullptr) continue;
    endpoints.load_nodes.push_back(node);
    endpoints.load_locations.push_back(io_pins[io_pin_ids[index]].GetLocation());
  }
  endpoints.usable = true;
  return endpoints;
}

}  // namespace
#endif

/** Hand the estimated per-net RC parasitics to the timing manager. */
void StarPiModelEstimator::PushNetRCToManager() {
#if PHYDB_USE_GALOIS
  FindFirstHorizontalAndVerticalMetalLayer();
  AddEdgesToManager();
  auto& timing_api = phy_db_->GetTimingApi();
  auto* spef_manager = phy_db_->GetParaManager();
  auto& nets = phy_db_->design().GetNetsRef();
  for (int net_id = 0; net_id < static_cast<int>(nets.size()); ++net_id) {
    auto& net = nets[net_id];
    if (timing_api.PhydbNetId2ActPtr(net_id) == nullptr) {
      LOG(trace) << "RC_SKIP net " << net.GetName() << " reason no_act_ptr\n";
      continue;
    }
    NetEndpoints endpoints = CollectNetEndpoints(phy_db_, net);
    if (!endpoints.usable) {
      LOG(trace) << "RC_SKIP net " << net.GetName()
                 << " reason no_driver driver_id " << net.GetDriverPinId()
                 << " io_driver " << net.IsDriverIoPin() << "\n";
      continue;
    }
    LOG(trace) << "RC_NET net " << net.GetName() << " pins "
               << net.GetPinsRef().size() << " iopins "
               << net.GetIoPinIdsRef().size() << " io_driver "
               << net.IsDriverIoPin() << " loads "
               << endpoints.load_nodes.size() << "\n";

    double driver_cap = 0;
    for (size_t index = 0; index < endpoints.load_nodes.size(); ++index) {
      double res, cap;
      GetResistanceAndCapacitance(endpoints.driver_location,
                                  endpoints.load_locations[index], res, cap);
      // The per-net extraction inputs are the only place to see why a long net
      // can come back with negligible RC. Trace level, so it costs nothing at
      // the default verbosity.
      LOG(trace) << "RC_TRACE net " << net.GetName() << " dx "
                 << std::abs(endpoints.driver_location.x -
                             endpoints.load_locations[index].x) /
                        (double)distance_micron_
                 << " dy "
                 << std::abs(endpoints.driver_location.y -
                             endpoints.load_locations[index].y) /
                        (double)distance_micron_
                 << " res " << res << " cap " << cap << "\n";
      endpoints.load_nodes[index]->setC(0, cap / 2.0);
      driver_cap += cap / 2.0;
      auto edge = spef_manager->findEdge(endpoints.driver_node,
                                         endpoints.load_nodes[index]);
      DaliExpects(edge != nullptr, "Cannot find edge!");
      edge->setR(0, res);
    }
    endpoints.driver_node->setC(0, driver_cap);
  }
#endif
}

void StarPiModelEstimator::AddEdgesToManager() {
#if PHYDB_USE_GALOIS
  if (edge_pushed_to_spef_manager_) return;
  edge_pushed_to_spef_manager_ = true;
  auto& timing_api = phy_db_->GetTimingApi();
  auto spef_manager = phy_db_->GetParaManager();
  auto& nets = phy_db_->design().GetNetsRef();
  for (int net_id = 0; net_id < static_cast<int>(nets.size()); ++net_id) {
    auto& net = nets[net_id];
    if (timing_api.PhydbNetId2ActPtr(net_id) == nullptr) continue;
    // Shares CollectNetEndpoints with PushNetRCToManager so the two cover
    // exactly the same endpoint pairs: this loop creates the edges that one
    // sets resistance on, and an edge that was never created aborts there.
    NetEndpoints endpoints = CollectNetEndpoints(phy_db_, net);
    if (!endpoints.usable) continue;
    for (auto *load_node : endpoints.load_nodes) {
      auto ret = spef_manager->addEdge(endpoints.driver_node, load_node);
      DaliExpects(ret != nullptr, "Fail to add an edge\n");
    }
  }
#endif
}

void StarPiModelEstimator::FindFirstHorizontalAndVerticalMetalLayer() {
  distance_micron_ = phy_db_->design().GetUnitsDistanceMicrons();
  if (horizontal_layer_ != nullptr && vertical_layer_ != nullptr) return;
  int layer_index = -1;
  for (auto& metal : phy_db_->tech().GetMetalLayersRef()) {
    ++layer_index;
    if (layer_index < min_routing_layer_) continue;
    if (horizontal_layer_ == nullptr &&
        metal->GetDirection() == phydb::MetalDirection::HORIZONTAL) {
      horizontal_layer_ = metal;
    }
    if (vertical_layer_ == nullptr &&
        metal->GetDirection() == phydb::MetalDirection::VERTICAL) {
      vertical_layer_ = metal;
    }
  }

  DaliExpects(horizontal_layer_ != nullptr,
              "Cannot find RC parameters in a horizontal layer?");
  DaliExpects(vertical_layer_ != nullptr,
              "Cannot find RC parameters in a vertical layer?");
}

void StarPiModelEstimator::GetResistanceAndCapacitance(
    phydb::Point2D<int>& driver_loc, phydb::Point2D<int>& load_loc,
    double& resistance, double& capacitance) {
  double x_span =
      std::abs(driver_loc.x - load_loc.x) / (double)distance_micron_;
  double y_span =
      std::abs(driver_loc.y - load_loc.y) / (double)distance_micron_;

  double hor_res = horizontal_layer_->GetResistance(
      horizontal_layer_->GetWidth(), x_span, 0);
  double ver_res =
      vertical_layer_->GetResistance(vertical_layer_->GetWidth(), y_span, 0);
  resistance = hor_res + ver_res;

  double hor_cap = horizontal_layer_->GetFringeCapacitance(
      horizontal_layer_->GetWidth(), x_span, 0);
  double ver_cap = vertical_layer_->GetFringeCapacitance(
      vertical_layer_->GetWidth(), y_span, 0);
  capacitance = hor_cap + ver_cap;
}

}  // namespace dali
