#include "dali/placer/legalizer/standard_cell_legalizer/standard_cell_row_legalizer.h"

#include <gtest/gtest.h>

namespace dali {

StandardCellRowLegalizationCell MakeCell(int id, int width, double target_lx) {
  StandardCellRowLegalizationCell cell;
  cell.id = id;
  cell.width = width;
  cell.target_lx = target_lx;
  return cell;
}

TEST(StandardCellRowLegalizerTest, PreservesOrderAndRemovesOverlap) {
  StandardCellRowLegalizer legalizer;
  std::vector<StandardCellRowLegalizationCell> cells = {
      MakeCell(0, 4, 0.0),
      MakeCell(1, 4, 2.0),
      MakeCell(2, 4, 4.0),
  };

  ASSERT_TRUE(legalizer.Legalize({0, 20}, 1, &cells));

  EXPECT_EQ(cells[0].legal_lx, 0);
  EXPECT_EQ(cells[1].legal_lx, 4);
  EXPECT_EQ(cells[2].legal_lx, 8);
}

TEST(StandardCellRowLegalizerTest, ClampsClusterInsideSegment) {
  StandardCellRowLegalizer legalizer;
  std::vector<StandardCellRowLegalizationCell> cells = {
      MakeCell(0, 4, 90.0),
      MakeCell(1, 4, 95.0),
  };

  ASSERT_TRUE(legalizer.Legalize({10, 30}, 1, &cells));

  EXPECT_EQ(cells[0].legal_lx, 22);
  EXPECT_EQ(cells[1].legal_lx, 26);
}

TEST(StandardCellRowLegalizerTest, SnapsToSites) {
  StandardCellRowLegalizer legalizer;
  std::vector<StandardCellRowLegalizationCell> cells = {
      MakeCell(0, 4, 3.0),
      MakeCell(1, 4, 11.0),
  };

  ASSERT_TRUE(legalizer.Legalize({0, 30}, 2, &cells));

  EXPECT_EQ(cells[0].legal_lx % 2, 0);
  EXPECT_EQ(cells[1].legal_lx % 2, 0);
  EXPECT_GE(cells[1].legal_lx, cells[0].legal_lx + cells[0].width);
}

TEST(StandardCellRowLegalizerTest, RejectsOverflowingSegment) {
  StandardCellRowLegalizer legalizer;
  std::vector<StandardCellRowLegalizationCell> cells = {
      MakeCell(0, 8, 0.0),
      MakeCell(1, 8, 8.0),
  };

  EXPECT_FALSE(legalizer.Legalize({0, 10}, 1, &cells));
}

}  // namespace dali
