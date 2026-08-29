/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 *******************************************************************************/
#include "dali/timing/timing_snapshot.h"

#include <phydb/phydb.h>

#if PHYDB_USE_GALOIS
#include <galois/eda/utility/ExtNetlistAdaptor.h>
#endif

#include <algorithm>
#include <cmath>
#include <map>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "dali/common/misc.h"

namespace dali {

double TimingPathSnapshot::TotalDelay() const {
  return std::accumulate(steps.begin(), steps.end(), 0.0,
                         [](double delay, const TimingPathStep &step) {
                           return delay + step.delay;
                         });
}

std::string RelativeTimingConstraintSnapshot::SemanticIdentity() const {
  if (fast_path.root_pin.empty() || slow_path.root_pin.empty() ||
      fast_path.root_pin != slow_path.root_pin ||
      fast_path.terminal_pin.empty() || slow_path.terminal_pin.empty()) {
    return {};
  }
  return fast_path.root_pin + "|" + slow_path.terminal_pin + "|" +
         fast_path.terminal_pin;
}

std::vector<std::string>
FindDelayRepairCandidateNets(const TimingPathSnapshot &fast_path,
                             const TimingPathSnapshot &slow_path) {
  std::unordered_set<std::string> fast_path_nets;
  for (const TimingPathStep &step : fast_path.steps) {
    if (!step.net_name.empty())
      fast_path_nets.insert(step.net_name);
  }

  std::unordered_set<std::string> emitted_nets;
  std::vector<std::string> repair_candidates;
  for (const TimingPathStep &step : slow_path.steps) {
    if (!step.net_name.empty() &&
        fast_path_nets.find(step.net_name) == fast_path_nets.end() &&
        emitted_nets.insert(step.net_name).second) {
      repair_candidates.push_back(step.net_name);
    }
  }
  return repair_candidates;
}

/**
 * Pin-to-net index backing the recovery of timing edges with no net recorded.
 *
 * Built once per capture and consulted per edge. What it replaces was a scan of
 * every net for every edge, deliberately without an early exit because the
 * recovery only accepts an unambiguous connection and so had to see every
 * match. That is affordable once and ruinous per edge: on a 512-constraint
 * design it made a snapshot cost sixteen seconds and dominated every
 * global-placement iteration of a timing-driven run.
 */
class PinNetIndex {
public:
  explicit PinNetIndex(std::vector<phydb::Net> &nets) {
    for (int net_index = 0; net_index < static_cast<int>(nets.size());
         ++net_index) {
      for (const phydb::PhydbPin &pin : nets[net_index].GetPinsRef()) {
        pin_to_nets_[pin].push_back(net_index);
      }
    }
  }

  /**
   * Recover a missing timing-edge net from an unambiguous physical connection.
   *
   * Returns the one net carrying both pins, or -1 when none does or more than
   * one does. Intersecting the two pins' net lists sees exactly the matches the
   * full scan saw, so an edge resolves here iff it resolved before.
   */
  int Find(const phydb::PhydbPin &source,
           const phydb::PhydbPin &target) const {
    auto source_entry = pin_to_nets_.find(source);
    if (source_entry == pin_to_nets_.end()) return -1;
    auto target_entry = pin_to_nets_.find(target);
    if (target_entry == pin_to_nets_.end()) return -1;
    const std::vector<int> &target_nets = target_entry->second;
    int matched_net = -1;
    for (int net_index : source_entry->second) {
      if (std::find(target_nets.begin(), target_nets.end(), net_index) ==
          target_nets.end()) {
        continue;
      }
      if (matched_net >= 0) return -1;
      matched_net = net_index;
    }
    return matched_net;
  }

private:
  std::unordered_map<phydb::PhydbPin, std::vector<int>, phydb::PhydbPinHasher>
      pin_to_nets_;
};

/** Return whether a logical timing pin belongs to one declared meta site. */
static bool IsDeclaredDelaySitePin(const std::string &pin_name,
                                   const DelayRepairSite &site) {
  if (!site.adjustable)
    return false;
  const std::string &prefix = site.logical_path_prefix.empty()
                                  ? site.instance_name
                                  : site.logical_path_prefix;
  if (pin_name.size() < prefix.size() ||
      pin_name.compare(0, prefix.size(), prefix) != 0) {
    return false;
  }
  if (pin_name.size() == prefix.size())
    return true;
  const char boundary = pin_name[prefix.size()];
  return boundary == '.' || boundary == '[' || boundary == ':' ||
         boundary == '/' || boundary == '_' || boundary == '$';
}

static void
AppendDeclaredDelaySiteNames(const TimingPathSnapshot &path,
                             const std::vector<DelayRepairSite> &declared_sites,
                             std::unordered_set<std::string> *names,
                             std::vector<std::string> *ordered_names) {
  for (const TimingPathStep &step : path.steps) {
    for (const std::string &pin_name :
         {step.logical_source_pin, step.logical_target_pin}) {
      for (const DelayRepairSite &site : declared_sites) {
        if (IsDeclaredDelaySitePin(pin_name, site) &&
            names->insert(site.id).second) {
          ordered_names->push_back(site.id);
        }
      }
    }
  }
}

std::vector<std::string> FindDelayRepairCandidateSites(
    const TimingPathSnapshot &fast_path, const TimingPathSnapshot &slow_path,
    const std::vector<DelayRepairSite> &declared_sites) {
  std::unordered_set<std::string> fast_sites;
  std::vector<std::string> ignored_order;
  AppendDeclaredDelaySiteNames(fast_path, declared_sites, &fast_sites,
                               &ignored_order);

  std::unordered_set<std::string> slow_sites;
  std::vector<std::string> candidates;
  AppendDeclaredDelaySiteNames(slow_path, declared_sites, &slow_sites,
                               &candidates);
  candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                                  [&fast_sites](const std::string &site) {
                                    return fast_sites.find(site) !=
                                           fast_sites.end();
                                  }),
                   candidates.end());
  return candidates;
}

