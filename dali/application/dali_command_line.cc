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
// clang-format off
#include <stdio.h>
#include <common/config.h>
// clang-format on

#include "dali/application/dali_command_line.h"

#include <iostream>

#include "dali/common/helper.h"

namespace dali {

static bool TryGetValue(int argc, char* argv[], int* index,
                        std::string* value) {
  if (*index >= argc) {
    return false;
  }
  *value = argv[(*index)++];
  return !value->empty();
}

static bool TryParseDouble(const std::string& text, double* value) {
  try {
    size_t parsed_length = 0;
    *value = std::stod(text, &parsed_length);
    return parsed_length == text.size();
  } catch (...) {
    return false;
  }
}

static bool TryParseInt(const std::string& text, int* value) {
  try {
    size_t parsed_length = 0;
    *value = std::stoi(text, &parsed_length);
    return parsed_length == text.size();
  } catch (...) {
    return false;
  }
}

static void EnableConfigFlag(const char* config_name) {
  config_set_int(config_name, 1);
}

void ReportDaliUsage(std::ostream& output) {
  output
      // clang-format off
      << "\033[0;36m"
      << "Usage: dali\n"
      << "  -lef <file.lef>\n"
      << "  -def <file.def>\n"
      << "  -cell <file.cell>                          (optional, if provided, well placement flow will be triggered)\n"
      << "  -o/-output_name <output_name>.def          (optional, default output def file name dali_out.def)\n"
      << "  -metrics_file <file.json>                  (optional, default dali_metrics.json)\n"
      << "  -visualization_dir <dir>                   emit placement visualization snapshots\n"
      << "  -gui_debug                                 show live placement debug GUI when built with Qt\n"
      << "  -gui_pause <every_snapshot/off>            GUI pause policy, default every_snapshot\n"
      << "  -g/-grid <grid_value_x> <grid_value_y>     (optional, default metal1 and metal2 pitch values)\n"
      << "  -d/-target_density <density>               (optional, value interval (0,1], default max(space_utility, 0.7))\n"
      << "  -net_hpwl_file <file.tsv>                  write final per-net weighted HPWL metrics\n"
      << "  -disable_legalization                      optional, if this flag is present, then legalization is skipped\n"
      << "  -disable_detailed_place                    optional, skip post-legalization detailed placement\n"
      << "  -io_metal_layer                            metal layer number for I/O placement (optional, default 1 for m1)\n"
      << "  -well_legalization_mode <scavenge/strict>  determine whether the last column use unassigned space\n"
      << "  -global_initializer <keep/uniform/gaussian/monte_carlo/density_aware>\n"
      << "  -global_anchor_schedule <dali/simpl>       choose global-placement anchor pseudo-net schedule\n"
      << "  -global_grid_schedule <dali/simpl>         choose look-ahead legalization grid schedule\n"
      << "  -global_lal_expansion <symmetric/best_neighbor>\n"
      << "  -global_lal_hotspot <area/overflow/overflow_ratio>\n"
      << "  -global_lal_affine_weight <0..1>           blend between packed and affine LAL spreading, default 0.65\n"
      << "  -global_lal_macro_boundary <off/balanced/preferred>\n"
      << "  -global_min_iterations <n>                 minimum global-placement iterations, default 10\n"
      << "  -global_max_iterations <n>                 maximum global-placement iterations, default 100\n"
      << "  -enable_gridded_global_capacity            use experimental well-aware LAL capacity\n"
      << "  -enable_gridded_upper_bound_refiner        roughly legalize every gridded global-placement iteration\n"
      << "  -enable_gridded_upper_bound_balancing      minimally rebalance failed rough-legal stripes\n"
      << "  -enable_adaptive_stripe_boundaries         optimize nonuniform gridded stripe widths\n"
      << "  -disable_gridded_legalization_feedback     do not anchor the next solve to rough-legal coordinates\n"
      << "  -enable_gridded_stripe_balancing           rebalance final neighboring gridded stripes\n"
      << "  -enable_gridded_local_reorder              reorder cells within finalized gridded rows\n"
      << "  -enable_gridded_detailed_placement         run gridded global swap, vertical swap, and local reorder\n"
      << "  -gridded_detailed_max_rounds <n>           maximum gridded detailed rounds, default 6\n"
      << "  -gridded_detailed_min_relative_improvement <0..1>  convergence threshold, default 0.001\n"
      << "  -disable_gridded_vertical_swap            skip vertical swaps in gridded detailed placement\n"
      << "  -enable_gridded_row_y_optimization         shift legal row groups toward net-optimal Y regions\n"
      << "  -debug_placement_region_scale <factor>      enlarge the placement boundary for debugging, default 1\n"
      << "  -standard_cell_legalizer_cost <displacement/hpwl>  default displacement\n"
      << "  -detailed_max_rounds <n>                   detailed-placement optimization rounds, default 1\n"
      << "  -detailed_max_move_candidates <n>          optimal-region move candidates per round, default 1000\n"
      << "  -save_intermediate_result                  dump placement snapshots for visualization\n"
      << "  -num_threads <n>                           number of OpenMP threads to use\n"
      << "  -v                                         verbosity_level (optional, 0-5, default 1)\n"
      << "  -disable_log_prefix                        optional, if this flag is present, then only messages will be saved to the log file\n"
      << "(flag order does not matter)"
      << "\033[0m\n";
  // clang-format on
}

bool ParseDaliCommandLine(int argc, char* argv[],
                          DaliCommandLineOptions* options,
                          std::ostream& error_output) {
  for (int i = 1; i < argc;) {
    std::string arg(argv[i++]);
    std::string value;

    if (arg == "-lef") {
      if (!TryGetValue(argc, argv, &i, &options->lef_file_name)) {
        error_output << "Invalid input lef file!\n";
        return false;
      }
    } else if (arg == "-def") {
      if (!TryGetValue(argc, argv, &i, &options->def_file_name)) {
        error_output << "Invalid input def file!\n";
        return false;
      }
    } else if (arg == "-cell") {
      if (!TryGetValue(argc, argv, &i, &options->cell_file_name)) {
        error_output << "Invalid input cell file!\n";
        return false;
      }
    } else if (arg == "-mcell") {
      if (!TryGetValue(argc, argv, &i, &options->ignored_mcell_file_name)) {
        error_output << "Invalid input mcell file!\n";
        return false;
      }
      error_output << "Warning: -mcell is currently accepted for compatibility "
                      "but ignored\n";
    } else if (arg == "-o" || arg == "-output_name") {
      if (!TryGetValue(argc, argv, &i, &options->output_name)) {
        error_output << "Invalid output name!\n";
        return false;
      }
      config_set_string("dali.output_name", options->output_name.c_str());
    } else if (arg == "-metrics_file") {
      if (!TryGetValue(argc, argv, &i, &options->metrics_file_name)) {
        error_output << "Invalid metrics file name!\n";
        return false;
      }
    } else if (arg == "-net_hpwl_file") {
      if (!TryGetValue(argc, argv, &i, &options->net_hpwl_file_name)) {
        error_output << "Invalid net HPWL file name!\n";
        return false;
      }
    } else if (arg == "-visualization_dir") {
      if (!TryGetValue(argc, argv, &i, &options->visualization_dir)) {
        error_output << "Invalid visualization directory!\n";
        return false;
      }
      config_set_string("dali.visualization_dir",
                        options->visualization_dir.c_str());
    } else if (arg == "-gui_debug") {
      EnableConfigFlag("dali.gui_debug");
    } else if (arg == "-gui_pause") {
      if (!TryGetValue(argc, argv, &i, &value)) {
        error_output << "Invalid GUI pause policy!\n";
        return false;
      }
      if (value != "every_snapshot" && value != "off") {
        error_output << "Invalid GUI pause policy!\n";
        return false;
      }
      config_set_string("dali.gui_pause", value.c_str());
    } else if (arg == "-v") {
      if (!TryGetValue(argc, argv, &i, &value)) {
        error_output << "Invalid verbosity level!\n";
        return false;
      }
      options->verbose_level = StrToLoggingLevel(value);
    } else if (arg == "-g" || arg == "-grid") {
      std::string x_grid;
      std::string y_grid;
      if (!TryGetValue(argc, argv, &i, &x_grid) ||
          !TryGetValue(argc, argv, &i, &y_grid) ||
          !TryParseDouble(x_grid, &options->x_grid) ||
          !TryParseDouble(y_grid, &options->y_grid) || options->x_grid <= 0 ||
          options->y_grid <= 0) {
        error_output << "Invalid placement grid!\n";
        return false;
      }
    } else if (arg == "-d" || arg == "-target_density") {
      double target_density = 0;
      if (!TryGetValue(argc, argv, &i, &value) ||
          !TryParseDouble(value, &target_density) || target_density <= 0 ||
          target_density > 1) {
        error_output << "Invalid target density!\n";
        return false;
      }
      config_set_real("dali.target_density", target_density);
    } else if (arg == "-io_metal_layer" || arg == "io_metal_layer") {
      int io_metal_layer = 0;
      if (!TryGetValue(argc, argv, &i, &value) ||
          !TryParseInt(value, &io_metal_layer) || io_metal_layer <= 0) {
        error_output << "Invalid metal layer number!\n";
        return false;
      }
      config_set_int("dali.io_metal_layer", io_metal_layer - 1);
    } else if (arg == "-disable_log_prefix") {
      EnableConfigFlag("dali.disable_log_prefix");
    } else if (arg == "-well_legalization_mode") {
      if (!TryGetValue(argc, argv, &i, &value)) {
        error_output << "Invalid well legalization mode!\n";
        return false;
      }
      if (value != "scavenge" && value != "strict") {
        error_output << "Invalid well legalization mode!\n";
        return false;
      }
      config_set_string("dali.well_legalization_mode", value.c_str());
    } else if (arg == "-disable_legalization") {
      EnableConfigFlag("dali.disable_legalization");
    } else if (arg == "-disable_detailed_place") {
      EnableConfigFlag("dali.disable_detailed_place");
    } else if (arg == "-disable_global_place") {
      EnableConfigFlag("dali.disable_global_place");
    } else if (arg == "-global_initializer") {
      if (!TryGetValue(argc, argv, &i, &value)) {
        error_output << "Invalid global initializer!\n";
        return false;
      }
      if (value != "keep" && value != "uniform" && value != "gaussian" &&
          value != "monte_carlo" && value != "density_aware") {
        error_output << "Invalid global initializer!\n";
        return false;
      }
      config_set_string("dali.global_initializer", value.c_str());
    } else if (arg == "-global_anchor_schedule") {
      if (!TryGetValue(argc, argv, &i, &value)) {
        error_output << "Invalid global anchor schedule!\n";
        return false;
      }
      if (value != "dali" && value != "simpl") {
        error_output << "Invalid global anchor schedule!\n";
        return false;
      }
      config_set_string("dali.global_anchor_schedule", value.c_str());
    } else if (arg == "-global_grid_schedule") {
      if (!TryGetValue(argc, argv, &i, &value)) {
        error_output << "Invalid global grid schedule!\n";
        return false;
      }
      if (value != "dali" && value != "simpl") {
        error_output << "Invalid global grid schedule!\n";
        return false;
      }
      config_set_string("dali.global_grid_schedule", value.c_str());
    } else if (arg == "-global_lal_expansion") {
      if (!TryGetValue(argc, argv, &i, &value)) {
        error_output << "Invalid global LAL expansion mode!\n";
        return false;
      }
      if (value != "symmetric" && value != "best_neighbor") {
        error_output << "Invalid global LAL expansion mode!\n";
        return false;
      }
      config_set_string("dali.global_lal_expansion", value.c_str());
    } else if (arg == "-global_lal_hotspot") {
      if (!TryGetValue(argc, argv, &i, &value)) {
        error_output << "Invalid global LAL hotspot mode!\n";
        return false;
      }
      if (value != "area" && value != "overflow" && value != "overflow_ratio") {
        error_output << "Invalid global LAL hotspot mode!\n";
        return false;
      }
      config_set_string("dali.global_lal_hotspot", value.c_str());
    } else if (arg == "-global_lal_affine_weight") {
      double affine_weight = 0;
      if (!TryGetValue(argc, argv, &i, &value) ||
          !TryParseDouble(value, &affine_weight) || affine_weight < 0 ||
          affine_weight > 1) {
        error_output << "Invalid global LAL affine weight!\n";
        return false;
      }
      config_set_real("dali.global_lal_affine_weight", affine_weight);
    } else if (arg == "-global_lal_macro_boundary") {
      if (!TryGetValue(argc, argv, &i, &value)) {
        error_output << "Invalid global LAL macro boundary mode!\n";
        return false;
      }
      if (value != "off" && value != "balanced" && value != "preferred") {
        error_output << "Invalid global LAL macro boundary mode!\n";
        return false;
      }
      config_set_string("dali.global_lal_macro_boundary", value.c_str());
    } else if (arg == "-global_min_iterations") {
      int global_min_iterations = 0;
      if (!TryGetValue(argc, argv, &i, &value) ||
          !TryParseInt(value, &global_min_iterations) ||
          global_min_iterations < 0) {
        error_output << "Invalid global minimum iteration count!\n";
        return false;
      }
      config_set_int("dali.global_min_iterations", global_min_iterations);
    } else if (arg == "-global_max_iterations") {
      int global_max_iterations = 0;
      if (!TryGetValue(argc, argv, &i, &value) ||
          !TryParseInt(value, &global_max_iterations) ||
          global_max_iterations < 0) {
        error_output << "Invalid global maximum iteration count!\n";
        return false;
      }
      config_set_int("dali.global_max_iterations", global_max_iterations);
    } else if (arg == "-standard_cell_legalizer_cost") {
      if (!TryGetValue(argc, argv, &i, &value)) {
        error_output << "Invalid standard-cell legalizer cost mode!\n";
        return false;
      }
      if (value != "hpwl" && value != "displacement") {
        error_output << "Invalid standard-cell legalizer cost mode!\n";
        return false;
      }
      config_set_string("dali.standard_cell_legalizer_cost", value.c_str());
    } else if (arg == "-detailed_max_rounds") {
      int detailed_max_rounds = 0;
      if (!TryGetValue(argc, argv, &i, &value) ||
          !TryParseInt(value, &detailed_max_rounds) ||
          detailed_max_rounds < 0) {
        error_output << "Invalid detailed placement maximum round count!\n";
        return false;
      }
      config_set_int("dali.detailed_max_rounds", detailed_max_rounds);
    } else if (arg == "-detailed_max_move_candidates") {
      int detailed_max_move_candidates = 0;
      if (!TryGetValue(argc, argv, &i, &value) ||
          !TryParseInt(value, &detailed_max_move_candidates) ||
          detailed_max_move_candidates < 0) {
        error_output << "Invalid detailed placement move candidate count!\n";
        return false;
      }
      config_set_int("dali.detailed_max_move_candidates",
                     detailed_max_move_candidates);
    } else if (arg == "-save_intermediate_result") {
      EnableConfigFlag("dali.save_intermediate_result");
    } else if (arg == "-max_row_width") {
      double max_row_width = 0;
      if (!TryGetValue(argc, argv, &i, &value) ||
          !TryParseDouble(value, &max_row_width) || max_row_width < 0) {
        error_output << "Invalid max row width!\n";
        return false;
      }
      config_set_real("dali.max_row_width", max_row_width);
    } else if (arg == "-enable_adaptive_stripe_boundaries") {
      EnableConfigFlag("dali.enable_adaptive_stripe_boundaries");
    } else if (arg == "-disable_welltap") {
      EnableConfigFlag("dali.disable_welltap");
    } else if (arg == "-disable_cell_flip") {
      EnableConfigFlag("dali.disable_cell_flip");
    } else if (arg == "-disable_io_place") {
      EnableConfigFlag("dali.disable_io_place");
    } else if (arg == "-export_well_cluster_matlab") {
      EnableConfigFlag("dali.export_well_cluster_matlab");
    } else if (arg == "-log_file_name") {
      if (!TryGetValue(argc, argv, &i, &options->log_file_name)) {
        error_output << "Invalid name for log file!\n";
        return false;
      }
      config_set_string("dali.log_file_name", options->log_file_name.c_str());
    } else if (arg == "-num_threads") {
      int num_threads = 0;
      if (!TryGetValue(argc, argv, &i, &value) ||
          !TryParseInt(value, &num_threads) || num_threads < 1) {
        error_output << "Invalid number of threads!\n";
        return false;
      }
      config_set_int("dali.num_threads", num_threads);
    } else if (arg == "-is_standard_cell") {
      EnableConfigFlag("dali.is_standard_cell");
    } else if (arg == "-enable_filler_cell") {
      EnableConfigFlag("dali.enable_filler_cell");
    } else if (arg == "-enable_end_cap_cell") {
      EnableConfigFlag("dali.enable_end_cap_cell");
    } else if (arg == "-enable_gridded_global_capacity") {
      EnableConfigFlag("dali.enable_gridded_global_capacity");
    } else if (arg == "-enable_gridded_upper_bound_refiner") {
      EnableConfigFlag("dali.enable_gridded_upper_bound_refiner");
    } else if (arg == "-enable_gridded_upper_bound_balancing") {
      EnableConfigFlag("dali.enable_gridded_upper_bound_balancing");
    } else if (arg == "-disable_gridded_legalization_feedback") {
      EnableConfigFlag("dali.disable_gridded_legalization_feedback");
    } else if (arg == "-enable_gridded_stripe_balancing") {
      EnableConfigFlag("dali.enable_gridded_stripe_balancing");
    } else if (arg == "-enable_gridded_local_reorder") {
      EnableConfigFlag("dali.enable_gridded_local_reorder");
    } else if (arg == "-enable_gridded_detailed_placement") {
      EnableConfigFlag("dali.enable_gridded_detailed_placement");
    } else if (arg == "-gridded_detailed_max_rounds") {
      int max_rounds = 0;
      if (!TryGetValue(argc, argv, &i, &value) ||
          !TryParseInt(value, &max_rounds) || max_rounds < 0) {
        error_output << "Invalid gridded detailed maximum round count!\n";
        return false;
      }
      config_set_int("dali.gridded_detailed_max_rounds", max_rounds);
    } else if (arg == "-gridded_detailed_min_relative_improvement") {
      double min_relative_improvement = 0;
      if (!TryGetValue(argc, argv, &i, &value) ||
          !TryParseDouble(value, &min_relative_improvement) ||
          min_relative_improvement < 0 || min_relative_improvement > 1) {
        error_output << "Invalid gridded detailed convergence threshold!\n";
        return false;
      }
      config_set_real("dali.gridded_detailed_min_relative_improvement",
                      min_relative_improvement);
    } else if (arg == "-disable_gridded_vertical_swap") {
      EnableConfigFlag("dali.disable_gridded_vertical_swap");
    } else if (arg == "-enable_gridded_row_y_optimization") {
      EnableConfigFlag("dali.enable_gridded_row_y_optimization");
    } else if (arg == "-debug_placement_region_scale") {
      double scale = 0;
      if (!TryGetValue(argc, argv, &i, &value) ||
          !TryParseDouble(value, &scale) || scale < 1.0) {
        error_output << "Invalid debug placement-region scale!\n";
        return false;
      }
      config_set_real("dali.debug_placement_region_scale", scale);
    } else if (arg == "-enable_shrink_off_grid_die_area") {
      EnableConfigFlag("dali.enable_shrink_off_grid_die_area");
    } else {
      error_output << "Unknown arg: " << arg << "\n";
      return false;
    }
  }

  if (options->lef_file_name.empty() || options->def_file_name.empty()) {
    error_output << "Invalid input files!\n";
    return false;
  }
  return true;
}

}  // namespace dali
