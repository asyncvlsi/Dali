#include "dali/timing/delay_line_detour.h"

#include <gtest/gtest.h>

#include <cstddef>

#include <algorithm>
#include <cmath>
#include <map>
#include <numeric>
#include <set>
#include <utility>

namespace dali {

namespace {

DelayLineChain HorizontalChain(int count, double spacing) {
  DelayLineChain chain;
  chain.name = "dl0";
  for (int index = 0; index < count; ++index)
    chain.nodes.push_back({index, index * spacing, 100.0});
  return chain;
}

} // namespace

TEST(BuildZigzagDetourTest, PinsBothEndpoints) {
  std::vector<DetourTarget> targets = BuildZigzagDetour(HorizontalChain(8, 3.0), 50.0);

  ASSERT_EQ(targets.size(), 6u);
  for (const DetourTarget &target : targets) {
    EXPECT_NE(target.component_id, 0);
    EXPECT_NE(target.component_id, 7);
  }
}

TEST(BuildZigzagDetourTest, DisplacesInteriorAlternatelyPerpendicular) {
  std::vector<DetourTarget> targets = BuildZigzagDetour(HorizontalChain(5, 10.0), 25.0);

  ASSERT_EQ(targets.size(), 3u);
  EXPECT_DOUBLE_EQ(targets[0].y, 125.0);
  EXPECT_DOUBLE_EQ(targets[1].y, 75.0);
  EXPECT_DOUBLE_EQ(targets[2].y, 125.0);
  EXPECT_DOUBLE_EQ(targets[0].x, 10.0);
  EXPECT_DOUBLE_EQ(targets[1].x, 20.0);
  EXPECT_DOUBLE_EQ(targets[2].x, 30.0);
}

TEST(BuildZigzagDetourTest, DisplacementIsPerpendicularToAnyAxis) {
  DelayLineChain chain;
  chain.nodes = {{0, 0.0, 0.0}, {1, 5.0, 5.0}, {2, 10.0, 10.0}};

  std::vector<DetourTarget> targets = BuildZigzagDetour(chain, std::sqrt(2.0));

  ASSERT_EQ(targets.size(), 1u);
  EXPECT_NEAR(targets[0].x, 4.0, 1e-9);
  EXPECT_NEAR(targets[0].y, 6.0, 1e-9);
}

TEST(BuildZigzagDetourTest, CoincidentEndpointsStillProduceAFiniteShape) {
  DelayLineChain chain;
  chain.nodes = {{0, 7.0, 7.0}, {1, 7.0, 7.0}, {2, 7.0, 7.0}};

  std::vector<DetourTarget> targets = BuildZigzagDetour(chain, 40.0);

  ASSERT_EQ(targets.size(), 1u);
  EXPECT_TRUE(std::isfinite(targets[0].x));
  EXPECT_TRUE(std::isfinite(targets[0].y));
  EXPECT_DOUBLE_EQ(targets[0].x, 7.0);
  EXPECT_DOUBLE_EQ(targets[0].y, 47.0);
}

TEST(BuildZigzagDetourTest, AmplitudeScalesTheDisplacement) {
  std::vector<DetourTarget> small = BuildZigzagDetour(HorizontalChain(5, 10.0), 10.0);
  std::vector<DetourTarget> large = BuildZigzagDetour(HorizontalChain(5, 10.0), 30.0);

  ASSERT_EQ(small.size(), large.size());
  for (size_t index = 0; index < small.size(); ++index) {
    EXPECT_DOUBLE_EQ(small[index].x, large[index].x);
    EXPECT_DOUBLE_EQ(std::abs(large[index].y - 100.0),
                     3.0 * std::abs(small[index].y - 100.0));
  }
}

TEST(BuildZigzagDetourTest, ChainsWithoutInteriorAreLeftAlone) {
  EXPECT_TRUE(BuildZigzagDetour(HorizontalChain(2, 3.0), 50.0).empty());
  EXPECT_TRUE(BuildZigzagDetour(HorizontalChain(1, 3.0), 50.0).empty());
  EXPECT_TRUE(BuildZigzagDetour(DelayLineChain(), 50.0).empty());
}

TEST(BuildTwoBandRowAssignmentTest, ZeroSeparationLeavesTheChainInOneRow) {
  EXPECT_EQ(BuildTwoBandRowAssignment(6, 0),
            (std::vector<int>{0, 0, 0, 0, 0, 0}));
}

TEST(BuildTwoBandRowAssignmentTest, RaisingSeparationLiftsAlternateElements) {
  EXPECT_EQ(BuildTwoBandRowAssignment(6, 1),
            (std::vector<int>{0, 1, 0, 1, 0, 1}));
  EXPECT_EQ(BuildTwoBandRowAssignment(6, 7),
            (std::vector<int>{0, 7, 0, 7, 0, 7}));
}

TEST(BuildTwoBandRowAssignmentTest, EveryHopCrossesTheSeparation) {
  std::vector<int> rows = BuildTwoBandRowAssignment(8, 20);

  ASSERT_EQ(rows.size(), 8u);
  for (size_t index = 0; index + 1 < rows.size(); ++index) {
    EXPECT_EQ(std::abs(rows[index + 1] - rows[index]), 20);
  }
}

TEST(BuildTwoBandRowAssignmentTest, BandsStaySeparatedForLongChains) {
  std::vector<int> rows = BuildTwoBandRowAssignment(40, 40);

  for (size_t index = 0; index < rows.size(); ++index) {
    if (index % 2 == 0) {
      EXPECT_LT(rows[index], 40);
    } else {
      EXPECT_GE(rows[index], 40);
    }
  }
}

TEST(BuildTwoBandRowAssignmentTest, TheChainOccupiesExactlyTwoRows) {
  // The chain runs along the rows like the datapath it serves, so length is
  // spent on columns rather than on climbing rows.
  std::vector<int> rows = BuildTwoBandRowAssignment(40, 40);

  std::set<int> distinct(rows.begin(), rows.end());
  EXPECT_EQ(distinct, (std::set<int>{0, 40}));
}

TEST(BuildRowBandTargetsTest, ElementsAdvanceAlongTheRow) {
  DelayLineChain chain;
  for (int index = 0; index < 6; ++index)
    chain.nodes.push_back({index, 100.0, 50.0});

  std::vector<DetourTarget> targets = BuildRowBandTargets(
      chain, BuildTwoBandRowAssignment(6, 3), 2.7, 2.4);

  ASSERT_EQ(targets.size(), 6u);
  for (size_t index = 0; index < targets.size(); ++index) {
    EXPECT_DOUBLE_EQ(targets[index].x, 100.0 + 2.4 * index);
    EXPECT_DOUBLE_EQ(targets[index].y, (index % 2 == 1) ? 50.0 + 3 * 2.7 : 50.0);
  }
}

TEST(BuildInterleavedRowBandTargetsTest, ReturnPathInterleavesIntoTheSameRows) {
  DelayLineChain chain;
  for (int index = 0; index < 12; ++index)
    chain.nodes.push_back({index, 0.0, 0.0});

  std::vector<DetourTarget> targets =
      BuildInterleavedRowBandTargets(chain, 1, 2.7, 2.4);

  ASSERT_EQ(targets.size(), 12u);
  // col:      0    1    2    3    4    5
  // top:      1   11    3    9    5    7      (chain positions 0,10,2,8,4,6)
  // bottom:  12    2   10    4    8    6      (chain positions 11,1,9,3,7,5)
  const int expected_column[12] = {0, 1, 2, 3, 4, 5, 5, 4, 3, 2, 1, 0};
  for (int index = 0; index < 12; ++index) {
    EXPECT_DOUBLE_EQ(targets[index].x, expected_column[index] * 2.4);
    EXPECT_DOUBLE_EQ(targets[index].y, (index % 2 == 1) ? 2.7 : 0.0);
  }
}

TEST(BuildInterleavedRowBandTargetsTest, OutputReturnsToTheInputSide) {
  DelayLineChain chain;
  for (int index = 0; index < 10; ++index)
    chain.nodes.push_back({index, 0.0, 0.0});

  std::vector<DetourTarget> targets =
      BuildInterleavedRowBandTargets(chain, 3, 2.7, 2.4);

  ASSERT_EQ(targets.size(), 10u);
  EXPECT_DOUBLE_EQ(targets.front().x, targets.back().x);
}

TEST(BuildInterleavedRowBandTargetsTest, FoldingHalvesTheWidth) {
  DelayLineChain chain;
  for (int index = 0; index < 40; ++index)
    chain.nodes.push_back({index, 0.0, 0.0});

  std::vector<DetourTarget> targets =
      BuildInterleavedRowBandTargets(chain, 5, 2.7, 2.4);

  double widest = 0.0;
  for (const DetourTarget &target : targets)
    widest = std::max(widest, target.x);
  // 40 elements occupy 20 columns, not 40.
  EXPECT_DOUBLE_EQ(widest, 19 * 2.4);
}

TEST(BuildInterleavedRowBandTargetsTest, FoldingAddsNoDelayOnlySeparationDoes) {
  DelayLineChain chain;
  for (int index = 0; index < 8; ++index)
    chain.nodes.push_back({index, 0.0, 0.0});

  // Every hop crosses exactly the separation, whichever limb it is on.
  for (int separation : {1, 4, 9}) {
    std::vector<DetourTarget> targets =
        BuildInterleavedRowBandTargets(chain, separation, 2.7, 2.4);
    ASSERT_EQ(targets.size(), 8u);
    for (size_t index = 0; index + 1 < targets.size(); ++index) {
      EXPECT_DOUBLE_EQ(std::abs(targets[index + 1].y - targets[index].y),
                       separation * 2.7);
    }
  }
}

TEST(BuildInterleavedRowBandTargetsTest, ZeroSeparationStillUsesTwoRows) {
  // Folding puts elements i and count-1-i in one column, so collapsing the two
  // rows would stack them exactly on top of each other.
  DelayLineChain chain;
  for (int index = 0; index < 6; ++index)
    chain.nodes.push_back({index, 0.0, 0.0});

  std::vector<DetourTarget> targets =
      BuildInterleavedRowBandTargets(chain, 0, 2.7, 2.4);

  ASSERT_EQ(targets.size(), 6u);
  for (size_t a = 0; a < targets.size(); ++a) {
    for (size_t b = a + 1; b < targets.size(); ++b) {
      EXPECT_FALSE(targets[a].x == targets[b].x && targets[a].y == targets[b].y)
          << "elements " << a << " and " << b << " coincide";
    }
  }
}

TEST(BuildInterleavedRowBandTargetsTest, DegenerateInputsAreEmpty) {
  EXPECT_TRUE(BuildInterleavedRowBandTargets(DelayLineChain(), 3, 2.7, 2.4).empty());
  DelayLineChain chain;
  chain.nodes.push_back({0, 0.0, 0.0});
  EXPECT_TRUE(BuildInterleavedRowBandTargets(chain, 3, 0.0, 2.4).empty());
  EXPECT_TRUE(BuildInterleavedRowBandTargets(chain, 3, 2.7, 0.0).empty());
}

TEST(BuildTwoBandRowAssignmentTest, SeparationScalesTheHopMonotonically) {
  int previous = 0;
  for (int separation : {2, 5, 10, 40}) {
    std::vector<int> rows = BuildTwoBandRowAssignment(8, separation);
    int span = *std::max_element(rows.begin(), rows.end()) -
               *std::min_element(rows.begin(), rows.end());
    EXPECT_GT(span, previous);
    previous = span;
  }
}

TEST(BuildTwoBandRowAssignmentTest, DegenerateCountsAreEmpty) {
  EXPECT_TRUE(BuildTwoBandRowAssignment(0, 10).empty());
  EXPECT_TRUE(BuildTwoBandRowAssignment(-3, 10).empty());
}

TEST(BuildTwoBandRowAssignmentTest, NegativeSeparationDegradesToOneRow) {
  EXPECT_EQ(BuildTwoBandRowAssignment(4, -5), (std::vector<int>{0, 0, 0, 0}));
}

TEST(BuildZigzagDetourTest, NonPositiveAmplitudeIsNotADetour) {
  EXPECT_TRUE(BuildZigzagDetour(HorizontalChain(8, 3.0), 0.0).empty());
  EXPECT_TRUE(BuildZigzagDetour(HorizontalChain(8, 3.0), -10.0).empty());
}


// --- trick 3: the column permutation -----------------------------------------

TEST(NearestCoprimeStrideTest, ReducesToAUsableStride) {
  // 4 shares a factor with 12, so it would fold four positions onto one column.
  EXPECT_EQ(NearestCoprimeStride(4, 12), 1);
  EXPECT_EQ(NearestCoprimeStride(5, 12), 5);
  EXPECT_EQ(NearestCoprimeStride(7, 12), 7);
  // A stride at or past the column count wraps to itself; clamp below it.
  EXPECT_EQ(NearestCoprimeStride(99, 12), 11);
  EXPECT_EQ(NearestCoprimeStride(0, 12), 1);
  // One column admits only the identity.
  EXPECT_EQ(NearestCoprimeStride(5, 1), 1);
}

// The permutation must not cost any element its own place: every column is
// still used exactly as often as before, or elements would be stacked.
TEST(BuildInterleavedRowBandTargetsTest, StrideIsAPermutationOfColumns) {
  const DelayLineChain chain = HorizontalChain(24, 1.0);
  for (int stride : {1, 5, 7, 11}) {
    const std::vector<DetourTarget> targets =
        BuildInterleavedRowBandTargets(chain, 3, 10.0, 2.0, stride);
    ASSERT_EQ(targets.size(), 24u);
    std::map<std::pair<double, double>, int> occupancy;
    for (const DetourTarget &target : targets) {
      ++occupancy[{target.x, target.y}];
    }
    for (const auto &entry : occupancy) {
      EXPECT_EQ(entry.second, 1) << "stride " << stride << " stacks elements";
    }
    EXPECT_EQ(occupancy.size(), 24u) << "stride " << stride;
  }
}

// The point of the trick: consecutive elements move further apart in x, which
// is length bought without spending any vertical extent.
TEST(BuildInterleavedRowBandTargetsTest, StrideLengthensTheHorizontalHop) {
  const DelayLineChain chain = HorizontalChain(24, 1.0);
  double previous_hop = 0.0;
  for (int stride : {1, 5, 7}) {
    const std::vector<DetourTarget> targets =
        BuildInterleavedRowBandTargets(chain, 3, 10.0, 2.0, stride);
    const double hop = std::fabs(targets[1].x - targets[0].x);
    EXPECT_GT(hop, previous_hop) << "stride " << stride;
    previous_hop = hop;
  }
}

// Stride changes the order columns are visited, never the vertical geometry
// that separation owns; the two knobs have to stay independent.
TEST(BuildInterleavedRowBandTargetsTest, StrideLeavesRowsAlone) {
  const DelayLineChain chain = HorizontalChain(16, 1.0);
  const std::vector<DetourTarget> plain =
      BuildInterleavedRowBandTargets(chain, 4, 10.0, 2.0, 1);
  const std::vector<DetourTarget> permuted =
      BuildInterleavedRowBandTargets(chain, 4, 10.0, 2.0, 3);
  ASSERT_EQ(plain.size(), permuted.size());
  for (size_t index = 0; index < plain.size(); ++index) {
    EXPECT_DOUBLE_EQ(plain[index].y, permuted[index].y);
  }
}

// Trick 2 must survive trick 3: the fold pairs element i with count-1-i in one
// column, and permuting positions must keep that pairing intact.
TEST(BuildInterleavedRowBandTargetsTest, StridePreservesTheFold) {
  const DelayLineChain chain = HorizontalChain(20, 1.0);
  const std::vector<DetourTarget> targets =
      BuildInterleavedRowBandTargets(chain, 2, 10.0, 2.0, 3);
  ASSERT_EQ(targets.size(), 20u);
  for (size_t index = 0; index < targets.size(); ++index) {
    EXPECT_DOUBLE_EQ(targets[index].x, targets[targets.size() - 1 - index].x)
        << "fold broken at " << index;
  }
}

TEST(MaxHopStrideTest, DegenerateColumnCountsAdmitOnlyTheIdentity) {
  EXPECT_EQ(MaxHopStride(1), 1);
  EXPECT_EQ(MaxHopStride(2), 1);
}

TEST(MaxHopStrideTest, ResultIsAlwaysUsable) {
  for (int columns = 1; columns <= 128; ++columns) {
    const int stride = MaxHopStride(columns);
    EXPECT_GE(stride, 1) << "columns " << columns;
    EXPECT_LT(stride, std::max(columns, 2)) << "columns " << columns;
    if (columns > 1) {
      EXPECT_EQ(std::gcd(stride, columns), 1) << "columns " << columns;
    }
  }
}

namespace {
/** Total width a chain traverses, which is the length a stride actually buys. */
double TotalTravel(const std::vector<DetourTarget> &targets) {
  double travel = 0.0;
  for (size_t index = 1; index < targets.size(); ++index) {
    travel += std::fabs(targets[index].x - targets[index - 1].x);
  }
  return travel;
}
} // namespace

// The automatic stride must beat every hand-picked one on total travel, since
// maximizing that is the only reason to offer it. A large stride makes one long
// hop and then short ones, so this is not the same as maximizing the first hop.
TEST(BuildInterleavedRowBandTargetsTest, AutomaticStrideMaximizesTotalTravel) {
  for (int count : {14, 24, 190}) {
    const DelayLineChain chain = HorizontalChain(count, 1.0);
    const double automatic = TotalTravel(
        BuildInterleavedRowBandTargets(chain, 3, 10.0, 2.0, 0));
    for (int stride = 1; stride < (count + 1) / 2; ++stride) {
      EXPECT_LE(TotalTravel(BuildInterleavedRowBandTargets(chain, 3, 10.0, 2.0,
                                                           stride)),
                automatic)
          << "count " << count << " stride " << stride;
    }
  }
}

// Trick 3 has to be worth using: permuting must buy strictly more travel than
// the unpermuted layout on a chain long enough to have room for it.
TEST(BuildInterleavedRowBandTargetsTest, PermutingBuysTravelOverTheIdentity) {
  const DelayLineChain chain = HorizontalChain(190, 1.0);
  const double plain =
      TotalTravel(BuildInterleavedRowBandTargets(chain, 3, 10.0, 2.0, 1));
  const double permuted =
      TotalTravel(BuildInterleavedRowBandTargets(chain, 3, 10.0, 2.0, 0));
  EXPECT_GT(permuted, 3.0 * plain);
}

} // namespace dali
