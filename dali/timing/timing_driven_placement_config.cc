/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 *******************************************************************************/

#include "dali/timing/timing_driven_placement_config.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <type_traits>
#include <utility>

#include "dali/timing/timing_driven_candidate_policy.h"

namespace dali {
namespace timing_driven_placement_config_detail {

bool ParseBool(const std::string &value, bool *result) {
  if (value == "true") {
    *result = true;
    return true;
  }
  if (value == "false") {
    *result = false;
    return true;
  }
  return false;
}

template <typename T> bool ParseNumber(const std::string &value, T *result) {
  std::istringstream input(value);
  input >> *result;
  if (!input || !input.eof())
    return false;
  if constexpr (std::is_floating_point_v<T>)
    return std::isfinite(*result);
  return true;
}

std::string ResolvePath(const std::filesystem::path &base,
                        const std::string &value) {
  const std::filesystem::path path(value);
  if (path.is_absolute())
    return path.lexically_normal().string();
  return (base / path).lexically_normal().string();
}

bool SetError(std::string *error, const std::string &message) {
  if (error != nullptr)
    *error = message;
  return false;
}

bool HasSite(const std::vector<std::string> &sites, const std::string &site) {
  for (const std::string &declared : sites) {
    if (declared == site)
      return true;
  }
  return false;
}

bool ValidateCandidateMaps(const TimingDrivenPlacementConfig &config,
                           std::string *error) {
  if (config.policy.delay_site_ids.empty()) {
    return SetError(error, "no policy.delay_site declarations");
  }
  std::set<std::string> unique_sites(config.policy.delay_site_ids.begin(),
                                     config.policy.delay_site_ids.end());
  for (const std::string &site : config.policy.delay_site_ids) {
    if (site.empty())
      return SetError(error, "empty policy.delay_site");
  }
  if (unique_sites.size() != config.policy.delay_site_ids.size()) {
    return SetError(error, "duplicate policy.delay_site declaration");
  }
  const auto validate = [&](const TimingDrivenPlacementCandidate &candidate,
                            const std::string &name) {
    if (candidate.delay_parameters.size() != unique_sites.size() ||
        candidate.replacement_processes.size() != unique_sites.size()) {
      return SetError(error, name + " does not cover every delay site");
    }
    for (const std::string &site : config.policy.delay_site_ids) {
      if (candidate.delay_parameters.find(site) ==
              candidate.delay_parameters.end() ||
          candidate.replacement_processes.find(site) ==
              candidate.replacement_processes.end()) {
        return SetError(error, name + " is missing delay site " + site);
      }
      if (candidate.delay_parameters.at(site) <= 0)
        return SetError(error,
                        name + " has an invalid delay parameter for " + site);
      if (candidate.replacement_processes.at(site).empty()) {
        return SetError(error, name + " has an empty replacement for " + site);
      }
    }
    for (const auto &parameter : candidate.delay_parameters) {
      if (!HasSite(config.policy.delay_site_ids, parameter.first)) {
        return SetError(error, name + " has an undeclared parameter site " +
                                   parameter.first);
      }
    }
    for (const auto &replacement : candidate.replacement_processes) {
      if (!HasSite(config.policy.delay_site_ids, replacement.first)) {
        return SetError(error, name + " has an undeclared replacement site " +
                                   replacement.first);
      }
    }
    return true;
  };

  if (!validate(config.policy.controller.initial_candidate, "policy.initial"))
    return false;
  for (std::size_t index = 0; index < config.policy.candidate_sequence.size();
       ++index) {
    if (!validate(config.policy.candidate_sequence[index],
                  "policy.candidate " + std::to_string(index + 1)))
      return false;
  }
  return true;
}

bool ValidateConfig(const TimingDrivenPlacementConfig &config,
                    std::string *error) {
  const auto &controller = config.policy.controller;
  const auto &lifecycle = config.lifecycle;
  if (controller.expected_constraint_count <= 0)
    return SetError(error, "expected constraint count must be positive");
  if (!std::isfinite(controller.required_slack_margin_ps) ||
      controller.required_slack_margin_ps < 0.0)
    return SetError(error, "invalid required slack margin");
  if (controller.max_trials <= 0 || controller.no_improvement_limit <= 0)
    return SetError(error, "trial limits must be positive");
  if (!std::isfinite(controller.minimum_period_improvement_ps) ||
      controller.minimum_period_improvement_ps < 0.0 ||
      !std::isfinite(controller.feasibility_merit_tolerance_ps) ||
      controller.feasibility_merit_tolerance_ps < 0.0)
    return SetError(error, "invalid controller tolerance");
  if (controller.initial_anchor.empty() ||
      controller.initial_generation_id.empty())
    return SetError(error, "initial anchor and generation are required");
  if (lifecycle.liberty_path.empty() || lifecycle.tech_config_path.empty() ||
      lifecycle.placement_recipe_path.empty())
    return SetError(error, "all lifecycle paths are required");
  if (!std::isfinite(lifecycle.target_density) ||
      lifecycle.target_density <= 0.0 || lifecycle.target_density > 1.0)
    return SetError(error, "target density must be in (0, 1]");
  if (lifecycle.num_threads <= 0 || lifecycle.rc_min_routing_layer < 0)
    return SetError(error, "invalid lifecycle integer setting");
  if (lifecycle.well_emit_mode < 0 || lifecycle.well_emit_mode > 2)
    return SetError(error, "unsupported well emit mode");
  const auto &die = lifecycle.fixed_die_grid;
  if (!die.valid || !std::isfinite(die.die_llx) ||
      !std::isfinite(die.die_lly) || !std::isfinite(die.die_urx) ||
      !std::isfinite(die.die_ury) || !std::isfinite(die.grid_x) ||
      !std::isfinite(die.grid_y) || die.die_urx <= die.die_llx ||
      die.die_ury <= die.die_lly || die.grid_x <= 0.0 || die.grid_y <= 0.0)
    return SetError(error, "invalid fixed die/grid");
  return ValidateCandidateMaps(config, error);
}

bool ValidateRequiredKeys(const std::set<std::string> &keys,
                          std::string *error) {
  static const char *required[] = {
      "format_version",
      "policy.expected_constraint_count",
      "policy.required_slack_margin_ps",
      "policy.max_trials",
      "policy.no_improvement_limit",
      "policy.minimum_period_improvement_ps",
      "policy.feasibility_merit_tolerance_ps",
      "policy.baseline_mode",
      "policy.initial_anchor",
      "policy.initial_generation_id",
      "policy.require_measurement_metadata",
      "lifecycle.liberty_path",
      "lifecycle.tech_config_path",
      "lifecycle.placement_recipe_path",
      "lifecycle.timing_use_rc",
      "lifecycle.rc_min_routing_layer",
      "lifecycle.target_density",
      "lifecycle.num_threads",
      "lifecycle.is_standard_cell",
      "lifecycle.well_emit_mode",
      "lifecycle.enable_well_taps",
      "lifecycle.fixed_die",
  };
  for (const char *key : required) {
    if (keys.find(key) == keys.end()) {
      return SetError(error, std::string("missing required key ") + key);
    }
  }
  return true;
}

} // namespace timing_driven_placement_config_detail

using namespace timing_driven_placement_config_detail;

bool LoadTimingDrivenPlacementConfig(const std::string &file_name,
                                     TimingDrivenPlacementConfig *config,
                                     std::string *error) {
  if (config == nullptr)
    return SetError(error, "null config output");
  std::ifstream input(file_name);
  if (!input)
    return SetError(error, "cannot open config " + file_name);

  TimingDrivenPlacementConfig parsed;
  const std::filesystem::path config_path(file_name);
  const std::filesystem::path base =
      config_path.has_parent_path()
          ? std::filesystem::absolute(config_path.parent_path())
          : std::filesystem::current_path();
  std::set<std::string> scalar_keys;
  std::set<std::string> seen_keys;
  std::set<std::string> initial_parameter_sites;
  std::set<std::string> initial_replacement_sites;
  std::map<int, std::set<std::string>> candidate_parameter_sites;
  std::map<int, std::set<std::string>> candidate_replacement_sites;
  std::map<int, TimingDrivenPlacementCandidate> candidates;
  std::string line;
  int line_number = 0;
  while (std::getline(input, line)) {
    ++line_number;
    const std::size_t comment = line.find('#');
    if (comment != std::string::npos)
      line.resize(comment);
    std::istringstream tokens(line);
    std::string key;
    if (!(tokens >> key))
      continue;
    std::vector<std::string> values;
    std::string value;
    while (tokens >> value)
      values.push_back(value);
    auto malformed = [&](const std::string &message) {
      return SetError(error,
                      "line " + std::to_string(line_number) + ": " + message);
    };
    auto scalar_seen = [&]() {
      if (!scalar_keys.insert(key).second) {
        return malformed("duplicate key " + key);
      }
      return true;
    };
    auto require_values = [&](std::size_t count) {
      return values.size() == count ||
             malformed("malformed value count for " + key);
    };
    if (key == "format_version") {
      if (!scalar_seen() || !require_values(1))
        return false;
      int version = 0;
      if (!ParseNumber(values[0], &version) || version != 1)
        return malformed("format_version must be 1");
    } else if (key == "policy.expected_constraint_count") {
      if (!scalar_seen() || !require_values(1) ||
          !ParseNumber(values[0],
                       &parsed.policy.controller.expected_constraint_count))
        return malformed("invalid expected constraint count");
    } else if (key == "policy.required_slack_margin_ps") {
      if (!scalar_seen() || !require_values(1) ||
          !ParseNumber(values[0],
                       &parsed.policy.controller.required_slack_margin_ps))
        return malformed("invalid slack margin");
    } else if (key == "policy.max_trials") {
      if (!scalar_seen() || !require_values(1) ||
          !ParseNumber(values[0], &parsed.policy.controller.max_trials))
        return malformed("invalid max_trials");
    } else if (key == "policy.no_improvement_limit") {
      if (!scalar_seen() || !require_values(1) ||
          !ParseNumber(values[0],
                       &parsed.policy.controller.no_improvement_limit))
        return malformed("invalid no_improvement_limit");
    } else if (key == "policy.minimum_period_improvement_ps") {
      if (!scalar_seen() || !require_values(1) ||
          !ParseNumber(values[0],
                       &parsed.policy.controller.minimum_period_improvement_ps))
        return malformed("invalid minimum period improvement");
    } else if (key == "policy.feasibility_merit_tolerance_ps") {
      if (!scalar_seen() || !require_values(1) ||
          !ParseNumber(
              values[0],
              &parsed.policy.controller.feasibility_merit_tolerance_ps))
        return malformed("invalid feasibility tolerance");
    } else if (key == "policy.baseline_mode") {
      if (!scalar_seen() || !require_values(1))
        return false;
      if (values[0] == "require_feasible") {
        parsed.policy.controller.baseline_mode =
            TimingDrivenBaselineMode::kRequireFeasible;
      } else if (values[0] == "track_best_infeasible") {
        parsed.policy.controller.baseline_mode =
            TimingDrivenBaselineMode::kTrackBestInfeasible;
      } else {
        return malformed("invalid baseline mode");
      }
    } else if (key == "policy.initial_anchor") {
      if (!scalar_seen() || !require_values(1))
        return false;
      parsed.policy.controller.initial_anchor = values[0];
    } else if (key == "policy.initial_generation_id") {
      if (!scalar_seen() || !require_values(1))
        return false;
      parsed.policy.controller.initial_generation_id = values[0];
    } else if (key == "policy.require_measurement_metadata") {
      if (!scalar_seen() || !require_values(1) ||
          !ParseBool(values[0],
                     &parsed.policy.controller.require_measurement_metadata))
        return malformed("invalid metadata requirement");
    } else if (key == "policy.delay_site") {
      if (!require_values(1))
        return false;
      if (!seen_keys.insert(key + " " + values[0]).second)
        return malformed("duplicate delay site " + values[0]);
      parsed.policy.delay_site_ids.push_back(values[0]);
    } else if (key == "policy.initial.parameter" ||
               key == "policy.initial.replacement") {
      if (!require_values(2))
        return false;
      std::set<std::string> &sites = key == "policy.initial.parameter"
                                         ? initial_parameter_sites
                                         : initial_replacement_sites;
      if (!sites.insert(values[0]).second)
        return malformed("duplicate initial site " + values[0]);
      if (key == "policy.initial.parameter") {
        int parameter = 0;
        if (!ParseNumber(values[1], &parameter))
          return malformed("invalid initial parameter");
        parsed.policy.controller.initial_candidate.delay_parameters[values[0]] =
            parameter;
      } else {
        parsed.policy.controller.initial_candidate
            .replacement_processes[values[0]] = values[1];
      }
    } else if (key == "policy.candidate") {
      if (values.size() != 4 ||
          (values[1] != "parameter" && values[1] != "replacement"))
        return malformed("expected: policy.candidate INDEX "
                         "parameter|replacement SITE VALUE");
      int index = 0;
      if (!ParseNumber(values[0], &index) || index <= 0)
        return malformed("candidate index must be positive");
      std::set<std::string> &sites = values[1] == "parameter"
                                         ? candidate_parameter_sites[index]
                                         : candidate_replacement_sites[index];
      if (!sites.insert(values[2]).second)
        return malformed("duplicate candidate site " + values[2]);
      if (values[1] == "parameter") {
        int parameter = 0;
        if (!ParseNumber(values[3], &parameter))
          return malformed("invalid candidate parameter");
        candidates[index].delay_parameters[values[2]] = parameter;
      } else {
        candidates[index].replacement_processes[values[2]] = values[3];
      }
    } else if (key == "lifecycle.liberty_path" ||
               key == "lifecycle.tech_config_path" ||
               key == "lifecycle.placement_recipe_path") {
      if (!scalar_seen() || !require_values(1) || values[0].empty())
        return malformed("invalid lifecycle path");
      const std::string resolved = ResolvePath(base, values[0]);
      if (key == "lifecycle.liberty_path")
        parsed.lifecycle.liberty_path = resolved;
      else if (key == "lifecycle.tech_config_path")
        parsed.lifecycle.tech_config_path = resolved;
      else
        parsed.lifecycle.placement_recipe_path = resolved;
    } else if (key == "lifecycle.timing_use_rc") {
      if (!scalar_seen() || !require_values(1) ||
          !ParseBool(values[0], &parsed.lifecycle.timing_use_rc))
        return malformed("invalid timing_use_rc");
    } else if (key == "lifecycle.rc_min_routing_layer") {
      if (!scalar_seen() || !require_values(1) ||
          !ParseNumber(values[0], &parsed.lifecycle.rc_min_routing_layer))
        return malformed("invalid rc_min_routing_layer");
    } else if (key == "lifecycle.target_density") {
      if (!scalar_seen() || !require_values(1) ||
          !ParseNumber(values[0], &parsed.lifecycle.target_density))
        return malformed("invalid target_density");
    } else if (key == "lifecycle.num_threads") {
      if (!scalar_seen() || !require_values(1) ||
          !ParseNumber(values[0], &parsed.lifecycle.num_threads))
        return malformed("invalid num_threads");
    } else if (key == "lifecycle.is_standard_cell") {
      if (!scalar_seen() || !require_values(1) ||
          !ParseBool(values[0], &parsed.lifecycle.is_standard_cell))
        return malformed("invalid is_standard_cell");
    } else if (key == "lifecycle.well_emit_mode") {
      if (!scalar_seen() || !require_values(1) ||
          !ParseNumber(values[0], &parsed.lifecycle.well_emit_mode))
        return malformed("invalid well_emit_mode");
    } else if (key == "lifecycle.enable_well_taps") {
      if (!scalar_seen() || !require_values(1) ||
          !ParseBool(values[0], &parsed.lifecycle.enable_well_taps))
        return malformed("invalid enable_well_taps");
    } else if (key == "lifecycle.fixed_die") {
      if (!scalar_seen() || !require_values(6) ||
          !ParseNumber(values[0], &parsed.lifecycle.fixed_die_grid.die_llx) ||
          !ParseNumber(values[1], &parsed.lifecycle.fixed_die_grid.die_lly) ||
          !ParseNumber(values[2], &parsed.lifecycle.fixed_die_grid.die_urx) ||
          !ParseNumber(values[3], &parsed.lifecycle.fixed_die_grid.die_ury) ||
          !ParseNumber(values[4], &parsed.lifecycle.fixed_die_grid.grid_x) ||
          !ParseNumber(values[5], &parsed.lifecycle.fixed_die_grid.grid_y))
        return malformed("invalid fixed_die");
      parsed.lifecycle.fixed_die_grid.valid = true;
    } else {
      return malformed("unknown key " + key);
    }
    seen_keys.insert(key);
  }

  if (!ValidateRequiredKeys(scalar_keys, error))
    return false;
  std::map<int, TimingDrivenPlacementCandidate> ordered;
  for (const auto &candidate : candidates) {
    ordered.emplace(candidate.first, candidate.second);
  }
  for (int index = 1; index <= static_cast<int>(ordered.size()); ++index) {
    if (ordered.find(index) == ordered.end())
      return SetError(error, "candidate indices must be contiguous from 1");
    parsed.policy.candidate_sequence.push_back(ordered[index]);
  }
  if (parsed.policy.candidate_sequence.empty())
    return SetError(error, "at least one policy.candidate is required");
  parsed.policy.controller.require_replacement_map = true;
  parsed.policy.controller.expected_delay_site_ids =
      parsed.policy.delay_site_ids;
  parsed.policy.controller.require_lifecycle_invariants = true;
  parsed.policy.controller.expected_die_grid = parsed.lifecycle.fixed_die_grid;
  parsed.policy.controller.expected_timing_use_rc =
      parsed.lifecycle.timing_use_rc;
  parsed.policy.controller.expected_rc_min_routing_layer =
      parsed.lifecycle.rc_min_routing_layer;
  if (!ValidateConfig(parsed, error))
    return false;
  *config = std::move(parsed);
  return true;
}

bool WriteTimingDrivenPlacementConfig(const std::string &file_name,
                                      const TimingDrivenPlacementConfig &config,
                                      std::string *error) {
  TimingDrivenPlacementConfig normalized = config;
  const std::filesystem::path output_path(file_name);
  const std::filesystem::path base =
      output_path.has_parent_path()
          ? std::filesystem::absolute(output_path.parent_path())
          : std::filesystem::current_path();
  normalized.lifecycle.liberty_path =
      ResolvePath(base, normalized.lifecycle.liberty_path);
  normalized.lifecycle.tech_config_path =
      ResolvePath(base, normalized.lifecycle.tech_config_path);
  normalized.lifecycle.placement_recipe_path =
      ResolvePath(base, normalized.lifecycle.placement_recipe_path);
  if (!ValidateConfig(normalized, error))
    return false;
  std::ofstream output(file_name);
  if (!output)
    return SetError(error, "cannot write config " + file_name);
  const auto &controller = config.policy.controller;
  output << std::setprecision(17);
  output << "format_version 1\n"
         << "policy.expected_constraint_count "
         << controller.expected_constraint_count << "\n"
         << "policy.required_slack_margin_ps "
         << controller.required_slack_margin_ps << "\n"
         << "policy.max_trials " << controller.max_trials << "\n"
         << "policy.no_improvement_limit " << controller.no_improvement_limit
         << "\n"
         << "policy.minimum_period_improvement_ps "
         << controller.minimum_period_improvement_ps << "\n"
         << "policy.feasibility_merit_tolerance_ps "
         << controller.feasibility_merit_tolerance_ps << "\n"
         << "policy.baseline_mode "
         << (controller.baseline_mode ==
                     TimingDrivenBaselineMode::kTrackBestInfeasible
                 ? "track_best_infeasible"
                 : "require_feasible")
         << "\n"
         << "policy.initial_anchor " << controller.initial_anchor << "\n"
         << "policy.initial_generation_id " << controller.initial_generation_id
         << "\n"
         << "policy.require_measurement_metadata "
         << (controller.require_measurement_metadata ? "true" : "false")
         << "\n";
  for (const std::string &site : config.policy.delay_site_ids)
    output << "policy.delay_site " << site << "\n";
  for (const auto &parameter : controller.initial_candidate.delay_parameters)
    output << "policy.initial.parameter " << parameter.first << " "
           << parameter.second << "\n";
  for (const auto &replacement :
       controller.initial_candidate.replacement_processes)
    output << "policy.initial.replacement " << replacement.first << " "
           << replacement.second << "\n";
  for (std::size_t index = 0; index < config.policy.candidate_sequence.size();
       ++index) {
    for (const auto &parameter :
         config.policy.candidate_sequence[index].delay_parameters)
      output << "policy.candidate " << index + 1 << " parameter "
             << parameter.first << " " << parameter.second << "\n";
    for (const auto &replacement :
         config.policy.candidate_sequence[index].replacement_processes)
      output << "policy.candidate " << index + 1 << " replacement "
             << replacement.first << " " << replacement.second << "\n";
  }
  const auto &lifecycle = normalized.lifecycle;
  output << "lifecycle.liberty_path " << lifecycle.liberty_path << "\n"
         << "lifecycle.tech_config_path " << lifecycle.tech_config_path << "\n"
         << "lifecycle.placement_recipe_path "
         << lifecycle.placement_recipe_path << "\n"
         << "lifecycle.timing_use_rc "
         << (lifecycle.timing_use_rc ? "true" : "false") << "\n"
         << "lifecycle.rc_min_routing_layer " << lifecycle.rc_min_routing_layer
         << "\n"
         << "lifecycle.target_density " << lifecycle.target_density << "\n"
         << "lifecycle.num_threads " << lifecycle.num_threads << "\n"
         << "lifecycle.is_standard_cell "
         << (lifecycle.is_standard_cell ? "true" : "false") << "\n"
         << "lifecycle.well_emit_mode " << lifecycle.well_emit_mode << "\n"
         << "lifecycle.enable_well_taps "
         << (lifecycle.enable_well_taps ? "true" : "false") << "\n"
         << "lifecycle.fixed_die " << lifecycle.fixed_die_grid.die_llx << " "
         << lifecycle.fixed_die_grid.die_lly << " "
         << lifecycle.fixed_die_grid.die_urx << " "
         << lifecycle.fixed_die_grid.die_ury << " "
         << lifecycle.fixed_die_grid.grid_x << " "
         << lifecycle.fixed_die_grid.grid_y << "\n";
  if (!output)
    return SetError(error, "failed while writing config " + file_name);
  return true;
}

} // namespace dali
