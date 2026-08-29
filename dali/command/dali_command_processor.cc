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

#include <poll.h>
#include <unistd.h>

#include <cctype>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "dali/common/logging.h"
#include "dali/dali.h"

namespace dali {

static bool ParseDouble(const std::string &text, double *value) {
  try {
    std::size_t parsed_length = 0;
    *value = std::stod(text, &parsed_length);
    return parsed_length == text.size();
  } catch (...) {
    return false;
  }
}

static bool ParseInt(const std::string &text, int *value) {
  try {
    std::size_t parsed_length = 0;
    *value = std::stoi(text, &parsed_length);
    return parsed_length == text.size();
  } catch (...) {
    return false;
  }
}

static bool HasLineContinuation(const std::string &line,
                                std::size_t *backslash_position) {
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

/** Return true when a successful command can change physical placement. */
static bool ChangesPlacement(const std::vector<std::string> &arguments) {
  std::string command = arguments.front();
  const std::string namespace_prefix = "dali:";
  if (command.compare(0, namespace_prefix.size(), namespace_prefix) == 0) {
    command.erase(0, namespace_prefix.size());
  }
  if (command == "run" || command == "place-design" ||
      command == "global-place" || command == "add-welltap" ||
      command == "move-io" || command == "unfix-io") {
    return true;
  }
  if (command != "place-io" || arguments.size() < 2) {
    return false;
  }
  const std::string &option = arguments[1];
  return option != "-h" && option != "--help" && option != "-c" &&
         option != "-config" && option != "--config" && option != "-show" &&
         option != "--show" && option != "-check" && option != "--check";
}

DaliCommandProcessor::DaliCommandProcessor(Dali *dali) : dali_(dali) {}

bool DaliCommandProcessor::TokenizeCommandLine(
    const std::string &command_line, std::vector<std::string> *arguments,
    std::string *error_message) {
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
    const std::vector<std::string> &arguments,
    bool (Dali::*command)(int, char **)) {
  std::vector<std::string> mutable_arguments = arguments;
  std::vector<char *> argv;
  argv.reserve(mutable_arguments.size());
  for (std::string &argument : mutable_arguments) {
    argv.push_back(argument.data());
  }
  return (dali_->*command)(static_cast<int>(argv.size()), argv.data());
}

bool DaliCommandProcessor::ExecuteRun(
    const std::vector<std::string> &arguments) {
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
    const std::vector<std::string> &arguments) {
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
    const std::vector<std::string> &arguments) {
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
      << "  read-lef <file>          load LEF technology and cell libraries\n"
      << "  read-def <file>          load a DEF design after LEF\n"
      << "  read-cell <file>         load optional gridded-cell well data\n"
      << "  set <option> <value>     configure a placement run\n"
      << "  show settings            report the resolved runtime settings\n"
      << "  run placement            execute the configured placement flow\n"
      << "  read-delay-sites <file> load declared timing-delay metadata\n"
      << "  timing-report            synchronize placement and report timing\n"
      << "  runtime-report           report accumulated wall time by flow phase\n"
      << "  timing-check             fail if timing constraints are violated\n"
      << "  write-timing-repair-plan <file.json>\n"
      << "                           write globally ranked delay candidates\n"
      << "  write-timing-constraint-identities <file.json> [site ...]\n"
      << "  write-current-timing-constraint-identities <file.json> [site ...]\n"
      << "                           write identities without refreshing timing\n"
      << "  write-timing-decomposition <file.json> [site ...]\n"
      << "                           write stable constraint endpoints and slacks\n"
      << "  weight-fast-paths <mult>  weight datapath nets on constraint fast paths\n"
      << "  weight-critical-cycle <multiplier>\n"
      << "                           weight mapped critical-cycle nets\n"
      << "  detour-delay-line <prefix> <amplitude_um>\n"
      << "                           zigzag a delay line's interior to add "
         "wire delay\n"
      << "  spread-delay-line <prefix> <separation_rows> [<stride>]\n"
      << "  stage-band <prefix> [<prefix> ...]\n"
      << "                           spread a delay line across rows to add "
         "wire delay\n"
      << "  close-timing-with-delay-lines <margin_ps> <initial_step> "
         "<max_rounds>\n"
      << "                           place, measure, and widen delay lines "
         "until timing closes\n"
      << "  delay-line-timing-feedback <margin_ps> <initial_step> <warmup> "
         "<interval> <freeze> <damping> <max_separation>\n"
      << "                           retune delay lines from timing during "
         "global placement\n"
      << "  place-io ...             configure or run I/O placement\n"
      << "  show-io [pin]            report current I/O pin placement\n"
      << "  move-io <pin> <x> <y> [orient]\n"
      << "                           move and fix a pin in microns\n"
      << "  unfix-io <pin>           release a pin for automatic placement\n"
      << "  check-io                 validate I/O pin placement\n"
      << "  write-def [output]       export the current placement\n"
      << "  source <file.dali>       execute another command recipe\n"
      << "  history                  interactive: show commands in this "
         "session\n"
      << "  quit/exit                interactive: finish and export the "
         "design\n"
      << "  place-design <d> [n]     legacy placement command\n"
      << "  global-place <d> [n]     legacy global-placement command\n"
      << "  add-welltap ...          legacy well-tap command\n"
      << "  help                     show this command list\n";
}

bool DaliCommandProcessor::DispatchCommand(
    const std::vector<std::string> &arguments) {
  if (arguments.empty()) {
    return true;
  }
  std::string command = arguments.front();
  const std::string namespace_prefix = "dali:";
  if (command.compare(0, namespace_prefix.size(), namespace_prefix) == 0) {
    command.erase(0, namespace_prefix.size());
  }

  if (command == "read-lef" || command == "read-def" ||
      command == "read-cell") {
    if (arguments.size() != 2) {
      LOG(error) << "Usage: " << command << " <file>\n";
      return false;
    }
    const std::string file_name = ResolvePath(arguments[1]);
    if (command == "read-lef") {
      return dali_->ReadLef(file_name);
    }
    if (command == "read-def") {
      return dali_->ReadDef(file_name);
    }
    return dali_->ReadCell(file_name);
  }
  if (command == "set") {
    if (arguments.size() != 3) {
      LOG(error) << "Usage: set <option> <value>\n";
      return false;
    }
    const std::string value = arguments[1] == "output_name"
                                  ? ResolvePath(arguments[2])
                                  : arguments[2];
    return dali_->SetRuntimeOption(arguments[1], value);
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
  if (command == "read-delay-sites") {
    if (arguments.size() != 2) {
      LOG(error) << "Usage: read-delay-sites <file.json>\n";
      return false;
    }
    return dali_->ReadDelayRepairSites(ResolvePath(arguments[1]));
  }
  if (command == "timing-report") {
    if (arguments.size() != 1) {
      LOG(error) << "Usage: timing-report\n";
      return false;
    }
    return dali_->ReportTiming();
  }
  if (command == "runtime-report") {
    if (arguments.size() != 1) {
      LOG(error) << "Usage: runtime-report\n";
      return false;
    }
    return dali_->ReportRuntimeBreakdown();
  }
  if (command == "timing-check") {
    if (arguments.size() != 1) {
      LOG(error) << "Usage: timing-check\n";
      return false;
    }
    return dali_->CheckTiming();
  }
  if (command == "write-timing-repair-plan") {
    if (arguments.size() != 2) {
      LOG(error) << "Usage: write-timing-repair-plan <file.json>\n";
      return false;
    }
    return dali_->WriteTimingRepairPlan(ResolvePath(arguments[1]));
  }
  if (command == "write-timing-constraint-identities") {
    if (arguments.size() < 2) {
      LOG(error) << "Usage: write-timing-constraint-identities <file.json> "
                    "[<replaceable_site_prefix> ...]\n";
      return false;
    }
    return dali_->WriteTimingConstraintIdentities(
        ResolvePath(arguments[1]),
        std::vector<std::string>(arguments.begin() + 2, arguments.end()));
  }
  if (command == "write-current-timing-constraint-identities") {
    if (arguments.size() < 2) {
      LOG(error)
          << "Usage: write-current-timing-constraint-identities <file.json> "
             "[<replaceable_site_prefix> ...]\n";
      return false;
    }
    return dali_->WriteCurrentTimingConstraintIdentities(
        ResolvePath(arguments[1]),
        std::vector<std::string>(arguments.begin() + 2, arguments.end()));
  }
  if (command == "write-timing-decomposition") {
    if (arguments.size() < 2) {
      LOG(error) << "Usage: write-timing-decomposition <file.json> "
                    "[<delay_site_prefix> ...]\n";
      return false;
    }
    return dali_->WriteCurrentTimingDecomposition(
        ResolvePath(arguments[1]),
        std::vector<std::string>(arguments.begin() + 2, arguments.end()));
  }
  if (command == "weight-fast-paths") {
    double multiplier = 0.0;
    if (arguments.size() != 2 || !ParseDouble(arguments[1], &multiplier)) {
      LOG(error) << "Usage: weight-fast-paths <multiplier>\n";
      return false;
    }
    return dali_->WeightFastPathNets(multiplier);
  }

  if (command == "detour-delay-line") {
    double amplitude = 0.0;
    if (arguments.size() != 3 || !ParseDouble(arguments[2], &amplitude) ||
        amplitude <= 0.0) {
      LOG(error) << "Usage: detour-delay-line <name_prefix> <amplitude_um>\n";
      return false;
    }
    return dali_->DetourDelayLine(arguments[1], amplitude);
  }

  if (command == "spread-delay-line") {
    int separation = 0;
    int column_stride = 1;
    const bool arity_ok = arguments.size() == 3 || arguments.size() == 4;
    if (!arity_ok || !ParseInt(arguments[2], &separation) || separation < 0 ||
        (arguments.size() == 4 &&
         (!ParseInt(arguments[3], &column_stride) || column_stride < 0))) {
      LOG(error) << "Usage: spread-delay-line <name_prefix> <separation_rows> "
                    "[<column_stride>|0 for widest]\n";
      return false;
    }
    return dali_->SpreadDelayLineAcrossRows(arguments[1], separation,
                                            column_stride);
  }

  if (command == "register-delay-line") {
    if (arguments.size() != 2) {
      LOG(error) << "Usage: register-delay-line <name_prefix>\n";
      return false;
    }
    return dali_->RegisterDelayLine(arguments[1]);
  }

  if (command == "observe-timing-domains") {
    // Observation only. Records which coordinates each timing measurement was
    // actually taken on, which is the question the sizing mismatch turns on.
    if (arguments.size() < 2) {
      LOG(error) << "Usage: observe-timing-domains <prefix> [<site> ...]\n";
      return false;
    }
    dali_->EnableTimingDomainObservation(
        ResolvePath(arguments[1]),
        std::vector<std::string>(arguments.begin() + 2, arguments.end()));
    return true;
  }

  if (command == "topology-request") {
    // The fixed experiment, stated rather than decided. Travels the same
    // transport and the same delta validation as the automatic path.
    if (arguments.size() != 4) {
      LOG(error) << "Usage: topology-request <site> <current_pairs> "
                    "<requested_pairs>\n";
      return false;
    }
    int current_pairs = 0;
    int requested_pairs = 0;
    if (!ParseInt(arguments[2], &current_pairs) || current_pairs < 1 ||
        !ParseInt(arguments[3], &requested_pairs) ||
        requested_pairs <= current_pairs) {
      LOG(error) << "topology-request needs 1 <= current_pairs < "
                    "requested_pairs\n";
      return false;
    }
    dali_->SetFixedTopologyRequest(arguments[1], current_pairs,
                                   requested_pairs);
    return true;
  }

  if (command == "delay-line-characterization") {
    // The measured gain for one site, and the pair range it was measured over.
    // Stated in the recipe rather than derived in the run: one size and one
    // slack cannot yield a slope, and a coefficient describes only the sizes it
    // came from.
    if (arguments.size() != 5) {
      LOG(error) << "Usage: delay-line-characterization <site> <ps_per_pair> "
                    "<min_pairs> <max_pairs>\n";
      return false;
    }
    double ps_per_pair = 0.0;
    int min_pairs = 0;
    int max_pairs = 0;
    if (!ParseDouble(arguments[2], &ps_per_pair) || ps_per_pair <= 0.0) {
      LOG(error) << "delay-line-characterization needs a positive ps-per-pair "
                    "gain\n";
      return false;
    }
    if (!ParseInt(arguments[3], &min_pairs) || min_pairs < 1 ||
        !ParseInt(arguments[4], &max_pairs) || max_pairs < min_pairs) {
      LOG(error) << "delay-line-characterization needs 1 <= min_pairs <= "
                    "max_pairs\n";
      return false;
    }
    dali_->SetDelayLineCharacterization(arguments[1], ps_per_pair, min_pairs,
                                        max_pairs);
    return true;
  }

  if (command == "delay-line-response") {
    if (arguments.size() != 5) {
      LOG(error) << "Usage: delay-line-response <site> <current_pairs> "
                    "<target_pairs> <covered_deficit_ps>\n";
      return false;
    }
    int current_pairs = 0;
    int target_pairs = 0;
    double covered_deficit_ps = 0.0;
    if (!ParseInt(arguments[2], &current_pairs) || current_pairs < 1 ||
        !ParseInt(arguments[3], &target_pairs) ||
        target_pairs <= current_pairs ||
        !ParseDouble(arguments[4], &covered_deficit_ps) ||
        covered_deficit_ps <= 0.0) {
      LOG(error) << "delay-line-response needs 1 <= current_pairs < "
                    "target_pairs and positive covered_deficit_ps\n";
      return false;
    }
    return dali_->AddDelayLineResponse(arguments[1], current_pairs,
                                       target_pairs, covered_deficit_ps);
  }

  if (command == "stage-band") {
    if (arguments.size() < 2) {
      LOG(error) << "Usage: stage-band <name_prefix> [<name_prefix> ...]\n";
      return false;
    }
    return dali_->DeclareStageBand(
        std::vector<std::string>(arguments.begin() + 1, arguments.end()));
  }

  if (command == "close-timing-with-delay-lines") {
    double margin = 0.0;
    int initial_step = 0;
    int max_rounds = 0;
    if (arguments.size() != 4 || !ParseDouble(arguments[1], &margin) ||
        !ParseInt(arguments[2], &initial_step) ||
        !ParseInt(arguments[3], &max_rounds)) {
      LOG(error) << "Usage: close-timing-with-delay-lines <margin_ps> "
                    "<initial_step_rows> <max_rounds>\n";
      return false;
    }
    return dali_->CloseTimingWithDelayLineSpread(margin, initial_step,
                                                 max_rounds);
  }

  if (command == "delay-line-timing-feedback") {
    double margin = 0.0;
    double damping = 1.0;
    int initial_step = 0;
    int warmup = 0;
    int interval = 0;
    int freeze = 0;
    int max_separation = 0;
    if (arguments.size() != 8 || !ParseDouble(arguments[1], &margin) ||
        !ParseInt(arguments[7], &max_separation) || max_separation < 0 ||
        !ParseInt(arguments[2], &initial_step) ||
        !ParseInt(arguments[3], &warmup) || !ParseInt(arguments[4], &interval) ||
        !ParseInt(arguments[5], &freeze) ||
        !ParseDouble(arguments[6], &damping) || initial_step < 1 ||
        interval < 1 || damping <= 0.0) {
      LOG(error) << "Usage: delay-line-timing-feedback <margin_ps> "
                    "<initial_step> <warmup> <interval> <freeze> <damping> "
                    "<max_separation>\n";
      return false;
    }
    dali_->EnableDelayLineTimingFeedback(margin, initial_step, warmup, interval,
                                         freeze, damping, max_separation);
    return true;
  }

  if (command == "weight-critical-cycle") {
    double multiplier = 0.0;
    if (arguments.size() != 2 || !ParseDouble(arguments[1], &multiplier) ||
        multiplier <= 0.0) {
      LOG(error) << "Usage: weight-critical-cycle <positive_multiplier>\n";
      return false;
    }
    return dali_->WeightCriticalCycleNets(multiplier);
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
  if (command == "source") {
    if (arguments.size() != 2) {
      LOG(error) << "Usage: source <file.dali>\n";
      return false;
    }
    return RunCommandFile(arguments[1]);
  }
  if (command == "write-def") {
    if (arguments.size() > 2) {
      LOG(error) << "Usage: write-def [output]\n";
      return false;
    }
    const std::string output_name =
        arguments.size() == 2 ? ResolvePath(arguments[1]) : "";
    return dali_->ExportPlacement(output_name);
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

bool DaliCommandProcessor::ExecuteCommand(
    const std::vector<std::string> &arguments) {
  const bool is_success = DispatchCommand(arguments);
  if (is_success && !arguments.empty() && ChangesPlacement(arguments)) {
    dali_->WriteInteractiveCommandSnapshot(arguments.front());
  }
  return is_success;
}

bool DaliCommandProcessor::ExecuteCommandLine(const std::string &command_line,
                                              const std::string &source_name,
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

std::string DaliCommandProcessor::ResolvePath(const std::string &path) const {
  std::filesystem::path resolved(path);
  if (resolved.is_absolute()) {
    return resolved.lexically_normal().string();
  }
  if (!command_directories_.empty()) {
    return (std::filesystem::path(command_directories_.back()) / resolved)
        .lexically_normal()
        .string();
  }
  std::error_code error;
  const std::filesystem::path current_directory =
      std::filesystem::current_path(error);
  if (error) {
    return resolved.lexically_normal().string();
  }
  return (current_directory / resolved).lexically_normal().string();
}

bool DaliCommandProcessor::RunCommandFile(const std::string &file_name) {
  const std::string resolved_file_name = ResolvePath(file_name);
  std::ifstream input(resolved_file_name);
  if (!input) {
    LOG(error) << "Cannot open Dali command file: " << resolved_file_name
               << "\n";
    return false;
  }

  const std::filesystem::path command_file_path(resolved_file_name);
  command_directories_.push_back(command_file_path.parent_path().string());
  std::string physical_line;
  std::string logical_line;
  std::size_t physical_line_number = 0;
  std::size_t logical_line_number = 0;
  bool is_success = true;
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
    if (!ExecuteCommandLine(logical_line, resolved_file_name,
                            logical_line_number)) {
      is_success = false;
      break;
    }
    logical_line.clear();
  }
  if (is_success && !logical_line.empty()) {
    LOG(error) << resolved_file_name << ":" << logical_line_number
               << ": incomplete line continuation\n";
    is_success = false;
  }
  command_directories_.pop_back();
  return is_success;
}

void DaliCommandProcessor::ReportHistory(std::ostream &output) const {
  for (std::size_t i = 0; i < command_history_.size(); ++i) {
    output << "  " << i + 1 << "  " << command_history_[i] << "\n";
  }
}

bool DaliCommandProcessor::RunInteractive(
    std::istream &input, std::ostream &output, bool show_prompt,
    const std::function<void()> &wait_for_input) {
  output << "Dali interactive mode. Type 'help' for commands and 'quit' to "
            "finish.\n";

  std::string physical_line;
  std::string logical_line;
  std::size_t line_number = 0;
  std::size_t logical_line_number = 0;
  while (true) {
    if (show_prompt) {
      output << (logical_line.empty() ? "dali> " : "  ... ") << std::flush;
    }
    if (wait_for_input) {
      wait_for_input();
    }
    if (!std::getline(input, physical_line)) {
      if (input.bad()) {
        LOG(error) << "Failed while reading the interactive command stream\n";
        return false;
      }
      break;
    }

    ++line_number;
    if (logical_line.empty()) {
      logical_line_number = line_number;
    }
    std::size_t backslash_position = 0;
    bool continues = HasLineContinuation(physical_line, &backslash_position);
    if (continues) {
      physical_line.erase(backslash_position);
    }
    logical_line += physical_line;
    if (continues) {
      logical_line.push_back(' ');
      continue;
    }

    std::vector<std::string> arguments;
    std::string error_message;
    if (!TokenizeCommandLine(logical_line, &arguments, &error_message)) {
      LOG(error) << "<stdin>:" << logical_line_number << ": " << error_message
                 << "\n";
      logical_line.clear();
      continue;
    }
    if (arguments.empty()) {
      logical_line.clear();
      continue;
    }

    command_history_.push_back(logical_line);
    std::string command = arguments.front();
    const std::string namespace_prefix = "dali:";
    if (command.compare(0, namespace_prefix.size(), namespace_prefix) == 0) {
      command.erase(0, namespace_prefix.size());
    }
    if ((command == "quit" || command == "exit") && arguments.size() == 1) {
      return true;
    }
    if (command == "history" && arguments.size() == 1) {
      ReportHistory(output);
      logical_line.clear();
      continue;
    }

    ExecuteCommandLine(logical_line, "<stdin>", logical_line_number);
    logical_line.clear();
  }

  if (!logical_line.empty()) {
    LOG(error) << "<stdin>:" << logical_line_number
               << ": incomplete line continuation\n";
    return false;
  }
  return true;
}

bool Dali::ExecuteCommand(const std::vector<std::string> &arguments) {
  return DaliCommandProcessor(this).ExecuteCommand(arguments);
}

bool Dali::ExecuteCommandLine(const std::string &command_line) {
  return DaliCommandProcessor(this).ExecuteCommandLine(command_line);
}

bool Dali::RunCommandFile(const std::string &file_name) {
  return DaliCommandProcessor(this).RunCommandFile(file_name);
}

bool Dali::RunInteractiveSession(std::istream &input, std::ostream &output,
                                 bool show_prompt) {
  interactive_session_expected_ = true;
  if (HasInputDesign()) {
    InitializeCircuitFromPhyDBIfNeeded();
    InitializeVisualizationSnapshots();
    WriteInteractiveCommandSnapshot("start");
  }
  std::function<void()> wait_for_input;
  if (show_prompt && &input == &std::cin) {
    wait_for_input = [this]() {
      pollfd stdin_poll = {STDIN_FILENO, POLLIN, 0};
      while (true) {
        const int result = poll(&stdin_poll, 1, 16);
        FlushVisualizationEvents();
        if (result > 0 || (result < 0 && errno != EINTR)) {
          return;
        }
      }
    };
  }
  const bool is_success = DaliCommandProcessor(this).RunInteractive(
      input, output, show_prompt, wait_for_input);
  interactive_session_expected_ = false;
  FinishVisualizationSnapshots();
  return is_success;
}

} // namespace dali
