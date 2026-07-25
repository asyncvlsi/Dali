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

/**
 * @file
 * Verify that a `.dali` recipe can own design loading and final export.
 */

#include <filesystem>
#include <fstream>
#include <string>

#include "dali/dali.h"

using namespace dali;

int main() {
  const std::filesystem::path benchmark_directory =
      std::filesystem::current_path();
  const std::filesystem::path test_directory =
      std::filesystem::temp_directory_path() / "dali_script_owned_design_flow";
  std::filesystem::remove_all(test_directory);
  std::filesystem::create_directories(test_directory);
  std::filesystem::copy_file(benchmark_directory / "ispd19_test3.input.lef",
                             test_directory / "design.lef",
                             std::filesystem::copy_options::overwrite_existing);
  std::filesystem::copy_file(benchmark_directory / "ispd19_test3.input.def",
                             test_directory / "design.def",
                             std::filesystem::copy_options::overwrite_existing);

  {
    std::ofstream script(test_directory / "flow.dali");
    script << "# All paths are relative to this recipe.\n"
           << "read-lef \"design.lef\"\n"
           << "read-def \"design.def\"\n"
           << "set is_standard_cell true\n"
           << "set disable_global_place true\n"
           << "set disable_legalization true\n"
           << "set disable_detailed_place true\n"
           << "set disable_io_place true\n"
           << "run placement\n"
           << "write-def \"results/placed\"\n";
  }

  std::filesystem::current_path(test_directory);
  phydb::PhyDB phy_db;
  Dali dali(&phy_db, severity::info);
  const bool command_success = dali.RunCommandFile("flow.dali");
  const bool design_loaded = dali.HasInputDesign();
  const bool placement_exported = dali.HasExplicitPlacementExport();
  const bool output_exists =
      std::filesystem::is_regular_file("results/placed.def");
  const bool phydb_output_exists =
      std::filesystem::is_regular_file("phydb.def");
  dali.Close();

  std::filesystem::current_path(benchmark_directory);
  std::filesystem::remove_all(test_directory);
  return command_success && design_loaded && placement_exported &&
                 output_exists && phydb_output_exists
             ? 0
             : 1;
}