std::vector<TimingRepairSitePlanItem>
BuildTimingRepairSitePlan(const TimingSnapshot &snapshot,
                          const std::vector<DelayRepairSite> &declared_sites) {
  std::unordered_map<std::string, double> worst_slack_by_site;
  for (const RelativeTimingViolationSnapshot &violation :
       snapshot.relative_violations) {
    for (const std::string &site_id : violation.delay_repair_candidate_sites) {
      const auto [it, inserted] =
          worst_slack_by_site.emplace(site_id, violation.slack);
      if (!inserted)
        it->second = std::min(it->second, violation.slack);
    }
  }

  std::vector<TimingRepairSitePlanItem> plan;
  for (const DelayRepairSite &site : declared_sites) {
    if (!site.adjustable)
      continue;
    const auto it = worst_slack_by_site.find(site.id);
    if (it == worst_slack_by_site.end())
      continue;
    plan.push_back({site.id, site.process_name, site.instance_name,
                    site.parameter_name, site.initial_parameter_value,
                    it->second});
  }
  return plan;
}

static std::unordered_set<std::string>
CollectPathNets(const TimingPathSnapshot &path) {
  std::unordered_set<std::string> nets;
  for (const TimingPathStep &step : path.steps) {
    if (!step.net_name.empty())
      nets.insert(step.net_name);
  }
  return nets;
}

static std::unordered_map<std::string, double>
CollectPathNetDelays(const TimingPathSnapshot &path) {
  std::unordered_map<std::string, double> net_delays;
  for (const TimingPathStep &step : path.steps) {
    if (!step.net_name.empty())
      net_delays[step.net_name] += step.delay;
  }
  return net_delays;
}

/** Add an observed pin once while preserving deterministic path order. */
static void AppendUniqueValue(const std::string &value,
                              std::vector<std::string> *values) {
  if (value.empty() ||
      std::find(values->begin(), values->end(), value) != values->end()) {
    return;
  }
  values->push_back(value);
}

/**
 * Record the physical endpoints observed for each net edge in a timing path.
 *
 * A well-formed net normally has one driver, but retaining all observed
 * sources makes an inconsistent or multi-driver topology visible to the host
 * instead of silently choosing one endpoint.
 */
static void CollectPathNetEndpoints(
    const TimingPathSnapshot &path,
    std::unordered_map<std::string, TimingNetCandidateSnapshot> *net_impacts) {
  for (const TimingPathStep &step : path.steps) {
    if (step.net_name.empty())
      continue;
    TimingNetCandidateSnapshot &impact = (*net_impacts)[step.net_name];
    impact.net_name = step.net_name;
    AppendUniqueValue(step.source_pin, &impact.driver_pins);
    AppendUniqueValue(step.target_pin, &impact.load_pins);
    AppendUniqueValue(step.logical_net_name, &impact.logical_net_names);
    AppendUniqueValue(step.logical_source_pin, &impact.logical_driver_pins);
    AppendUniqueValue(step.logical_target_pin, &impact.logical_load_pins);
  }
}

static std::vector<TimingNetCandidateSnapshot> BuildDirectionalCandidates(
    const std::vector<RelativeTimingConstraintSnapshot> &constraints,
    bool improve_slow_only_nets) {
  std::unordered_set<std::string> candidate_names;
  std::unordered_map<std::string, TimingNetCandidateSnapshot> net_impacts;
  for (const RelativeTimingConstraintSnapshot &constraint : constraints) {
    CollectPathNetEndpoints(constraint.fast_path, &net_impacts);
    CollectPathNetEndpoints(constraint.slow_path, &net_impacts);
    const std::unordered_set<std::string> fast_nets =
        CollectPathNets(constraint.fast_path);
    const std::unordered_set<std::string> slow_nets =
        CollectPathNets(constraint.slow_path);
    const std::unordered_map<std::string, double> fast_net_delays =
        CollectPathNetDelays(constraint.fast_path);
    const std::unordered_map<std::string, double> slow_net_delays =
        CollectPathNetDelays(constraint.slow_path);
    const std::unordered_set<std::string> &improved_nets =
        improve_slow_only_nets ? slow_nets : fast_nets;
    const std::unordered_set<std::string> &degraded_nets =
        improve_slow_only_nets ? fast_nets : slow_nets;
    const std::unordered_map<std::string, double> &improved_net_delays =
        improve_slow_only_nets ? slow_net_delays : fast_net_delays;
    const std::unordered_map<std::string, double> &degraded_net_delays =
        improve_slow_only_nets ? fast_net_delays : slow_net_delays;
    for (const std::string &net_name : improved_nets) {
      if (degraded_nets.find(net_name) != degraded_nets.end())
        continue;
      TimingNetCandidateSnapshot &impact = net_impacts[net_name];
      impact.net_name = net_name;
      impact.improved_constraint_ids.push_back(constraint.constraint_id);
      impact.improved_path_delay += improved_net_delays.at(net_name);
      if (constraint.slack < 0.0) {
        candidate_names.insert(net_name);
        impact.improved_negative_slack += -constraint.slack;
      }
    }
    for (const std::string &net_name : degraded_nets) {
      if (improved_nets.find(net_name) != improved_nets.end())
        continue;
      TimingNetCandidateSnapshot &impact = net_impacts[net_name];
      impact.net_name = net_name;
      impact.degraded_constraint_ids.push_back(constraint.constraint_id);
      impact.degraded_path_delay += degraded_net_delays.at(net_name);
      if (constraint.slack < 0.0) {
        impact.degraded_negative_slack += -constraint.slack;
      }
    }
  }

  std::vector<TimingNetCandidateSnapshot> candidates;
  candidates.reserve(candidate_names.size());
  for (const std::string &net_name : candidate_names) {
    std::sort(net_impacts[net_name].driver_pins.begin(),
              net_impacts[net_name].driver_pins.end());
    std::sort(net_impacts[net_name].load_pins.begin(),
              net_impacts[net_name].load_pins.end());
    std::sort(net_impacts[net_name].logical_net_names.begin(),
              net_impacts[net_name].logical_net_names.end());
    std::sort(net_impacts[net_name].logical_driver_pins.begin(),
              net_impacts[net_name].logical_driver_pins.end());
    std::sort(net_impacts[net_name].logical_load_pins.begin(),
              net_impacts[net_name].logical_load_pins.end());
    candidates.push_back(std::move(net_impacts[net_name]));
  }

  std::sort(
      candidates.begin(), candidates.end(),
      [](const TimingNetCandidateSnapshot &left,
         const TimingNetCandidateSnapshot &right) {
        if (left.degraded_negative_slack != right.degraded_negative_slack) {
          return left.degraded_negative_slack < right.degraded_negative_slack;
        }
        if (left.improved_negative_slack != right.improved_negative_slack) {
          return left.improved_negative_slack > right.improved_negative_slack;
        }
        if (left.improved_path_delay != right.improved_path_delay) {
          return left.improved_path_delay > right.improved_path_delay;
        }
        return left.net_name < right.net_name;
      });
  return candidates;
}

