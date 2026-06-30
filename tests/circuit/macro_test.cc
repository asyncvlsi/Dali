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
#include "dali/circuit/macro.h"

#include <gtest/gtest.h>

#include <string>

namespace {

TEST(MacroTest, ReportsFirstWellHeights) {
  std::string name = "cell";
  dali::Macro macro(&name);
  macro.SetSize(10, 20);
  macro.AddPwellRect(0, 0, 10, 8);
  macro.AddNwellRect(0, 8, 10, 20);

  EXPECT_EQ(macro.FirstPwellHeight(), 8);
  EXPECT_EQ(macro.FirstNwellHeight(), 12);
  EXPECT_EQ(macro.Pheight(), macro.FirstPwellHeight());
  EXPECT_EQ(macro.Nheight(), macro.FirstNwellHeight());
}

TEST(MacroTest, InfersFirstWellHeightsFromNwellOnlyGeometry) {
  std::string name = "nwell_only_cell";
  dali::Macro macro(&name);
  macro.SetSize(10, 20);
  macro.AddNwellRect(0, 6, 10, 20);

  EXPECT_EQ(macro.FirstPwellHeight(), 6);
  EXPECT_EQ(macro.FirstNwellHeight(), 14);
}

}  // namespace
