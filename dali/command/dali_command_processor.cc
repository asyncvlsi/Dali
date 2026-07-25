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
#include "dali/command/dali_command_processor.h"

#include <cctype>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "dali/common/logging.h"
#include "dali/dali.h"

namespace dali {

static bool ParseDouble(const std::string& text, double* value) {
  try {
    std::size_t parsed_length = 0;
    *value = std::stod(text, &parsed_length);
    return parsed_length == text.size();
  } catch (...) {
    return false;
  }
}

static bool ParseInt(const std::string& text, int* value) {
  try {
    std::size_t parsed_length = 0;
    *value = std::stoi(text, &parsed_length);
    return parsed_length == text.size();
  } catch (...) {
    return false;
  }
}

static bool HasLineContinuation(const std::string& line,
                                std::size_t* backslash_position) {
  const std::size_t last = line.find_last_not_of(" \t\r");
  if (last == std::string::npos || line[last] != '\\') {
    return false;
  }
  std::size_t preceding_backslashes = 0;
  for (std::size_t i = last; i > 0 && line[i - 1] == '\\'; --i) {
    ++preceding_backslashes;
  }
  if (preceding_backslashes % 2 != 0) {
    return false;
  }
  *backslash_position = last;
  return true;
}

DaliCommandProcessor::DaliCommandProcessor(Dali* dali) : dali_(dali) {}

bool DaliCommandProcessor::TokenizeCommandLine(
    const std::string& command_line, std::vector<std::string>* arguments,
    std::string* error_message) {
  arguments->clear();
  std::string argument;
  char quote = '\0';
  bool escaping = false;
  bool argument_started = false;

  for (char character : command_line) {
    if (escaping) {
      argument.push_back(character);
      argument_started = true;
      escaping = false;
      continue;
    }
    if (character == '\\') {
      escaping = true;
      argument_started = true;
      continue;
    }
    if (quote != '\0') {
      if (character == quote) {
        quote = '\0';
      } else {
        argument.push_back(character);
      }
      argument_started = true;
      continue;
    }
    if (character == '\'' || character == '"') {
      quote = character;
      argument_started = true;
      continue;
    }
    if (character == '#') {
      break;
    }
    if (std::isspace(static_cast<unsigned char>(character))) {
      if (argument_started) {
        arguments->push_back(std::move(argument));
        argument.clear();
        argument_started = false;
      }
      continue;
    }
    argument.push_back(character);
    argument_started = true;
  }

  if (escaping) {
    *error_message = "line ends with an incomplete escape";
    return false;
  }
  if (quote != '\0') {
    *error_message = "unterminated quoted argument";
    return false;
  }
  if (argument_started) {
    arguments->push_back(std::move(argument));
  }
  return true;
}

bool DaliCommandProcessor::ForwardArgvCommand(
    const std::vector<std::string>& arguments,
    bool (Dali::*command)(int, char**)) {
  std::vector<std::string> mutable_arguments = arguments;
  std::vector<char*> argv;
  argv.reserve(mutable_arguments.size());
  for (std::string& argument : mutable_arguments) {
    argv.push_back(argument.data());
  }
  return (dali_->*command)(static_cast<int>(argv.size()), argv.data());
}

bool DaliCommandProcessor::ExecuteRun(
    const std::vector<std::string>& arguments) {
  if (arguments.size() != 2) {
    LOG(error) << "Usage: run placement\n";
    return false;
  }
  if (arguments[1] == "placement") {
    return dali_->StartPlacement();
  }
  LOG(error) << "Unknown Dali run target: " << arguments[1] << "\n";
  return false;
}

bool DaliCommandProcessor::ExecuteLegacyPlaceDesign(
    const std::vector<std::string>& arguments) {
  if (arguments.size() < 2 || arguments.size() > 3) {
    LOG(error) << "Usage: place-design <target_density> [number_of_threads]\n";
    return false;
  }
  double density = 0;
  if (!ParseDouble(arguments[1], &density) || density <= 0 || density > 1) {
    LOG(error) << "Invalid target density: " << arguments[1] << "\n";
    return false;
  }
  int number_of_threads = -1;
  if (arguments.size() == 3 &&
      (!ParseInt(arguments[2], &number_of_threads) || number_of_threads < 1)) {
    LOG(error) << "Invalid number of threads: " << arguments[2] << "\n";
    return false;
  }
  return dali_->StartPlacement(density, number_of_threads);
}

bool DaliCommandProcessor::ExecuteLegacyGlobalPlace(
    const std::vector<std::string>& arguments) {
  if (arguments.size() < 2 || arguments.size() > 3) {
    LOG(error) << "Usage: global-place <target_density> [number_of_threads]\n";
    return false;
  }
  double density = 0;
  if (!ParseDouble(arguments[1], &density) || density <= 0 || density > 1) {
    LOG(error) << "Invalid target density: " << arguments[1] << "\n";
    return false;
  }
  int number_of_threads = 1;
  if (arguments.size() == 3 &&
      (!ParseInt(arguments[2], &number_of_threads) || number_of_threads < 1)) {
    LOG(error) << "Invalid number of threads: " << arguments[2] << "\n";
    return false;
  }
  return dali_->GlobalPlace(density, number_of_threads);
}

void DaliCommandProcessor::ReportUsage() const {
  LOG(info)
      << "Dali command language:\n"
      << "  set <option> <value>     configure a placement run\n"
      << "  show settings            report the resolved runtime settings\n"
      << "  run placement            execute the configured placement flow\n"
      << "  place-io ...             configure or run I/O placement\n"
      << "  show-io [pin]            report current I/O pin placement\n"
      << "  move-io <pin> <x> <y> [orient]\n"
      << "                           move and fix a pin in microns\n"
      << "  unfix-io <pin>           release a pin for automatic placement\n"
      << "  check-io                 validate I/O pin placement\n"
      << "  place-design <d> [n]     legacy placement command\n"
      << "  global-place <d> [n]     legacy global-placement command\n"
      << "  add-welltap ...          legacy well-tap command\n"
      << "  help                     show this command list\n";
}

bool DaliCommandProcessor::ExecuteCommand(
    const std::vector<std::string>& arguments) {
  if (arguments.empty()) {
    return true;
  }
  std::string command = arguments.front();
  const std::string namespace_prefix = "dali:";
  if (command.compare(0, namespace_prefix.size(), namespace_prefix) == 0) {
    command.erase(0, namespace_prefix.size());
  }

  if (command == "set") {
    if (arguments.size() != 3) {
      LOG(error) << "Usage: set <option> <value>\n";
      return false;
    }
    return dali_->SetRuntimeOption(arguments[1], arguments[2]);
  }
  if (command == "show") {
    if (arguments.size() != 2 || arguments[1] != "settings") {
      LOG(error) << "Usage: show settings\n";
      return false;
    }
    dali_->ShowParamsList();
    return true;
  }
  if (command == "run") {
    return ExecuteRun(arguments);
  }
  if (command == "place-io") {
    std::vector<std::string> normalized_arguments = arguments;
    normalized_arguments[0] = "place-io";
    return ForwardArgvCommand(normalized_arguments, &Dali::IoPinPlacement);
  }
  if (command == "show-io" || command == "move-io" || command == "unfix-io" ||
      command == "check-io") {
    std::vector<std::string> normalized_arguments{"place-io"};
    normalized_arguments.push_back("-" + command.substr(0, command.size() - 3));
    normalized_arguments.insert(normalized_arguments.end(),
                                arguments.begin() + 1, arguments.end());
    return ForwardArgvCommand(normalized_arguments, &Dali::IoPinPlacement);
  }
  if (command == "add-welltap") {
    std::vector<std::string> normalized_arguments = arguments;
    normalized_arguments[0] = "add-welltap";
    return ForwardArgvCommand(normalized_arguments, &Dali::AddWellTaps);
  }
  if (command == "place-design") {
    return ExecuteLegacyPlaceDesign(arguments);
  }
  if (command == "global-place") {
    return ExecuteLegacyGlobalPlace(arguments);
  }
  if (command == "help") {
    if (arguments.size() != 1) {
      LOG(error) << "Usage: help\n";
      return false;
    }
    ReportUsage();
    return true;
  }

  LOG(error) << "Unknown Dali command: " << arguments.front() << "\n";
  return false;
}

bool DaliCommandProcessor::ExecuteCommandLine(const std::string& command_line,
                                              const std::string& source_name,
                                              std::size_t line_number) {
  std::vector<std::string> arguments;
  std::string error_message;
  if (!TokenizeCommandLine(command_line, &arguments, &error_message)) {
    LOG(error) << source_name << ":" << line_number << ": " << error_message
               << "\n";
    return false;
  }
  if (arguments.empty()) {
    return true;
  }
  if (!ExecuteCommand(arguments)) {
    LOG(error) << source_name << ":" << line_number << ": command failed\n";
    return false;
  }
  return true;
}

bool DaliCommandProcessor::RunCommandFile(const std::string& file_name) {
  std::ifstream input(file_name);
  if (!input) {
    LOG(error) << "Cannot open Dali command file: " << file_name << "\n";
    return false;
  }

  std::string physical_line;
  std::string logical_line;
  std::size_t physical_line_number = 0;
  std::size_t logical_line_number = 0;
  while (std::getline(input, physical_line)) {
    ++physical_line_number;
    if (logical_line.empty()) {
      logical_line_number = physical_line_number;
    }
    std::size_t backslash_position = 0;
    const bool continues =
        HasLineContinuation(physical_line, &backslash_position);
    if (continues) {
      physical_line.erase(backslash_position);
    }
    logical_line += physical_line;
    if (continues) {
      logical_line.push_back(' ');
      continue;
    }
    if (!ExecuteCommandLine(logical_line, file_name, logical_line_number)) {
      return false;
    }
    logical_line.clear();
  }
  if (!logical_line.empty()) {
    LOG(error) << file_name << ":" << logical_line_number
               << ": incomplete line continuation\n";
    return false;
  }
  return true;
}

bool Dali::ExecuteCommand(const std::vector<std::string>& arguments) {
  return DaliCommandProcessor(this).ExecuteCommand(arguments);
}

bool Dali::ExecuteCommandLine(const std::string& command_line) {
  return DaliCommandProcessor(this).ExecuteCommandLine(command_line);
}

bool Dali::RunCommandFile(const std::string& file_name) {
  return DaliCommandProcessor(this).RunCommandFile(file_name);
}

}  // namespace dali