std::vector<TimingNetCandidateSnapshot> BuildDelayRepairCandidates(
    const std::vector<RelativeTimingConstraintSnapshot> &constraints) {
  return BuildDirectionalCandidates(constraints, true);
}

std::vector<TimingNetCandidateSnapshot> BuildFastPathPlacementCandidates(
    const std::vector<RelativeTimingConstraintSnapshot> &constraints) {
  std::vector<TimingNetCandidateSnapshot> candidates =
      BuildDirectionalCandidates(constraints, false);
  std::sort(
      candidates.begin(), candidates.end(),
      [](const TimingNetCandidateSnapshot &left,
         const TimingNetCandidateSnapshot &right) {
        const bool left_has_conflict = !left.degraded_constraint_ids.empty();
        const bool right_has_conflict = !right.degraded_constraint_ids.empty();
        if (left_has_conflict != right_has_conflict) {
          return !left_has_conflict;
        }
        if (left.improved_path_delay != right.improved_path_delay) {
          return left.improved_path_delay > right.improved_path_delay;
        }
        if (left.improved_negative_slack != right.improved_negative_slack) {
          return left.improved_negative_slack > right.improved_negative_slack;
        }
        return left.net_name < right.net_name;
      });
  return candidates;
}

static std::string EscapeJsonString(const std::string &text) {
  std::string escaped;
  escaped.reserve(text.size());
  for (char character : text) {
    switch (character) {
    case '\\':
      escaped += "\\\\";
      break;
    case '"':
      escaped += "\\\"";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      escaped += character;
    }
  }
  return escaped;
}

static void WriteJsonIntArray(std::ostream &output,
                              const std::vector<int> &values) {
  output << '[';
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0)
      output << ", ";
    output << values[index];
  }
  output << ']';
}

static void WriteJsonStringArray(std::ostream &output,
                                 const std::vector<std::string> &values) {
  output << '[';
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index != 0)
      output << ", ";
    output << '"' << EscapeJsonString(values[index]) << '"';
  }
  output << ']';
}

static void WriteTimingPathJson(std::ostream &output,
                                const TimingPathSnapshot &path,
                                const std::string &indentation) {
  output << "{\n"
         << indentation << "  \"total_delay\": " << path.TotalDelay() << ",\n"
         << indentation << "  \"steps\": [\n";
  for (std::size_t index = 0; index < path.steps.size(); ++index) {
    const TimingPathStep &step = path.steps[index];
    output << indentation << "    {\n"
           << indentation << "      \"source_pin\": \""
           << EscapeJsonString(step.source_pin) << "\",\n"
           << indentation << "      \"target_pin\": \""
           << EscapeJsonString(step.target_pin) << "\",\n"
           << indentation << "      \"net_name\": \""
           << EscapeJsonString(step.net_name) << "\",\n"
           << indentation << "      \"delay\": " << step.delay << ",\n"
           << indentation << "      \"logical_source_pin\": \""
           << EscapeJsonString(step.logical_source_pin) << "\",\n"
           << indentation << "      \"logical_target_pin\": \""
           << EscapeJsonString(step.logical_target_pin) << "\",\n"
           << indentation << "      \"logical_net_name\": \""
           << EscapeJsonString(step.logical_net_name) << "\"\n"
           << indentation << "    }";
    if (index + 1 != path.steps.size())
      output << ',';
    output << '\n';
  }
  output << indentation << "  ]\n" << indentation << '}';
}

static void WriteRelativeViolationsJson(
    std::ostream &output,
    const std::vector<RelativeTimingViolationSnapshot> &violations,
    const std::string &indentation) {
  output << "[\n";
  for (std::size_t index = 0; index < violations.size(); ++index) {
    const RelativeTimingViolationSnapshot &violation = violations[index];
    output << indentation << "{\n"
           << indentation << "  \"constraint_id\": " << violation.constraint_id
           << ",\n"
           << indentation << "  \"slack\": " << violation.slack << ",\n"
           << indentation << "  \"fast_path\": ";
    WriteTimingPathJson(output, violation.fast_path, indentation + "  ");
    output << ",\n" << indentation << "  \"slow_path\": ";
    WriteTimingPathJson(output, violation.slow_path, indentation + "  ");
    output << ",\n" << indentation << "  \"delay_repair_candidate_nets\": ";
    WriteJsonStringArray(output, violation.delay_repair_candidate_nets);
    output << ",\n" << indentation << "  \"delay_repair_candidate_sites\": ";
    WriteJsonStringArray(output, violation.delay_repair_candidate_sites);
    output << '\n' << indentation << '}';
    if (index + 1 != violations.size())
      output << ',';
    output << '\n';
  }
  output << indentation.substr(0, indentation.size() - 2) << ']';
}

