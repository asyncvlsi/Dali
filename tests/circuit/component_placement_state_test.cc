/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ******************************************************************************/
#include "dali/circuit/component_placement_state.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "dali/circuit/macro.h"

class ComponentPlacementStateTest : public testing::Test {
 protected:
  std::string macro_name = "cell";
  std::string name_a = "a";
  std::string name_b = "b";
  dali::Macro macro{&macro_name};
  std::vector<dali::Component> components;

  void SetUp() override {
    macro.SetSize(10, 4);
    components.emplace_back(&name_a);
    components.emplace_back(&name_b);
    for (dali::Component &component : components) {
      component.SetMacro(&macro);
    }
    components[0].SetLoc(3, 5);
    components[0].SetPlacementStatus(dali::UNPLACED);
    components[1].SetLoc(40, 8);
    components[1].SetOrient(dali::FS);
    components[1].SetPlacementStatus(dali::FIXED);
  }
};

// What a feedback pass leaves behind -- legalized coordinates, flipped rows,
// promoted status -- is undone, so the next pass starts where the first did.
TEST_F(ComponentPlacementStateTest, RestoresLocationOrientationAndStatus) {
  const dali::ComponentPlacementState checkpoint(components);
  components[0].SetLoc(90, 70);
  components[0].SetOrient(dali::FS);
  components[0].SetPlacementStatus(dali::PLACED);
  components[1].SetLoc(0, 0);
  components[1].SetOrient(dali::N);
  ASSERT_EQ(components[0].LLX(), 90);  // the mutation landed

  ASSERT_TRUE(checkpoint.Restore(components));
  EXPECT_EQ(components[0].LLX(), 3);
  EXPECT_EQ(components[0].LLY(), 5);
  EXPECT_EQ(components[0].Orient(), dali::N);
  EXPECT_EQ(components[0].Status(), dali::UNPLACED);
  EXPECT_EQ(components[1].LLX(), 40);
  EXPECT_EQ(components[1].LLY(), 8);
  EXPECT_EQ(components[1].Orient(), dali::FS);
  EXPECT_EQ(components[1].Status(), dali::FIXED);
}

TEST_F(ComponentPlacementStateTest, RefusesADifferentComponentCount) {
  const dali::ComponentPlacementState checkpoint(components);
  std::string name_c = "c";
  components.emplace_back(&name_c);
  components.back().SetMacro(&macro);
  components[0].SetLoc(90, 70);

  EXPECT_FALSE(checkpoint.Restore(components));
  EXPECT_EQ(components[0].LLX(), 90);
}
