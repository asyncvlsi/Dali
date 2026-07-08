#include "dali/placer/legalizer/standard_cell_legalizer/standard_cell_placement_model.h"

#include <gtest/gtest.h>

namespace dali {

TEST(StandardCellPlacementModelTest, KeepsWholeRowFreeWithoutBlockages) {
  StandardCellPlacementModel model;

  model.AddRow(10, 20, 12, 2, 5);
  model.BuildFreeSegments();

  ASSERT_EQ(model.RowCount(), 1);
  ASSERT_EQ(model.Rows()[0].free_segments.size(), 1);
  EXPECT_EQ(model.Rows()[0].free_segments[0].lx, 10);
  EXPECT_EQ(model.Rows()[0].free_segments[0].ux, 20);
  EXPECT_TRUE(model.IsIntervalFree(0, 10, 20));
}

TEST(StandardCellPlacementModelTest, SplitsRowsAroundBlockages) {
  StandardCellPlacementModel model;

  model.AddRow(0, 0, 10, 2, 10);
  model.AddBlockage(5, -1, 11, 11);
  model.BuildFreeSegments();

  ASSERT_EQ(model.Rows()[0].free_segments.size(), 2);
  EXPECT_EQ(model.Rows()[0].free_segments[0].lx, 0);
  EXPECT_EQ(model.Rows()[0].free_segments[0].ux, 4);
  EXPECT_EQ(model.Rows()[0].free_segments[1].lx, 12);
  EXPECT_EQ(model.Rows()[0].free_segments[1].ux, 20);
  EXPECT_TRUE(model.IsIntervalFree(0, 12, 20));
  EXPECT_FALSE(model.IsIntervalFree(0, 4, 12));
}

TEST(StandardCellPlacementModelTest, ClipsBlockagesToTouchedRows) {
  StandardCellPlacementModel model;

  model.AddRow(0, 0, 10, 1, 10);
  model.AddRow(0, 10, 10, 1, 10);
  model.AddBlockage(3, 1, 6, 9);
  model.BuildFreeSegments();

  ASSERT_EQ(model.Rows()[0].free_segments.size(), 2);
  EXPECT_EQ(model.Rows()[1].free_segments.size(), 1);
  EXPECT_TRUE(model.IsIntervalFree(1, 0, 10));
}

TEST(StandardCellPlacementModelTest, FindsRowContainingY) {
  StandardCellPlacementModel model;

  model.AddRow(0, 100, 10, 1, 10);
  model.AddRow(0, 110, 10, 1, 10);

  ASSERT_TRUE(model.RowIndexAtY(100).has_value());
  EXPECT_EQ(*model.RowIndexAtY(100), 0);
  ASSERT_TRUE(model.RowIndexAtY(119).has_value());
  EXPECT_EQ(*model.RowIndexAtY(119), 1);
  EXPECT_FALSE(model.RowIndexAtY(120).has_value());
}

}  // namespace dali