static void WriteTimingCandidatesJson(
    std::ostream &output,
    const std::vector<TimingNetCandidateSnapshot> &candidates,
    const std::string &indentation) {
  output << "[\n";
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    const TimingNetCandidateSnapshot &candidate = candidates[index];
    output << indentation << "{\n"
           << indentation << "  \"net_name\": \""
           << EscapeJsonString(candidate.net_name) << "\",\n"
           << indentation << "  \"logical_net_names\": ";
    WriteJsonStringArray(output, candidate.logical_net_names);
    output << ",\n" << indentation << "  \"driver_pins\": ";
    WriteJsonStringArray(output, candidate.driver_pins);
    output << ",\n" << indentation << "  \"load_pins\": ";
    WriteJsonStringArray(output, candidate.load_pins);
    output << ",\n" << indentation << "  \"logical_driver_pins\": ";
    WriteJsonStringArray(output, candidate.logical_driver_pins);
    output << ",\n" << indentation << "  \"logical_load_pins\": ";
    WriteJsonStringArray(output, candidate.logical_load_pins);
    output << ",\n"
           << indentation << "  \"improved_negative_slack\": "
           << candidate.improved_negative_slack << ",\n"
           << indentation << "  \"degraded_negative_slack\": "
           << candidate.degraded_negative_slack << ",\n"
           << indentation
           << "  \"improved_path_delay\": " << candidate.improved_path_delay
           << ",\n"
           << indentation
           << "  \"degraded_path_delay\": " << candidate.degraded_path_delay
           << ",\n"
           << indentation << "  \"improved_constraint_ids\": ";
    WriteJsonIntArray(output, candidate.improved_constraint_ids);
    output << ",\n" << indentation << "  \"degraded_constraint_ids\": ";
    WriteJsonIntArray(output, candidate.degraded_constraint_ids);
    output << "\n" << indentation << '}';
    if (index + 1 != candidates.size())
      output << ',';
    output << '\n';
  }
  output << indentation.substr(0, indentation.size() - 2) << ']';
}

bool WriteTimingRepairPlanJson(const TimingSnapshot &snapshot,
                               const std::string &file_name) {
  std::ofstream output(file_name);
  if (!output)
    return false;

  output << std::setprecision(17);
  output << "{\n"
         << "  \"schema_version\": 7,\n"
         << "  \"time_unit\": \"timing_library_unit\",\n"
         << "  \"has_critical_cycle\": "
         << (snapshot.has_critical_cycle ? "true" : "false") << ",\n"
         << "  \"critical_cycle_period\": " << snapshot.critical_cycle_period
         << ",\n"
         << "  \"critical_cycle_unroll_factor\": "
         << snapshot.critical_cycle_unroll_factor << ",\n"
         << "  \"relative_constraint_count\": "
         << snapshot.relative_constraint_count << ",\n"
         << "  \"worst_relative_slack\": " << snapshot.worst_relative_slack
         << ",\n"
         << "  \"total_negative_slack\": "
         << snapshot.relative_total_negative_slack << ",\n"
         << "  \"violation_count\": " << snapshot.relative_violations.size()
         << ",\n"
         << "  \"relative_violations\": ";
  WriteRelativeViolationsJson(output, snapshot.relative_violations, "    ");
  output << ",\n  \"delay_site_slack\": [\n";
  for (std::size_t index = 0; index < snapshot.delay_site_slack.size();
       ++index) {
    const DelaySiteSlackSnapshot &entry = snapshot.delay_site_slack[index];
    output << "    {\"site_id\": \"" << entry.site_id
           << "\", \"worst_slack\": " << entry.worst_slack
           << ", \"constraint_count\": " << entry.constraint_count
           << ", \"violating_count\": " << entry.violating_count << "}";
    if (index + 1 != snapshot.delay_site_slack.size()) output << ',';
    output << '\n';
  }
  output << "  ]";
  output << ",\n"
         << "  \"delay_repair_candidates\": ";
  WriteTimingCandidatesJson(output, snapshot.delay_repair_candidates, "    ");
  output << ",\n  \"fast_path_placement_candidates\": ";
  WriteTimingCandidatesJson(output, snapshot.fast_path_placement_candidates,
                            "    ");
  output << "\n}\n";
  return output.good();
}

