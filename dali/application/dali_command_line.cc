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
namespace {

bool TryGetValue(int argc, char* argv[], int* index, std::string* value) {
  if (*index >= argc) {
    return false;
  }
  *value = argv[(*index)++];
  return !value->empty();
}

bool TryParseDouble(const std::string& text, double* value) {
  try {
    size_t parsed_length = 0;
    *value = std::stod(text, &parsed_length);
    return parsed_length == text.size();
  } catch (...) {
    return false;
  }
}

bool TryParseInt(const std::string& text, int* value) {
  try {
    size_t parsed_length = 0;
    *value = std::stoi(text, &parsed_length);
    return parsed_length == text.size();
  } catch (...) {
    return false;
  }
}

void EnableConfigFlag(const char* config_name) {
  config_set_int(config_name, 1);
}

}  // namespace

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
      << "  -g/-grid <grid_value_x> <grid_value_y>     (optional, default metal1 and metal2 pitch values)\n"
      << "  -d/-target_density <density>               (optional, value interval (0,1], default max(space_utility, 0.7))\n"
      << "  -disable_legalization                      optional, if this flag is present, then legalization is skipped\n"
      << "  -io_metal_layer                            metal layer number for I/O placement (optional, default 1 for m1)\n"
      << "  -well_legalization_mode <scavenge/strict>  determine whether the last column use unassigned space\n"
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
    } else if (arg == "-disable_global_place") {
      EnableConfigFlag("dali.disable_global_place");
    } else if (arg == "-max_row_width") {
      double max_row_width = 0;
      if (!TryGetValue(argc, argv, &i, &value) ||
          !TryParseDouble(value, &max_row_width) || max_row_width < 0) {
        error_output << "Invalid max row width!\n";
        return false;
      }
      config_set_real("dali.max_row_width", max_row_width);
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
