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
#include <functional>
#include <iosfwd>
#include <string>
#include <vector>

namespace dali {

class Dali;

/**
 * Parse and execute the small command language used by `.dali` files.
 *
 * The processor contains no placement logic. It translates commands into the
 * same public Dali APIs used by the existing argv-style `interact` integration,
 * keeping scripts, the interactive prompt, and external command hosts on one
 * execution path.
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
   * Read and execute commands until EOF, `quit`, or `exit`.
   *
   * Unlike a command file, a failed interactive command is reported and the
   * session continues so users can correct the design. Returns false only for
   * an input-stream error or an incomplete final line continuation.
   */
  bool RunInteractive(std::istream& input, std::ostream& output,
                      bool show_prompt = true,
                      const std::function<void()>& wait_for_input = {});

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
  bool DispatchCommand(const std::vector<std::string>& arguments);
  bool ExecuteRun(const std::vector<std::string>& arguments);
  bool ExecuteLegacyPlaceDesign(const std::vector<std::string>& arguments);
  bool ExecuteLegacyGlobalPlace(const std::vector<std::string>& arguments);
  /** Resolve a command path against the recipe that contains it. */
  std::string ResolvePath(const std::string& path) const;
  void ReportHistory(std::ostream& output) const;
  bool ForwardArgvCommand(const std::vector<std::string>& arguments,
                          bool (Dali::*command)(int, char**));
  void ReportUsage() const;

  Dali* dali_ = nullptr;
  std::vector<std::string> command_history_;
  std::vector<std::string> command_directories_;
};

}  // namespace dali

#endif  // DALI_COMMAND_DALI_COMMAND_PROCESSOR_H_