bool WriteTimingConstraintIdentitiesJson(const TimingSnapshot &snapshot,
                                         const std::string &file_name) {
  if (snapshot.relative_constraint_count !=
      static_cast<int>(snapshot.relative_constraints.size())) {
    LOG(error) << "Constraint identity count mismatch: reported "
               << snapshot.relative_constraint_count << ", captured "
               << snapshot.relative_constraints.size() << "\n";
    return false;
  }
  std::vector<const RelativeTimingConstraintSnapshot *> constraints(
      snapshot.relative_constraints.size(), nullptr);
  std::unordered_set<std::string> identities;
  for (const RelativeTimingConstraintSnapshot &constraint :
       snapshot.relative_constraints) {
    if (constraint.constraint_id < 0 ||
        constraint.constraint_id >= snapshot.relative_constraint_count ||
        constraints[constraint.constraint_id] != nullptr) {
      LOG(error) << "Constraint identity has invalid or duplicate numeric ID "
                 << constraint.constraint_id << "\n";
      return false;
    }
    const std::string identity = constraint.SemanticIdentity();
    if (identity.empty()) {
      LOG(error) << "Constraint " << constraint.constraint_id
                 << " has unresolved or mismatched endpoints: fast root '"
                 << constraint.fast_path.root_pin << "', slow root '"
                 << constraint.slow_path.root_pin << "', fast terminal '"
                 << constraint.fast_path.terminal_pin << "', slow terminal '"
                 << constraint.slow_path.terminal_pin << "'\n";
      return false;
    }
    if (!identities.insert(identity).second) {
      LOG(error) << "Constraint " << constraint.constraint_id
                 << " repeats semantic identity '" << identity << "'\n";
      return false;
    }
    constraints[constraint.constraint_id] = &constraint;
  }

  std::ofstream output(file_name);
  if (!output) return false;
  output << std::setprecision(17);
  output << "{\n  \"schema_version\": 1,\n"
         << "  \"constraint_count\": " << constraints.size() << ",\n"
         << "  \"constraints\": [\n";
  for (std::size_t index = 0; index < constraints.size(); ++index) {
    const RelativeTimingConstraintSnapshot &constraint = *constraints[index];
    output << "    {\"constraint_id\": " << constraint.constraint_id
           << ", \"identity\": \""
           << EscapeJsonString(constraint.SemanticIdentity())
           << "\", \"root_pin\": \""
           << EscapeJsonString(constraint.fast_path.root_pin)
           << "\", \"slow_terminal_pin\": \""
           << EscapeJsonString(constraint.slow_path.terminal_pin)
           << "\", \"fast_terminal_pin\": \""
           << EscapeJsonString(constraint.fast_path.terminal_pin)
           << "\", \"slack\": " << constraint.slack << "}";
    if (index + 1 != constraints.size()) output << ',';
    output << '\n';
  }
  output << "  ]\n}\n";
  return output.good();
}

const char *ToString(ConstraintAttributionState state) {
  switch (state) {
  case ConstraintAttributionState::kUnique:
    return "unique";
  case ConstraintAttributionState::kAmbiguous:
    return "ambiguous";
  case ConstraintAttributionState::kUnattributed:
    break;
  }
  return "unattributed";
}

bool DecomposeTimingPath(const TimingPathSnapshot &path,
                         TimingPathDecomposition *decomposition,
                         std::string *error) {
  TimingPathDecomposition result;
  for (const TimingPathStep &step : path.steps) {
    if (!std::isfinite(step.delay)) {
      if (error != nullptr) {
        *error = "step " + step.source_pin + " -> " + step.target_pin +
                 " has a non-finite delay";
      }
      return false;
    }
    if (step.delay < 0.0) {
      if (error != nullptr) {
        *error = "step " + step.source_pin + " -> " + step.target_pin +
                 " has a negative delay";
      }
      return false;
    }
    if (step.net_name.empty()) {
      result.cell_delay += step.delay;
      ++result.cell_steps;
    } else {
      result.wire_delay += step.delay;
      ++result.wire_steps;
    }
  }
  result.total_delay = result.cell_delay + result.wire_delay;
  result.total_steps = result.cell_steps + result.wire_steps;
  if (!std::isfinite(result.total_delay)) {
    if (error != nullptr) *error = "path total delay is not finite";
    return false;
  }
  *decomposition = result;
  return true;
}

bool DecomposeTimingConstraint(
    const RelativeTimingConstraintSnapshot &constraint,
    TimingConstraintDecomposition *decomposition, std::string *error) {
  TimingConstraintDecomposition result;
  result.constraint_id = constraint.constraint_id;
  result.semantic_identity = constraint.SemanticIdentity();
  if (!DecomposeTimingPath(constraint.fast_path, &result.fast, error)) {
    if (error != nullptr) *error = "fast path: " + *error;
    return false;
  }
  if (!DecomposeTimingPath(constraint.slow_path, &result.slow, error)) {
    if (error != nullptr) *error = "slow path: " + *error;
    return false;
  }
  if (!std::isfinite(constraint.slack)) {
    if (error != nullptr) *error = "reported slack is not finite";
    return false;
  }
  result.slack = constraint.slack;
  result.reconciliation_residual =
      (result.slow.total_delay - result.fast.total_delay) - constraint.slack;
  result.candidate_sites = constraint.delay_repair_candidate_sites;
  if (result.candidate_sites.size() == 1) {
    result.attribution = ConstraintAttributionState::kUnique;
    result.unique_site = result.candidate_sites.front();
  } else if (result.candidate_sites.size() > 1) {
    result.attribution = ConstraintAttributionState::kAmbiguous;
  } else {
    result.attribution = ConstraintAttributionState::kUnattributed;
  }
  *decomposition = result;
  return true;
}

/**
 * Serialize one path's decomposition as a JSON object body.
 *
 * Numbers reach here already checked finite, so no NaN or infinity can be
 * emitted as a bare token that no JSON parser would accept.
 */
static void WritePathDecompositionJson(std::ostream &output,
                                       const TimingPathDecomposition &path) {
  output << "{\"cell_delay_ps\": " << path.cell_delay
         << ", \"wire_delay_ps\": " << path.wire_delay
         << ", \"total_delay_ps\": " << path.total_delay
         << ", \"cell_steps\": " << path.cell_steps
         << ", \"wire_steps\": " << path.wire_steps
         << ", \"total_steps\": " << path.total_steps << '}';
}

