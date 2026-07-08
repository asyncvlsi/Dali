#include <gtest/gtest.h>

#include "dali/circuit/circuit.h"

namespace dali {

using testing::ExitedWithCode;

static Circuit MakeUnitGridCircuit() {
  Circuit circuit;
  circuit.SetManufacturingGrid(1);
  circuit.SetUnitsDistanceMicrons(1);
  circuit.SetGridValue(1, 1);
  circuit.ReserveSpaceForDesignImp(4, 0, 0);
  return circuit;
}

TEST(CircuitStatisticsTest, TracksComponentCountsAndAverages) {
  Circuit circuit = MakeUnitGridCircuit();
  circuit.SetDieArea(0, 0, 100, 100);
  circuit.AddMacro("small_cell", 4, 2);
  circuit.AddMacro("wide_cell", 10, 3);
  circuit.AddMacro("fixed_cell", 6, 5);

  circuit.AddComponent("u0", "small_cell");
  circuit.AddComponent("u1", "wide_cell");
  circuit.AddComponent("u_fixed", "fixed_cell", 0, 0, FIXED);
  circuit.UpdateTotalComponentArea();

  EXPECT_EQ(circuit.TotalComponentCount(), 3);
  EXPECT_EQ(circuit.TotalMovableComponentCnt(), 2);
  EXPECT_EQ(circuit.TotalFixedComponentCnt(), 1);
  EXPECT_EQ(circuit.MinComponentWidth(), 4);
  EXPECT_EQ(circuit.MaxComponentWidth(), 10);
  EXPECT_EQ(circuit.MinComponentHeight(), 2);
  EXPECT_EQ(circuit.MaxComponentHeight(), 5);

  EXPECT_DOUBLE_EQ(circuit.AverageComponentWidth(), 20.0 / 3.0);
  EXPECT_DOUBLE_EQ(circuit.AverageComponentHeight(), 10.0 / 3.0);
  EXPECT_DOUBLE_EQ(circuit.AverageComponentArea(), 68.0 / 3.0);
  EXPECT_DOUBLE_EQ(circuit.AverageMovableComponentWidth(), 7.0);
  EXPECT_DOUBLE_EQ(circuit.AverageMovableComponentHeight(), 2.5);
  EXPECT_DOUBLE_EQ(circuit.AverageMovableComponentArea(), 19.0);
  EXPECT_DOUBLE_EQ(circuit.WhiteSpaceUsage(), 38.0 / 9970.0);
}

TEST(CircuitStatisticsTest, RejectsAverageWithoutComponents) {
  Circuit circuit = MakeUnitGridCircuit();

  EXPECT_EXIT(circuit.AverageComponentWidth(), ExitedWithCode(1), "");
  EXPECT_EXIT(circuit.AverageMovableComponentArea(), ExitedWithCode(1), "");
  EXPECT_EXIT(circuit.WhiteSpaceUsage(), ExitedWithCode(1), "");
}

TEST(CircuitStatisticsTest, ReportsSummaryForFixedOnlyCircuit) {
  Circuit circuit = MakeUnitGridCircuit();
  circuit.SetDieArea(0, 0, 100, 100);
  circuit.AddMacro("fixed_cell", 6, 5);
  circuit.AddComponent("u_fixed", "fixed_cell", 0, 0, FIXED);
  circuit.UpdateTotalComponentArea();

  EXPECT_EQ(circuit.TotalMovableComponentCnt(), 0);
  EXPECT_NO_FATAL_FAILURE(circuit.ReportBriefSummary());
}

TEST(CircuitStatisticsTest, ReportsSummaryForEmptyCircuit) {
  Circuit circuit = MakeUnitGridCircuit();

  EXPECT_EQ(circuit.TotalComponentCount(), 0);
  EXPECT_EQ(circuit.TotalMovableComponentCnt(), 0);
  EXPECT_NO_FATAL_FAILURE(circuit.ReportBriefSummary());
}

TEST(CircuitStatisticsTest, ReportsHistogramsForEmptyCircuit) {
  Circuit circuit = MakeUnitGridCircuit();

  EXPECT_NO_FATAL_FAILURE(circuit.ReportHPWLHistogramLinear());
  EXPECT_NO_FATAL_FAILURE(circuit.ReportHPWLHistogramLogarithm());
  EXPECT_NO_FATAL_FAILURE(circuit.InitNetFanoutHistogram());
  EXPECT_NO_FATAL_FAILURE(circuit.ReportNetFanoutHistogram());
}

TEST(CircuitStatisticsTest, ReportsHistogramsForNetsWithoutPins) {
  Circuit circuit = MakeUnitGridCircuit();
  circuit.AddNet("empty_net", 0);

  EXPECT_NO_FATAL_FAILURE(circuit.ReportHPWLHistogramLinear());
  EXPECT_NO_FATAL_FAILURE(circuit.ReportHPWLHistogramLogarithm());
}

}  // namespace dali
