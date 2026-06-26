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
#ifndef DALI_APPLICATION_DALI_COMMAND_LINE_H_
#define DALI_APPLICATION_DALI_COMMAND_LINE_H_

#include <iosfwd>
#include <string>

#include "dali/common/logging.h"

namespace dali {

/** Parsed command-line settings for the main `dali` application. */
struct DaliCommandLineOptions {
  std::string lef_file_name;
  std::string def_file_name;
  std::string cell_file_name;
  std::string ignored_mcell_file_name;
  std::string output_name = "dali_out";
  std::string log_file_name;
  std::string metrics_file_name = "dali_metrics.json";
  severity verbose_level = severity::info;
  double x_grid = 0;
  double y_grid = 0;
};

/** Print command-line usage for the main placement application. */
void ReportDaliUsage(std::ostream& output);

/**
 * Parse command-line options for the main `dali` binary.
 *
 * Options that control the placement flow are also written to the ACT config
 * database because `Dali` loads its runtime settings from that shared config.
 */
bool ParseDaliCommandLine(int argc, char* argv[],
                          DaliCommandLineOptions* options,
                          std::ostream& error_output);

}  // namespace dali

#endif  // DALI_APPLICATION_DALI_COMMAND_LINE_H_
