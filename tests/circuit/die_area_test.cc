#include <gtest/gtest.h>

#include <vector>

#include "dali/circuit/circuit.h"
#include "dali/common/misc.h"

namespace dali {

static Circuit MakeUnitGridCircuit() {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  return circuit;
}

static std::vector<int2d> MakeDieArea(
    std::initializer_list<std::pair<int, int>> pts) {
  std::vector<int2d> die_area;
  die_area.reserve(pts.size());
  for (const auto& [x, y] : pts) {
    die_area.emplace_back(x, y);
  }
  return die_area;
}

TEST(DieAreaTest, CreatesBlockageForRectilinearNotch) {
  Circuit circuit = MakeUnitGridCircuit();
  std::vector<int2d> die_area =
      MakeDieArea({{0, 0}, {0, 10}, {5, 10}, {5, 5}, {10, 5}, {10, 0}});

  circuit.SetRectilinearDieArea(die_area);
  circuit.design().UpdatePlacementBlockages();

  EXPECT_EQ(circuit.RegionLLX(), 0);
  EXPECT_EQ(circuit.RegionLLY(), 0);
  EXPECT_EQ(circuit.RegionURX(), 10);
  EXPECT_EQ(circuit.RegionURY(), 10);

  const auto& blockages = circuit.design().PlacementBlockages();
  ASSERT_EQ(blockages.size(), 1U);
  EXPECT_EQ(blockages[0].GetRect().LLX(), 5);
  EXPECT_EQ(blockages[0].GetRect().LLY(), 5);
  EXPECT_EQ(blockages[0].GetRect().URX(), 10);
  EXPECT_EQ(blockages[0].GetRect().URY(), 10);
}

TEST(DieAreaTest, RemovesRedundantRectilinearVertices) {
  Circuit circuit = MakeUnitGridCircuit();
  std::vector<int2d> die_area = MakeDieArea(
      {{0, 0}, {0, 4}, {0, 8}, {0, 10}, {5, 10}, {5, 5}, {10, 5}, {10, 0}});

  circuit.SetRectilinearDieArea(die_area);
  circuit.design().UpdatePlacementBlockages();

  EXPECT_EQ(circuit.RegionLLX(), 0);
  EXPECT_EQ(circuit.RegionLLY(), 0);
  EXPECT_EQ(circuit.RegionURX(), 10);
  EXPECT_EQ(circuit.RegionURY(), 10);

  const auto& blockages = circuit.design().PlacementBlockages();
  ASSERT_EQ(blockages.size(), 1U);
  EXPECT_EQ(blockages[0].GetRect().LLX(), 5);
  EXPECT_EQ(blockages[0].GetRect().LLY(), 5);
  EXPECT_EQ(blockages[0].GetRect().URX(), 10);
  EXPECT_EQ(blockages[0].GetRect().URY(), 10);
}

}  // namespace dali
