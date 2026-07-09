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

TEST(DieAreaTest, PreservesAnOffsetPlacementGridOrigin) {
  Circuit circuit;
  circuit.SetManufacturingGrid(0.001);
  circuit.SetUnitsDistanceMicrons(1000);
  circuit.SetGridValue(1, 12);
  circuit.SetPlacementGridOrigin(459000, 459000);
  std::vector<int2d> die_area = MakeDieArea({{459000, 459000},
                                             {459000, 11139000},
                                             {11151000, 11139000},
                                             {11151000, 459000}});

  circuit.SetRectilinearDieArea(die_area);

  EXPECT_EQ(circuit.DieAreaOffsetX(), 0);
  EXPECT_EQ(circuit.DieAreaOffsetY(), 3000);
  EXPECT_EQ(circuit.RegionLLY(), 38);
  EXPECT_EQ(circuit.RegionURY(), 928);
  EXPECT_DOUBLE_EQ(circuit.LocPhydb2DaliY(8619000), 718);
  EXPECT_EQ(circuit.LocDali2PhydbY(718), 8619000);
}

}  // namespace dali