bool WriteTimingDecompositionJson(const TimingSnapshot &snapshot,
                                  const std::string &file_name) {
  if (snapshot.relative_constraint_count !=
      static_cast<int>(snapshot.relative_constraints.size())) {
    LOG(error) << "Timing decomposition count mismatch: reported "
               << snapshot.relative_constraint_count << ", captured "
               << snapshot.relative_constraints.size() << "\n";
    return false;
  }
  std::vector<TimingConstraintDecomposition> decompositions;
  decompositions.reserve(snapshot.relative_constraints.size());
  std::unordered_set<std::string> identities;
  unsigned long long digest = 0;
  double worst_residual = 0.0;
  for (const RelativeTimingConstraintSnapshot &constraint :
       snapshot.relative_constraints) {
    TimingConstraintDecomposition decomposition;
    std::string error;
    if (!DecomposeTimingConstraint(constraint, &decomposition, &error)) {
      LOG(error) << "Constraint " << constraint.constraint_id
                 << " cannot be decomposed: " << error << "\n";
      return false;
    }
    if (decomposition.semantic_identity.empty()) {
      LOG(error) << "Constraint " << constraint.constraint_id
                 << " has unresolved or mismatched endpoints\n";
      return false;
    }
    if (!identities.insert(decomposition.semantic_identity).second) {
      LOG(error) << "Constraint " << constraint.constraint_id
                 << " repeats semantic identity '"
                 << decomposition.semantic_identity << "'\n";
      return false;
    }
    if (std::abs(decomposition.reconciliation_residual) >
        kTimingDecompositionTolerancePs) {
      LOG(error) << "Constraint " << constraint.constraint_id
                 << " does not reconcile: (slow " << decomposition.slow.total_delay
                 << " - fast " << decomposition.fast.total_delay
                 << ") - slack " << decomposition.slack << " = "
                 << decomposition.reconciliation_residual << " ps, tolerance "
                 << kTimingDecompositionTolerancePs << " ps\n";
      return false;
    }
    worst_residual = std::max(
        worst_residual, std::abs(decomposition.reconciliation_residual));
    unsigned long long entry = 1469598103934665603ull;
    for (char character : (std::to_string(decomposition.constraint_id) + "=" +
                           decomposition.semantic_identity)) {
      entry ^= static_cast<unsigned long long>(
          static_cast<unsigned char>(character));
      entry *= 1099511628211ull;
    }
    digest += entry;
    decompositions.push_back(std::move(decomposition));
  }
  std::sort(decompositions.begin(), decompositions.end(),
            [](const TimingConstraintDecomposition &left,
               const TimingConstraintDecomposition &right) {
              return left.constraint_id < right.constraint_id;
            });

  std::ofstream output(file_name);
  if (!output) {
    LOG(error) << "Cannot open timing decomposition destination: " << file_name
               << "\n";
    return false;
  }
  output << std::setprecision(17);
  output << "{\n  \"schema_version\": 1,\n"
         << "  \"constraint_count\": " << decompositions.size() << ",\n"
         << "  \"identity_digest\": \"" << digest << "\",\n"
         << "  \"reconciliation_tolerance_ps\": "
         << kTimingDecompositionTolerancePs << ",\n"
         << "  \"worst_reconciliation_residual_ps\": " << worst_residual
         << ",\n  \"constraints\": [\n";
  for (std::size_t index = 0; index < decompositions.size(); ++index) {
    const TimingConstraintDecomposition &item = decompositions[index];
    output << "    {\"constraint_id\": " << item.constraint_id
           << ", \"identity\": \"" << EscapeJsonString(item.semantic_identity)
           << "\", \"fast\": ";
    WritePathDecompositionJson(output, item.fast);
    output << ", \"slow\": ";
    WritePathDecompositionJson(output, item.slow);
    output << ", \"slack_ps\": " << item.slack
           << ", \"reconciliation_residual_ps\": "
           << item.reconciliation_residual << ", \"attribution\": \""
           << ToString(item.attribution) << "\", \"unique_site\": \""
           << EscapeJsonString(item.unique_site) << "\", \"candidate_sites\": ";
    WriteJsonStringArray(output, item.candidate_sites);
    output << '}';
    if (index + 1 != decompositions.size()) output << ',';
    output << '\n';
  }
  output << "  ]\n}\n";
  return output.good();
}

static std::string CanonicalizeReplaceableSiteEndpoint(
    const std::string &endpoint,
    const std::vector<std::string> &replaceable_site_prefixes) {
  const std::size_t separator = endpoint.rfind(':');
  if (separator == std::string::npos) return endpoint;
  const std::string component = endpoint.substr(0, separator);
  for (const std::string &prefix : replaceable_site_prefixes) {
    if (!prefix.empty() && component.compare(0, prefix.size(), prefix) == 0 &&
        component.size() > prefix.size() && component[prefix.size()] == '_') {
      return "delay-site:" + prefix + endpoint.substr(separator);
    }
  }
  return endpoint;
}

void CanonicalizeReplaceableSiteEndpoints(
    TimingSnapshot *snapshot,
    const std::vector<std::string> &replaceable_site_prefixes) {
  if (snapshot == nullptr) return;
  for (RelativeTimingConstraintSnapshot &constraint :
       snapshot->relative_constraints) {
    constraint.fast_path.root_pin = CanonicalizeReplaceableSiteEndpoint(
        constraint.fast_path.root_pin, replaceable_site_prefixes);
    constraint.slow_path.root_pin = CanonicalizeReplaceableSiteEndpoint(
        constraint.slow_path.root_pin, replaceable_site_prefixes);
    constraint.fast_path.terminal_pin = CanonicalizeReplaceableSiteEndpoint(
        constraint.fast_path.terminal_pin, replaceable_site_prefixes);
    constraint.slow_path.terminal_pin = CanonicalizeReplaceableSiteEndpoint(
        constraint.slow_path.terminal_pin, replaceable_site_prefixes);
  }
}

static std::string ResolveTimingPinName(phydb::PhyDB *phy_db,
                                        phydb::PhydbPin pin) {
  if (!pin.IsValid()) return {};
  if (pin.IsComponentPin()) return phy_db->GetFullCompPinName(pin);
  auto &io_pins = phy_db->design().GetIoPinsRef();
  if (pin.PinId() < 0 || pin.PinId() >= static_cast<int>(io_pins.size())) {
    return {};
  }
  return io_pins[pin.PinId()].GetName();
}

