#include "dali/circuit/tech.h"

#include <gtest/gtest.h>

#include "dali/circuit/circuit.h"

namespace dali {
namespace {

using testing::ExitedWithCode;

TEST(TechTest, ReturnsManufacturingGridAfterCircuitSetsIt) {
  Circuit circuit;
  circuit.SetManufacturingGrid(0.001);

  EXPECT_DOUBLE_EQ(circuit.tech().ManufacturingGrid(), 0.001);
  EXPECT_DOUBLE_EQ(circuit.tech().GetManufacturingGrid(), 0.001);
}

TEST(TechTest, RejectsUnsetManufacturingGrid) {
  Tech tech;

  EXPECT_EXIT(tech.ManufacturingGrid(), ExitedWithCode(1), "");
}

}  // namespace
}  // namespace dali
