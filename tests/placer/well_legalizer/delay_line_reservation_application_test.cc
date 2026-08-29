/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#include "dali/dali.h"

#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include "dali/placer/well_legalizer/delay_line_reservation.h"

namespace dali {

class DelayLineReservationApplicationTest : public testing::Test {
 protected:
  struct ComponentState {
    double llx;
    double lly;
    PlaceStatus status;
  };

  DelayLineReservationApplicationTest() {
    placer_ = std::make_unique<Dali>(nullptr, severity::info);
    Circuit& circuit = placer_->GetCircuit();
    circuit.SetManufacturingGrid(1);
    circuit.SetUnitsDistanceMicrons(1);
    circuit.SetGridValue(1, 1);
    circuit.SetDieArea(0, 0, 100, 100);
    circuit.ReserveSpaceForDesignImp(8, 0, 6);
    circuit.AddMacro("cell", 4, 4);
    Macro* macro = circuit.GetMacroPtr("cell");
    circuit.AddMacroPin(macro, "in", true);
    circuit.AddMacroPin(macro, "out", false);

    AddChain("line_a", -5, 0, {PLACED, UNPLACED, PLACED});
    AddChain("anchor_b", 20, 20, {FIXED, PLACED, UNPLACED});
    ConnectChain("line_a");
    ConnectChain("anchor_b");
    AddRequest("line_a");
    AddRequest("anchor_b");
  }

  void AddChain(const std::string& prefix, int x, int y,
                const std::vector<PlaceStatus>& statuses) {
    Circuit& circuit = placer_->GetCircuit();
    for (int index = 0; index < 3; ++index) {
      circuit.AddComponent(prefix + "_" + std::to_string(index), "cell",
                           x + index * 4, y, statuses[index]);
    }
    circuit.AddComponent("sink_" + prefix, "cell", x + 12, y, FIXED);
  }

  void ConnectChain(const std::string& prefix) {
    Circuit& circuit = placer_->GetCircuit();
    for (int index = 0; index < 2; ++index) {
      const std::string net_name = prefix + "_net_" + std::to_string(index);
      circuit.AddNet(net_name, 2);
      circuit.AddComponentPinToNet(prefix + "_" + std::to_string(index),
                                   "out", net_name);
      circuit.AddComponentPinToNet(prefix + "_" + std::to_string(index + 1),
                                   "in", net_name);
    }
    const std::string output_net = prefix + "_output";
    circuit.AddNet(output_net, 2);
    circuit.AddComponentPinToNet(prefix + "_2", "out", output_net);
    circuit.AddComponentPinToNet("sink_" + prefix, "in", output_net);
  }

  void AddRequest(const std::string& name_prefix) {
    Dali::DelayLineSpreadRequest request;
    request.name_prefix = name_prefix;
    placer_->delay_line_spread_requests_.push_back(request);
  }

  bool Reserve() { return placer_->ReserveDelayLineComponentsForLegalization(); }

  void Restore() { placer_->RestoreDelayLineComponentStatuses(); }

  std::vector<int> ComponentIds(const std::string& prefix) const {
    const Circuit& circuit = placer_->GetCircuit();
    std::vector<int> ids;
    for (const Component& component : circuit.Components()) {
      if (component.Name().compare(0, prefix.size(), prefix) == 0)
        ids.push_back(component.Id());
    }
    return ids;
  }

  std::map<int, ComponentState> Snapshot(
      const std::vector<int>& component_ids) const {
    const Circuit& circuit = placer_->GetCircuit();
    std::map<int, ComponentState> snapshot;
    for (int component_id : component_ids) {
      const Component& component = circuit.Components()[component_id];
      snapshot.emplace(component_id,
                       ComponentState{component.LLX(), component.LLY(),
                                      component.Status()});
    }
    return snapshot;
  }

  std::unique_ptr<Dali> placer_;
};

TEST_F(DelayLineReservationApplicationTest,
       AppliesPlannedCoordinatesAndRestoresOriginalStatuses) {
  Circuit& circuit = placer_->GetCircuit();
  const std::vector<int> line_ids = ComponentIds("line_a");
  const std::vector<int> anchor_ids = ComponentIds("anchor_b");
  std::vector<int> all_ids = line_ids;
  all_ids.insert(all_ids.end(), anchor_ids.begin(), anchor_ids.end());
  const auto before = Snapshot(all_ids);

  const DelayLineReservationPlan plan = PlanDelayLineReservations(
      circuit, {{"line_a", line_ids}, {"anchor_b", anchor_ids}});
  ASSERT_TRUE(plan.valid()) << plan.error;
  std::map<int, std::pair<int, int>> planned_locations;
  for (const DelayLineReservation& reservation : plan.lines) {
    for (const DelayLineReservationComponent& planned :
         reservation.components) {
      planned_locations.emplace(planned.component_id,
                                std::make_pair(planned.llx, planned.lly));
    }
  }
  ASSERT_TRUE(Reserve());

  for (const DelayLineReservation& reservation : plan.lines) {
    for (const DelayLineReservationComponent& planned :
         reservation.components) {
      const Component& component = circuit.Components()[planned.component_id];
      EXPECT_DOUBLE_EQ(component.LLX(), planned.llx);
      EXPECT_DOUBLE_EQ(component.LLY(), planned.lly);
      EXPECT_EQ(component.Status(), FIXED);
    }
  }

  const int fixed_anchor = anchor_ids.front();
  EXPECT_DOUBLE_EQ(circuit.Components()[fixed_anchor].LLX(),
                   before.at(fixed_anchor).llx);
  EXPECT_DOUBLE_EQ(circuit.Components()[fixed_anchor].LLY(),
                   before.at(fixed_anchor).lly);

  Restore();
  for (const auto& [component_id, expected] : before) {
    const Component& component = circuit.Components()[component_id];
    EXPECT_DOUBLE_EQ(component.LLX(), planned_locations.at(component_id).first);
    EXPECT_DOUBLE_EQ(component.LLY(), planned_locations.at(component_id).second);
    EXPECT_EQ(component.Status(), expected.status);
  }
}

}  // namespace dali