TimingPathSnapshot
TimingSnapshotBuilder::CapturePath(phydb::PhydbPath &path,
                                   const PinNetIndex &pin_net_index) const {
  TimingPathSnapshot snapshot;
#if PHYDB_USE_GALOIS
  phydb::PhydbPin source = path.root;
  snapshot.root_pin = ResolveTimingPinName(phy_db_, source);
  snapshot.terminal_pin = snapshot.root_pin;
  auto &nets = phy_db_->design().GetNetsRef();
  phydb::ActPhyDBTimingAPI &timing_api = phy_db_->GetTimingApi();
  galois::eda::utility::ExtNetlistAdaptor *adaptor =
      timing_api.GetNetlistAdaptor();
  snapshot.steps.reserve(path.edges.size());
  for (const phydb::PhydbTimingEdge &edge : path.edges) {
    phydb::PhydbPin target = edge.target;
    TimingPathStep step;
    step.source_pin = phy_db_->GetFullCompPinName(source);
    step.target_pin = phy_db_->GetFullCompPinName(target);
    if (adaptor != nullptr) {
      void *act_source = timing_api.PhydbCompPin2ActPtr(source);
      void *act_target = timing_api.PhydbCompPin2ActPtr(target);
      if (act_source != nullptr) {
        step.logical_source_pin = adaptor->getFullName4Pin(act_source);
      }
      if (act_target != nullptr) {
        step.logical_target_pin = adaptor->getFullName4Pin(act_target);
      }
    }
    int net_index = edge.net_index;
    if (net_index < 0 || net_index >= static_cast<int>(nets.size())) {
      net_index = pin_net_index.Find(source, target);
    }
    if (net_index >= 0 && net_index < static_cast<int>(nets.size())) {
      step.net_name = nets[net_index].GetName();
      if (adaptor != nullptr) {
        void *act_net = timing_api.PhydbNetId2ActPtr(net_index);
        if (act_net != nullptr) {
          step.logical_net_name = adaptor->getFullName4Net(act_net);
        }
      }
    }
    step.delay = edge.delay;
    snapshot.steps.push_back(std::move(step));
    source = target;
    snapshot.terminal_pin = ResolveTimingPinName(phy_db_, source);
  }
#else
  (void)path;
  (void)pin_net_index;
#endif
  return snapshot;
}

TimingSnapshot TimingSnapshotBuilder::Capture() const {
  DaliExpects(phy_db_ != nullptr, "Cannot capture timing without PhyDB");
  TimingSnapshot snapshot;
#if PHYDB_USE_GALOIS
  static const std::vector<DelayRepairSite> kNoDeclaredDelaySites;
  const std::vector<DelayRepairSite> &declared_delay_sites =
      declared_delay_sites_ == nullptr ? kNoDeclaredDelaySites
                                       : *declared_delay_sites_;
  phydb::ActPhyDBTimingAPI &timing_api = phy_db_->GetTimingApi();
  if (timing_api.ReadyForCriticalCycleTiming()) {
    snapshot.has_critical_cycle = timing_api.GetCriticalCyclePeriod(
        &snapshot.critical_cycle_period,
        &snapshot.critical_cycle_unroll_factor);
    if (snapshot.has_critical_cycle && timing_api.ReadyForCriticalCycleNets()) {
      std::vector<phydb::PhydbNetTimingStep> net_steps;
      timing_api.GetCriticalCycleNetTiming(net_steps);
      auto &nets = phy_db_->design().GetNetsRef();
      snapshot.critical_cycle_nets.reserve(net_steps.size());
      for (const phydb::PhydbNetTimingStep &net_step : net_steps) {
        if (net_step.net_index < 0 ||
            net_step.net_index >= static_cast<int>(nets.size())) {
          continue;
        }
        snapshot.critical_cycle_nets.push_back(
            {nets[net_step.net_index].GetName(), net_step.delay});
      }
    }
  }

  if (timing_api.ReadyForTimingDriven()) {
    const PinNetIndex pin_net_index(phy_db_->design().GetNetsRef());
    snapshot.relative_constraint_count = timing_api.GetNumConstraints();
    std::vector<RelativeTimingConstraintSnapshot> constraints;
    constraints.reserve(snapshot.relative_constraint_count);
    for (int constraint_id = 0;
         constraint_id < snapshot.relative_constraint_count; ++constraint_id) {
      const double slack = timing_api.GetSlack(constraint_id);
      if (snapshot.worst_relative_constraint_id < 0 ||
          slack < snapshot.worst_relative_slack) {
        snapshot.worst_relative_constraint_id = constraint_id;
        snapshot.worst_relative_slack = slack;
      }
      RelativeTimingConstraintSnapshot constraint;
      constraint.constraint_id = constraint_id;
      constraint.slack = slack;
      phydb::PhydbPath fast_path;
      phydb::PhydbPath slow_path;
      timing_api.GetFastWitness(constraint_id, fast_path);
      timing_api.GetSlowWitness(constraint_id, slow_path);
      constraint.fast_path = CapturePath(fast_path, pin_net_index);
      constraint.slow_path = CapturePath(slow_path, pin_net_index);
      phydb::PhydbPin root;
      phydb::PhydbPin fast_terminal;
      phydb::PhydbPin slow_terminal;
      if (timing_api.GetConstraintEndpoints(constraint_id, root, fast_terminal,
                                            slow_terminal)) {
        const std::string root_name = ResolveTimingPinName(phy_db_, root);
        constraint.fast_path.root_pin = root_name;
        constraint.slow_path.root_pin = root_name;
        constraint.fast_path.terminal_pin =
            ResolveTimingPinName(phy_db_, fast_terminal);
        constraint.slow_path.terminal_pin =
            ResolveTimingPinName(phy_db_, slow_terminal);
      }
      constraint.delay_repair_candidate_nets = FindDelayRepairCandidateNets(
          constraint.fast_path, constraint.slow_path);
      constraint.delay_repair_candidate_sites = FindDelayRepairCandidateSites(
          constraint.fast_path, constraint.slow_path, declared_delay_sites);
      constraints.push_back(std::move(constraint));
      if (slack < 0.0)
        snapshot.relative_total_negative_slack += slack;
    }
    // Every declared site, not only the violating ones. A site whose
    // constraints all pass still carries information the sizing loop needs:
    // how much slack it could give back by shrinking.
    std::map<std::string, DelaySiteSlackSnapshot> site_slack;
    for (const RelativeTimingConstraintSnapshot &constraint : constraints) {
      for (const std::string &site : constraint.delay_repair_candidate_sites) {
        auto insertion = site_slack.emplace(site, DelaySiteSlackSnapshot{});
        DelaySiteSlackSnapshot &entry = insertion.first->second;
        if (insertion.second) {
          entry.site_id = site;
          entry.worst_slack = constraint.slack;
        } else {
          entry.worst_slack = std::min(entry.worst_slack, constraint.slack);
        }
        entry.constraint_count += 1;
        if (constraint.slack < 0.0) entry.violating_count += 1;
      }
    }
    for (auto &entry : site_slack) {
      snapshot.delay_site_slack.push_back(entry.second);
    }

    for (const RelativeTimingConstraintSnapshot &constraint : constraints) {
      if (constraint.slack < 0.0) {
        snapshot.relative_violations.push_back(constraint);
      }
    }
    std::sort(snapshot.relative_violations.begin(),
              snapshot.relative_violations.end(),
              [](const RelativeTimingViolationSnapshot &left,
                 const RelativeTimingViolationSnapshot &right) {
                return left.slack < right.slack;
              });
    snapshot.delay_repair_candidates = BuildDelayRepairCandidates(constraints);
    snapshot.fast_path_placement_candidates =
        BuildFastPathPlacementCandidates(constraints);
    snapshot.relative_constraints = std::move(constraints);
  }
#endif
  return snapshot;
}

