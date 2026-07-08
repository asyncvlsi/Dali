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
#include "dali/circuit/component.h"

#include <gtest/gtest.h>

#include <string>

#include "dali/circuit/macro.h"

class ComponentTest : public testing::Test {
 protected:
  std::string macro_name = "cell";
  std::string component_name = "u0";
  dali::Macro macro{&macro_name};
  dali::Component component{&component_name};

  void SetUp() override {
    macro.SetSize(10, 4);
    component.SetMacro(&macro);
  }
};

TEST_F(ComponentTest, ReportsLowerLeftAndCenterCoordinates) {
  component.SetLowerLeft(7, 9);

  EXPECT_EQ(component.LLX(), 7);
  EXPECT_EQ(component.LLY(), 9);
  EXPECT_EQ(component.URX(), 17);
  EXPECT_EQ(component.URY(), 13);
  EXPECT_EQ(component.CenterX(), 12);
  EXPECT_EQ(component.CenterY(), 11);
  EXPECT_EQ(component.X(), component.CenterX());
  EXPECT_EQ(component.Y(), component.CenterY());
}

TEST_F(ComponentTest, SetLocPreservesLegacyLowerLeftBehavior) {
  component.SetLoc(3, 5);

  EXPECT_EQ(component.LLX(), 3);
  EXPECT_EQ(component.LLY(), 5);
  EXPECT_EQ(component.CenterX(), 8);
  EXPECT_EQ(component.CenterY(), 7);
}

TEST_F(ComponentTest, TracksEffectiveHeightAndArea) {
  EXPECT_EQ(component.Height(), 4);
  EXPECT_EQ(component.Area(), 40);

  component.SetHeight(6);
  EXPECT_EQ(component.Height(), 6);
  EXPECT_EQ(component.Area(), 60);

  component.ResetHeight();
  EXPECT_EQ(component.Height(), 4);
  EXPECT_EQ(component.Area(), 40);
}
