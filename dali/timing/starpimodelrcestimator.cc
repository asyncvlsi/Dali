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
#include "starpimodelrcestimator.h"

#include "dali/common/logging.h"

namespace dali {

void StarPiModelEstimator::PushNetRCToManager() {
#if PHYDB_USE_GALOIS
  FindFirstHorizontalAndVerticalMetalLayer();
  // AddEdgesToManager();
  auto maxMode = galois::eda::utility::AnalysisMode::ANALYSIS_MAX;
  auto &timing_api = phy_db_->GetTimingApi();
  auto *spef_manager = phy_db_->GetParaManager();
  auto &libs = phy_db_->GetCellLibs();
  auto &design = *(phy_db_->GetDesignPtr());
  DaliExpects(!libs.empty(), "CellLibs empty?");
  auto &nets = phy_db_->design().GetNetsRef();
  for (auto &net : nets) {
    if (!net.GetIoPinIdsRef().empty()) continue;
    int driver_id = net.GetDriverPinId();
    auto &net_pins = net.GetPinsRef();
    auto &driver = net_pins[driver_id];
    std::string driver_name = phy_db_->GetFullCompPinName(driver);
    auto *driver_node = timing_api.PhyDBPinToSpefNode(driver);
    double driver_cap = 0;
    phydb::Point2D<int> driver_pin_loc =
        design.GetComponentPinLocation(driver.InstanceId(), driver.PinId());
    int net_sz = (int)net_pins.size();
    for (int pin_id = 0; pin_id < net_sz; ++pin_id) {
      if (pin_id == driver_id) continue;
      phydb::PhydbPin &load = net_pins[pin_id];
      std::string load_name = phy_db_->GetFullCompPinName(load);
      auto load_node = timing_api.PhyDBPinToSpefNode(load);
      double res, cap;
      phydb::Point2D<int> load_pin_loc =
          design.GetComponentPinLocation(load.InstanceId(), load.PinId());
      ;
      GetResistanceAndCapacitance(driver_pin_loc, load_pin_loc, res, cap);
      load_node->setC(libs[0], maxMode, cap / 2.0);
      std::cout << "Set C for load pin: " << load_name << " " << cap / 2.0
                << "\n";
      driver_cap += cap / 2.0;
      auto edge = spef_manager->findEdge(driver_node, load_node);
      DaliExpects(edge != nullptr, "Cannot find edge!");
      edge->setR(libs[0], maxMode, res);
      std::cout << "Set R for edge, "
                << "driver: " << driver_name << ", "
                << "load: " << load_name << ", " << res << "\n";
    }
    driver_node->setC(libs[0], maxMode, driver_cap);
    // std::cout << "Set C for driver pin: " << driver_name << " " << driver_cap
    // << "\n";
  }
  // std::cout << "after adding\n";
  // spef_manager->dump();
  // std::cout << "--------------------------------------------------\n";
#endif
}

void StarPiModelEstimator::AddEdgesToManager() {
#if PHYDB_USE_GALOIS
  if (edge_pushed_to_spef_manager_) return;
  edge_pushed_to_spef_manager_ = true;
  auto &timing_api = phy_db_->GetTimingApi();
  auto spef_manager = phy_db_->GetParaManager();
  for (auto &net : phy_db_->design().GetNetsRef()) {
    int driver_id = net.GetDriverPinId();
    auto &net_pins = net.GetPinsRef();
    auto &driver = net_pins[driver_id];
    auto driver_node = timing_api.PhyDBPinToSpefNode(driver);
    int net_sz = static_cast<int>(net_pins.size());
    for (int i = 0; i < net_sz; ++i) {
      if (i == driver_id) continue;
      auto &load = net_pins[i];
      auto load_node = timing_api.PhyDBPinToSpefNode(load);
      auto ret = spef_manager->addEdge(driver_node, load_node);
      DaliExpects(ret != nullptr, "Fail to add an edge\n");
    }
  }
#endif
}

void StarPiModelEstimator::FindFirstHorizontalAndVerticalMetalLayer() {
  distance_micron_ = phy_db_->design().GetUnitsDistanceMicrons();
  if (horizontal_layer_ != nullptr && vertical_layer_ != nullptr) return;
  for (auto &metal : phy_db_->tech().GetMetalLayersRef()) {
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
    phydb::Point2D<int> &driver_loc, phydb::Point2D<int> &load_loc,
    double &resistance, double &capacitance) {
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