TimingSnapshot TimingSnapshotBuilder::CaptureConstraintIdentities() const {
  DaliExpects(phy_db_ != nullptr, "Cannot capture timing without PhyDB");
  TimingSnapshot snapshot;
#if PHYDB_USE_GALOIS
  phydb::ActPhyDBTimingAPI &timing_api = phy_db_->GetTimingApi();
  if (!timing_api.ReadyForTimingDriven()) return snapshot;
  snapshot.relative_constraint_count = timing_api.GetNumConstraints();
  snapshot.relative_constraints.reserve(snapshot.relative_constraint_count);
  for (int constraint_id = 0;
       constraint_id < snapshot.relative_constraint_count; ++constraint_id) {
    RelativeTimingConstraintSnapshot constraint;
    constraint.constraint_id = constraint_id;
    constraint.slack = timing_api.GetSlack(constraint_id);
    if (snapshot.worst_relative_constraint_id < 0 ||
        constraint.slack < snapshot.worst_relative_slack) {
      snapshot.worst_relative_constraint_id = constraint_id;
      snapshot.worst_relative_slack = constraint.slack;
    }
    if (constraint.slack < 0.0) {
      snapshot.relative_total_negative_slack += constraint.slack;
    }
    phydb::PhydbPin root;
    phydb::PhydbPin fast_terminal;
    phydb::PhydbPin slow_terminal;
    if (timing_api.GetConstraintEndpoints(constraint_id, root, fast_terminal,
                                          slow_terminal)) {
      const std::string root_name = ResolveTimingPinName(phy_db_, root);
      constraint.fast_path.root_pin = root_name;
      constraint.slow_path.root_pin = root_name;
      constraint.fast_path.terminal_pin =
          ResolveTimingPinName(phy_db_, fast_terminal);
      constraint.slow_path.terminal_pin =
          ResolveTimingPinName(phy_db_, slow_terminal);
    }
    snapshot.relative_constraints.push_back(std::move(constraint));
  }
  for (const RelativeTimingConstraintSnapshot &constraint :
       snapshot.relative_constraints) {
    if (constraint.slack < 0.0) {
      snapshot.relative_violations.push_back(constraint);
    }
  }
  std::sort(snapshot.relative_violations.begin(),
            snapshot.relative_violations.end(),
            [](const RelativeTimingViolationSnapshot &left,
               const RelativeTimingViolationSnapshot &right) {
              return left.slack < right.slack;
            });
#endif
  return snapshot;
}

TimingSnapshot TimingSnapshotBuilder::CaptureConstraintEndpointIdentities()
    const {
  DaliExpects(phy_db_ != nullptr, "Cannot capture timing without PhyDB");
  TimingSnapshot snapshot;
#if PHYDB_USE_GALOIS
  phydb::ActPhyDBTimingAPI &timing_api = phy_db_->GetTimingApi();
  if (!timing_api.ReadyForTimingDriven()) return snapshot;
  snapshot.relative_constraint_count = timing_api.GetNumConstraints();
  snapshot.relative_constraints.reserve(snapshot.relative_constraint_count);
  for (int constraint_id = 0;
       constraint_id < snapshot.relative_constraint_count; ++constraint_id) {
    RelativeTimingConstraintSnapshot constraint;
    constraint.constraint_id = constraint_id;
    phydb::PhydbPin root;
    phydb::PhydbPin fast_terminal;
    phydb::PhydbPin slow_terminal;
    if (timing_api.GetConstraintEndpoints(constraint_id, root, fast_terminal,
                                          slow_terminal)) {
      const std::string root_name = ResolveTimingPinName(phy_db_, root);
      constraint.fast_path.root_pin = root_name;
      constraint.slow_path.root_pin = root_name;
      constraint.fast_path.terminal_pin =
          ResolveTimingPinName(phy_db_, fast_terminal);
      constraint.slow_path.terminal_pin =
          ResolveTimingPinName(phy_db_, slow_terminal);
    }
    snapshot.relative_constraints.push_back(std::move(constraint));
  }
#endif
  return snapshot;
}

} // namespace dali
