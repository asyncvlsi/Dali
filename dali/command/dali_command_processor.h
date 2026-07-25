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
#ifndef DALI_COMMAND_DALI_COMMAND_PROCESSOR_H_
#define DALI_COMMAND_DALI_COMMAND_PROCESSOR_H_

#include <cstddef>
#include <string>
#include <vector>

namespace dali {

class Dali;

/**
 * Parse and execute the small command language used by `.dali` files.
 *
 * The processor contains no placement logic. It translates commands into the
 * same public Dali APIs used by the existing argv-style `interact` integration,
 * keeping scripts, a future interactive prompt, and external command hosts on
 * one execution path.
 */
class DaliCommandProcessor {
 public:
  explicit DaliCommandProcessor(Dali* dali);

  /** Execute a command that has already been split into arguments. */
  bool ExecuteCommand(const std::vector<std::string>& arguments);

  /** Tokenize and execute one command line. */
  bool ExecuteCommandLine(const std::string& command_line,
                          const std::string& source_name = "<command>",
                          std::size_t line_number = 1);

  /** Execute every command in a file, stopping at the first error. */
  bool RunCommandFile(const std::string& file_name);

  /**
   * Split one command line using shell-like quotes and backslash escaping.
   *
   * A `#` outside quotes starts a comment. This intentionally omits shell
   * expansion, variables, and command substitution so `.dali` files remain
   * deterministic placement recipes rather than another scripting language.
   */
  static bool TokenizeCommandLine(const std::string& command_line,
                                  std::vector<std::string>* arguments,
                                  std::string* error_message);

 private:
  bool ExecuteRun(const std::vector<std::string>& arguments);
  bool ExecuteLegacyPlaceDesign(const std::vector<std::string>& arguments);
  bool ExecuteLegacyGlobalPlace(const std::vector<std::string>& arguments);
  bool ForwardArgvCommand(const std::vector<std::string>& arguments,
                          bool (Dali::*command)(int, char**));
  void ReportUsage() const;

  Dali* dali_ = nullptr;
};

}  // namespace dali

#endif  // DALI_COMMAND_DALI_COMMAND_PROCESSOR_H_
