/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 * SPDX-License-Identifier: GPL-2.0-or-later
 ******************************************************************************/
#include "dali/placer/consensus_placer/consensus_cell_regions.h"
#include "dali/placer/consensus_placer/consensus_placer.h"
#include "dali/placer/well_legalizer/gridded_row_legalizer.h"
#include "dali/placer/well_legalizer/multi_region_placement_validator.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace dali {
namespace {
class ConsensusPlacerTest : public testing::Test {
protected:
  void SetUp() override {
    circuit.SetManufacturingGrid(.001);
    circuit.SetUnitsDistanceMicrons(1000);
    circuit.SetGridValue(.45, .3);
    circuit.SetDieArea(0, 0, 45000, 30000);
    circuit.ReserveSpaceForDesignImp(3, 0, 2);
    auto *macro = circuit.AddMacro("cell", 1.8, 3);
    macro->AddWellRect(false, 0, 0, 4, 4);
    macro->AddWellRect(true, 0, 4, 4, 10);
    circuit.AddMacroPin(macro, "p", true)->SetOffset(2, 2);
    circuit.AddComponent("a", "cell", 10, 20, PLACED);
    circuit.AddComponent("b", "cell", 30, 20, PLACED);
    circuit.AddComponent("anchor", "cell", 45, 70, FIXED);
    circuit.AddNet("a", 2);
    circuit.AddComponentPinToNet("a", "p", "a");
    circuit.AddComponentPinToNet("anchor", "p", "a");
    circuit.AddNet("b", 2);
    circuit.AddComponentPinToNet("b", "p", "b");
    circuit.AddComponentPinToNet("anchor", "p", "b");
    columns.resize(1);
    auto &col = columns[0];
    col.lx_ = 0;
    col.width_ = 100;
    col.stripe_list_.resize(1);
    auto &stripe = col.stripe_list_[0];
    stripe.lx_ = 0;
    stripe.ly_ = 0;
    stripe.width_ = 100;
    stripe.height_ = 100;
    stripe.gridded_rows_.resize(1);
    auto &row = stripe.gridded_rows_[0];
    row.SetLLX(0);
    row.SetWidth(100);
    row.SetLLY(20);
    row.UpdateWellHeightUpward(4, 6);
    row.AddComponent(circuit.GetComponentPtr("a"));
    row.AddComponent(circuit.GetComponentPtr("b"));
    row.SetOrient(true);
    placer.SetCircuit(&circuit);
    placer.SetRows(&columns, ConsensusRowModel::kGridded);
    options.time_limit_seconds = 1;
    options.stagnant_checkpoints = 0;
    placer.SetOptions(options);
  }
  Circuit circuit;
  std::vector<StripeColumn> columns;
  ConsensusPlacer placer;
  ConsensusPlacerOptions options;
};

TEST_F(ConsensusPlacerTest, RegionPhysicalChecksUseRegionMembership) {
  auto &row = columns[0].stripe_list_[0].gridded_rows_[0];
  for (auto *cell : row.Components())
    row.AddComponentRegion(cell, 0, true);
  row.Components().clear();
  ASSERT_EQ(ValidateMultiRegionPlacement(circuit, columns).violations(), 0u);
  GriddedPlacementValidationConfig physical;
  physical.expect_well_taps = true;
  EXPECT_GT(ValidateMultiRegionPlacement(circuit, columns, physical)
                .physical.missing_well_tap_count,
            0u);
  // A foreign/null entry is rejected without reaching physical dereferences.
  row.ComponentRegions().push_back({nullptr, 0});
  EXPECT_GT(
      ValidateMultiRegionPlacement(circuit, columns, physical).violations(),
      0u);
}

TEST_F(ConsensusPlacerTest,
       BoundedInitializerRejectsCapacityWithoutAuxMutation) {
  auto &s = columns[0].stripe_list_[0];
  s.component_ptrs_vec_ = {circuit.GetComponentPtr("a"),
                           circuit.GetComponentPtr("b")};
  auto *aux = circuit.GetComponentPtr("a")->AuxPtr();
  s.height_ = 5;
  GriddedRowLegalizer initializer;
  initializer.SetCircuit(&circuit);
  EXPECT_FALSE(initializer.InitializeAssignedRows(columns, 4, 6, 3, 3));
  EXPECT_EQ(circuit.GetComponentPtr("a")->AuxPtr(), aux);
  s.height_ = 100;
  s.component_ptrs_vec_
      .clear(); // empty narrow stripes are legal, no rows needed.
  s.width_ = 2;
  EXPECT_TRUE(initializer.InitializeAssignedRows(columns, 4, 6, 3, 3));
  EXPECT_TRUE(s.gridded_rows_.empty());
}

TEST(GriddedAssignmentRepairTest, CompleteRowBudgetMovesOwnersOutOfSlivers) {
  Circuit c;
  c.SetManufacturingGrid(1);
  c.SetUnitsDistanceMicrons(1);
  c.SetGridValue(1, 1);
  c.SetDieArea(0, 0, 20, 100);
  c.ReserveSpaceForDesignImp(5, 0, 0);
  auto *m = c.AddMacro("cell", 4, 10);
  m->AddWellRect(false, 0, 0, 4, 4);
  m->AddWellRect(true, 0, 4, 4, 10);
  for (int i = 0; i < 5; ++i)
    c.AddComponent("c" + std::to_string(i), "cell", 0, 0, PLACED);
  std::vector<StripeColumn> columns(1);
  columns[0].lx_ = 0;
  columns[0].width_ = 20;
  columns[0].stripe_list_.resize(2);
  auto &a = columns[0].stripe_list_[0];
  a.lx_ = 0;
  a.ly_ = 0;
  a.width_ = 10;
  a.height_ = 30;
  auto &b = columns[0].stripe_list_[1];
  b.lx_ = 0;
  b.ly_ = 40;
  b.width_ = 20;
  b.height_ = 60;
  for (auto &cell : c.Components())
    a.component_ptrs_vec_.push_back(&cell);
  GriddedRowLegalizer init;
  init.SetCircuit(&c);
  std::size_t moved = 0;
  ASSERT_TRUE(init.InitializeAssignedRows(columns, 5, 7, 1, 1, &moved));
  EXPECT_EQ(moved, 1u);
  EXPECT_EQ(a.component_ptrs_vec_.size(), 4u);
  EXPECT_EQ(b.component_ptrs_vec_.size(), 1u);
  EXPECT_EQ(a.gridded_rows_.size(), 2u);
  EXPECT_EQ(ValidateMultiRegionPlacement(c, columns).violations(), 0u);
}

TEST(GriddedAssignmentRepairTest,
     FailedAssignmentTransactionPreservesOwnersAndPoses) {
  Circuit c;
  c.SetManufacturingGrid(1);
  c.SetUnitsDistanceMicrons(1);
  c.SetGridValue(1, 1);
  c.SetDieArea(0, 0, 20, 100);
  c.ReserveSpaceForDesignImp(2, 0, 0);
  auto *m = c.AddMacro("tall", 4, 40);
  for (int r = 0; r < 4; ++r) {
    if (r % 2 == 0) {
      m->AddWellRect(false, 0, r * 10, 4, r * 10 + 4);
      m->AddWellRect(true, 0, r * 10 + 4, 4, r * 10 + 10);
    } else {
      m->AddWellRect(true, 0, r * 10, 4, r * 10 + 6);
      m->AddWellRect(false, 0, r * 10 + 6, 4, r * 10 + 10);
    }
  }
  c.AddComponent("a", "tall", 1, 2, PLACED);
  c.AddComponent("b", "tall", 5, 3, PLACED);
  std::vector<StripeColumn> columns(1);
  columns[0].lx_ = 0;
  columns[0].width_ = 20;
  columns[0].stripe_list_.resize(2);
  auto &a = columns[0].stripe_list_[0];
  a.lx_ = 0;
  a.ly_ = 0;
  a.width_ = 10;
  a.height_ = 19;
  auto &b = columns[0].stripe_list_[1];
  b.lx_ = 0;
  b.ly_ = 40;
  b.width_ = 6;
  b.height_ = 48;
  for (auto &cell : c.Components())
    a.component_ptrs_vec_.push_back(&cell);
  auto owners = a.component_ptrs_vec_;
  GriddedRowLegalizer init;
  init.SetCircuit(&c);
  std::size_t moved = 99;
  EXPECT_FALSE(init.InitializeAssignedRows(columns, 5, 7, 1, 1, &moved));
  EXPECT_EQ(moved, 0u);
  EXPECT_EQ(a.component_ptrs_vec_, owners);
  EXPECT_TRUE(b.component_ptrs_vec_.empty());
  EXPECT_DOUBLE_EQ(c.Components()[0].LLX(), 1);
  EXPECT_DOUBLE_EQ(c.Components()[0].LLY(), 2);
  EXPECT_DOUBLE_EQ(c.Components()[1].LLX(), 5);
  EXPECT_DOUBLE_EQ(c.Components()[1].LLY(), 3);
}

TEST(GriddedAssignmentRepairTest, SpillsAnOwnerBlockedByFragmentedTallCopies) {
  Circuit c;
  c.SetManufacturingGrid(1);
  c.SetUnitsDistanceMicrons(1);
  c.SetGridValue(1, 1);
  c.SetDieArea(0, 0, 16, 60);
  c.ReserveSpaceForDesignImp(5, 0, 0);
  auto *tall = c.AddMacro("tall", 2, 20);
  tall->AddWellRect(false, 0, 0, 2, 4);
  tall->AddWellRect(true, 0, 4, 2, 10);
  tall->AddWellRect(true, 0, 10, 2, 16);
  tall->AddWellRect(false, 0, 16, 2, 20);
  for (int w : {4, 6, 8}) {
    auto *m = c.AddMacro("s" + std::to_string(w), w, 10);
    m->AddWellRect(false, 0, 0, w, 4);
    m->AddWellRect(true, 0, 4, w, 10);
  }
  c.AddComponent("t", "tall", 7, 0, PLACED);
  c.AddComponent("a", "s6", 1, 0, PLACED);
  c.AddComponent("b", "s6", 9, 0, PLACED);
  c.AddComponent("c", "s8", 0, 10, PLACED);
  c.AddComponent("d", "s4", 0, 10, PLACED);
  std::vector<StripeColumn> columns(1);
  columns[0].lx_ = 0;
  columns[0].width_ = 16;
  columns[0].stripe_list_.resize(2);
  auto &a = columns[0].stripe_list_[0];
  a.lx_ = 0;
  a.ly_ = 0;
  a.width_ = 16;
  a.height_ = 24;
  auto &b = columns[0].stripe_list_[1];
  b.lx_ = 0;
  b.ly_ = 30;
  b.width_ = 16;
  b.height_ = 24;
  for (auto &cell : c.Components())
    a.component_ptrs_vec_.push_back(&cell);
  GriddedRowLegalizer init;
  init.SetCircuit(&c);
  std::size_t moved = 0;
  ASSERT_TRUE(init.InitializeAssignedRows(columns, 5, 7, 1, 1, &moved));
  EXPECT_EQ(moved, 1u);
  ASSERT_EQ(b.component_ptrs_vec_.size(), 1u);
  EXPECT_EQ(b.component_ptrs_vec_[0]->Name(), "c");
  EXPECT_EQ(ValidateMultiRegionPlacement(c, columns).violations(), 0u);
}

TEST_F(ConsensusPlacerTest, ImprovesNonunitGridAndPreservesFixedPose) {
  const double before = circuit.WeightedHPWLX();
  ASSERT_TRUE(placer.StartPlacement()) << placer.Report().error;
  const auto &report = placer.Report();
  EXPECT_TRUE(report.committed);
  EXPECT_DOUBLE_EQ(report.source_hpwl_x_um, before);
  EXPECT_LT(report.hpwl_x_um, before);
  EXPECT_EQ(report.owners_considered, 2u);
  EXPECT_EQ(report.owners_updated, 2u);
  EXPECT_EQ(circuit.GetComponentPtr("anchor")->LLX(), 45);
  EXPECT_EQ(circuit.GetComponentPtr("a")->LLY(), 20);
  EXPECT_TRUE(
      GriddedPlacementValidator(&circuit, &columns).Validate().IsLegal());
}

TEST_F(ConsensusPlacerTest, ZeroBudgetCancellationAndIterationCapRetainSource) {
  options.time_limit_seconds = 0;
  placer.SetOptions(options);
  ASSERT_TRUE(placer.StartPlacement());
  EXPECT_EQ(placer.Report().stop_reason, "time_limit");
  EXPECT_EQ(placer.Report().owners_updated, 0u);
  EXPECT_EQ(placer.Report().owners_changed, 0u);
  EXPECT_EQ(circuit.GetComponentPtr("a")->LLX(), 10);
  options.time_limit_seconds = 1;
  options.max_iterations = 0;
  placer.SetOptions(options);
  ASSERT_TRUE(placer.StartPlacement());
  EXPECT_EQ(placer.Report().iterations, 0);
  options.max_iterations = 128;
  options.cancelled = [] { return true; };
  placer.SetOptions(options);
  ASSERT_TRUE(placer.StartPlacement());
  EXPECT_EQ(placer.Report().stop_reason, "cancelled");
  EXPECT_EQ(placer.Report().owners_changed, 0u);
}

TEST_F(ConsensusPlacerTest, RejectedAndExceptionalChecksRestoreSource) {
  int calls = 0;
  placer.SetAdditionalPlacementCheck([&] { return ++calls == 1; });
  ASSERT_TRUE(placer.StartPlacement());
  EXPECT_FALSE(placer.Report().committed);
  EXPECT_EQ(placer.Report().owners_changed, 0u);
  EXPECT_EQ(circuit.GetComponentPtr("a")->LLX(), 10);
  EXPECT_EQ(circuit.GetComponentPtr("b")->LLX(), 30);
  calls = 0;
  placer.SetAdditionalPlacementCheck([&] {
    if (++calls == 2)
      throw std::runtime_error("test checker failure");
    return true;
  });
  EXPECT_FALSE(placer.StartPlacement());
  EXPECT_EQ(circuit.GetComponentPtr("a")->LLX(), 10);
  EXPECT_EQ(circuit.GetComponentPtr("b")->LLX(), 30);
}

TEST_F(ConsensusPlacerTest, BoundsMovementAndRejectsInvalidInputsWithoutAbort) {
  options.max_move_sites = 2;
  placer.SetOptions(options);
  ASSERT_TRUE(placer.StartPlacement());
  EXPECT_LE(std::abs(circuit.GetComponentPtr("a")->LLX() - 10), 2);
  EXPECT_LE(std::abs(circuit.GetComponentPtr("b")->LLX() - 30), 2);
  const double saved = circuit.GetComponentPtr("a")->LLX();
  options.rho = 0;
  placer.SetOptions(options);
  EXPECT_FALSE(placer.StartPlacement());
  EXPECT_DOUBLE_EQ(circuit.GetComponentPtr("a")->LLX(), saved);
  columns[0].stripe_list_[0].gridded_rows_[0].Components().push_back(nullptr);
  EXPECT_FALSE(placer.StartPlacement());
  ConsensusPlacer missing;
  EXPECT_FALSE(missing.StartPlacement());
}

TEST_F(ConsensusPlacerTest,
       CompleteCellImportRejectsLaterErrorsBeforeMutation) {
  const auto path =
      std::filesystem::temp_directory_path() /
      ("dali-consensus-cell-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()) +
       ".cell");
  const std::string valid =
      "MACRO cell\n LAYER pwell ;\n RECT 0 0 1.8 1.5 ;\n END\n LAYER nwell ;\n "
      "RECT 0 1.5 1.8 3 ;\n END\nEND cell\n";
  {
    std::ofstream out(path);
    out << valid << "MACRO unknown_master\n";
  }
  std::string error;
  auto *macro = circuit.GetMacroPtr("cell");
  auto *pin = circuit.Nets()[0].ComponentPins()[0].PinPtr();
  EXPECT_FALSE(
      LoadConsensusCellRegions(circuit, path.string(), nullptr, &error));
  EXPECT_FALSE(error.empty());
  EXPECT_EQ(macro->PwellRect(0).URY(), 4);
  {
    std::ofstream out(path);
    out << "MACRO cell\n LAYER pwell ;\n RECT 0 0.3 1.8 1.5 ;\n"
           " LAYER nwell ;\n RECT 0 1.5 1.8 3 ;\nEND cell\n";
  }
  EXPECT_FALSE(
      LoadConsensusCellRegions(circuit, path.string(), nullptr, &error));
  EXPECT_EQ(macro->PwellRect(0).URY(), 4);
  {
    std::ofstream out(path);
    out << valid;
  }
  EXPECT_TRUE(LoadConsensusCellRegions(circuit, path.string(), nullptr, &error))
      << error;
  EXPECT_EQ(macro->PwellRect(0).URY(), 5);
  EXPECT_EQ(circuit.Nets()[0].ComponentPins()[0].PinPtr(), pin);
  EXPECT_EQ(circuit.GetComponentPtr("a")->MacroPtr(), macro);
  std::filesystem::remove(path);
}

TEST(ConsensusMultiRegionTest,
     ValidatesAsymmetricFlippedStretchAndMovementCap) {
  Circuit c;
  c.SetManufacturingGrid(1);
  c.SetUnitsDistanceMicrons(1);
  c.SetGridValue(1, 1);
  c.SetDieArea(0, 0, 80, 80);
  c.ReserveSpaceForDesignImp(3, 0, 2);
  for (int d : {2, 3}) {
    auto *m = c.AddMacro("d" + std::to_string(d), 4, 10 * d);
    for (int r = 0; r < d; ++r) {
      if (r % 2 == 0) {
        m->AddWellRect(false, 0, r * 10, 4, r * 10 + 4);
        m->AddWellRect(true, 0, r * 10 + 4, 4, r * 10 + 10);
      } else {
        m->AddWellRect(true, 0, r * 10, 4, r * 10 + 6);
        m->AddWellRect(false, 0, r * 10 + 6, 4, r * 10 + 10);
      }
    }
    c.AddMacroPin(m, "p", true)->SetOffset(2, 2);
  }
  auto *io = c.AddMacro("io", 1, 1);
  c.AddMacroPin(io, "p", true)->SetOffset(.5, .5);
  c.AddComponent("n", "d2", 5, 0, PLACED);
  c.AddComponent("fs", "d3", 35, 0, PLACED, FS);
  c.AddComponent("io", "io", 20, 70, FIXED);
  std::vector<StripeColumn> cols(1);
  auto &col = cols[0];
  col.lx_ = 0;
  col.width_ = 80;
  col.stripe_list_.resize(1);
  auto &s = col.stripe_list_[0];
  s.lx_ = 0;
  s.ly_ = 0;
  s.width_ = 80;
  s.height_ = 80;
  s.is_bottom_up_ = true;
  s.gridded_rows_.resize(5);
  int bottom = 0;
  for (int r = 0; r < 5; ++r) {
    auto &row = s.gridded_rows_[r];
    row.SetLLX(0);
    row.SetWidth(80);
    row.SetLLY(bottom);
    row.SetOrient(r % 2 == 0);
    row.UpdateWellHeightUpward(5 + r, 7 + r);
    bottom += row.Height();
  }
  auto *n = c.GetComponentPtr("n");
  auto *fs = c.GetComponentPtr("fs");
  for (int r = 0; r < 2; ++r)
    s.gridded_rows_[r].AddComponentRegion(n, r, true);
  for (int r = 0; r < 3; ++r)
    s.gridded_rows_[r + 1].AddComponentRegion(fs, r, true);
  s.UpdateComponentStretchLength();
  s.UpdateComponentYLocation();
  ASSERT_EQ(n->StretchLengths(), std::vector<int>({3}));
  ASSERT_EQ(fs->StretchLengths(), std::vector<int>({7, 5}));
  c.AddNet("a", 2);
  c.AddComponentPinToNet("n", "p", "a");
  c.AddComponentPinToNet("io", "p", "a");
  c.AddNet("b", 2);
  c.AddComponentPinToNet("fs", "p", "b");
  c.AddComponentPinToNet("io", "p", "b");
  ConsensusPlacer placer;
  placer.SetCircuit(&c);
  placer.SetRows(&cols, ConsensusRowModel::kMultiRegion);
  fs->SetLLX(6);
  EXPECT_TRUE(s.IsStripeLegal());
  EXPECT_FALSE(placer.StartPlacement());
  fs->SetLLX(35);
  int checks = 0;
  placer.SetAdditionalPlacementCheck([&] { return ++checks == 1; });
  ASSERT_TRUE(placer.StartPlacement()) << placer.Report().error;
  EXPECT_FALSE(placer.Report().committed);
  EXPECT_DOUBLE_EQ(n->LLX(), 5);
  EXPECT_DOUBLE_EQ(fs->LLX(), 35);
  placer.SetAdditionalPlacementCheck({});
  ConsensusPlacerOptions options;
  options.max_move_sites = 2;
  options.displacement_weight = .1;
  placer.SetOptions(options);
  ASSERT_TRUE(placer.StartPlacement()) << placer.Report().error;
  EXPECT_TRUE(placer.Report().committed);
  EXPECT_LE(std::abs(n->LLX() - 5), 2);
  EXPECT_LE(std::abs(fs->LLX() - 35), 2);
  EXPECT_EQ(fs->Orient(), FS);
  EXPECT_EQ(fs->StretchLengths(), std::vector<int>({7, 5}));
  EXPECT_EQ(placer.Report().row_copies, 5u);
}

class ConsensusAdderTest : public testing::TestWithParam<std::string> {};
TEST_P(ConsensusAdderTest, ReloadsAndRefinesThroughPublicMultiRegionPlacer) {
  const auto root = std::filesystem::path(CONSENSUS_TEST_DATA) / GetParam();
  phydb::PhyDB db;
  db.ReadLef((root / "cells.lef").string());
  db.ReadDef((root / "before.def").string());
  db.ReadCell((root / "cells.cell").string());
  Circuit circuit;
  circuit.InitializeFromPhyDB(&db);
  std::size_t restored = 0;
  std::string error;
  ASSERT_TRUE(LoadConsensusCellRegions(circuit, (root / "cells.cell").string(),
                                       &restored, &error))
      << error;
  EXPECT_EQ(restored, GetParam() == "mixed" ? 19u : 0u);
  std::vector<StripeColumn> cols(1);
  auto &col = cols[0];
  col.lx_ = 4;
  col.width_ = 108;
  col.stripe_list_.resize(1);
  auto &stripe = col.stripe_list_[0];
  stripe.lx_ = 4;
  stripe.ly_ = 0;
  stripe.width_ = 108;
  stripe.is_bottom_up_ = true;
  std::ifstream rows(root / "rows.tsv");
  std::string line;
  std::getline(rows, line);
  while (std::getline(rows, line)) {
    int id, lx, ux, ly, uy, orient, ph, nh;
    std::istringstream in(line);
    in >> id >> lx >> ux >> ly >> uy >> orient >> ph >> nh;
    ASSERT_TRUE(bool(in));
    stripe.gridded_rows_.emplace_back();
    auto &row = stripe.gridded_rows_.back();
    row.SetLLX(lx);
    row.SetWidth(ux - lx);
    row.SetLLY(ly);
    row.SetOrient(orient);
    row.UpdateWellHeightUpward(ph, nh);
    stripe.height_ = uy;
  }
  std::ifstream regions(root / "regions.tsv");
  std::getline(regions, line);
  while (std::getline(regions, line)) {
    std::string owner;
    int region, row;
    std::istringstream in(line);
    in >> owner >> region >> row;
    ASSERT_TRUE(bool(in));
    stripe.gridded_rows_.at(row).AddComponentRegion(
        circuit.GetComponentPtr(owner), region, true);
  }
  stripe.UpdateComponentStretchLength();
  ConsensusPlacer placer;
  placer.SetCircuit(&circuit);
  placer.SetRows(&cols, ConsensusRowModel::kMultiRegion);
  ConsensusPlacerOptions options;
  options.time_limit_seconds = 2;
  placer.SetOptions(options);
  ASSERT_TRUE(placer.StartPlacement()) << placer.Report().error;
  EXPECT_TRUE(placer.Report().committed);
  EXPECT_EQ(placer.Report().owners_considered, 49u);
  EXPECT_EQ(placer.Report().owners_changed, 47u);
  EXPECT_NEAR(placer.Report().hpwl_x_um,
              GetParam() == "mixed" ? 1240.65 : 1212.30, 1e-7);
  EXPECT_DOUBLE_EQ(placer.Report().hpwl_y_um, placer.Report().source_hpwl_y_um);
  std::ifstream expected(root / "after.def");
  const std::regex component(
      R"(^- (g[0-9]+) physical_\S+ \+ PLACED \( (\d+) (\d+) \) N ;$)");
  while (std::getline(expected, line)) {
    std::smatch match;
    if (!std::regex_match(line, match, component))
      continue;
    EXPECT_DOUBLE_EQ(circuit.GetComponentPtr(match[1])->LLX(),
                     std::stod(match[2]) / 450);
  }
}
INSTANTIATE_TEST_SUITE_P(SavedAdders, ConsensusAdderTest,
                         testing::Values("single", "mixed"));
} // namespace
} // namespace dali
