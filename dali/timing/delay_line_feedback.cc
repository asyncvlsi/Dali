/*******************************************************************************
 *
 * Constraint attribution for delay-line timing feedback.
 *
 *******************************************************************************/

#include "dali/timing/delay_line_feedback.h"

#include <algorithm>
#include <cstddef>
#include <unordered_map>
#include <unordered_set>

namespace dali {

namespace delay_line_feedback_internal {

bool HasTokenBoundaryPrefix(const std::string &prefix,
                            const std::string &candidate) {
  if (candidate.compare(0, prefix.size(), prefix) != 0) return false;
  if (candidate.size() == prefix.size()) return true;
  const char separator = candidate[prefix.size()];
  return separator == '_' || separator == '.' || separator == '/' ||
         separator == ':' || separator == '[' || separator == '$';
}

} // namespace delay_line_feedback_internal

int ApplySeparationEscalationGate(int current_separation,
                                  int proposed_separation,
                                  bool escalation_enabled) {
  if (!escalation_enabled && proposed_separation > current_separation) {
    return current_separation;
  }
  return proposed_separation;
}

DelayLineAttributionResult AttributeDelayLineConstraints(
    const std::vector<std::string> &line_prefixes,
    const std::vector<RelativeTimingConstraintSnapshot> &constraints) {
  DelayLineAttributionResult result;
  result.per_line.resize(line_prefixes.size());

  for (const RelativeTimingConstraintSnapshot &constraint : constraints) {
    std::vector<std::size_t> owners;
    for (std::size_t line_index = 0; line_index < line_prefixes.size();
         ++line_index) {
      const bool matches = std::any_of(
          constraint.delay_repair_candidate_nets.begin(),
          constraint.delay_repair_candidate_nets.end(),
          [&line_prefixes, line_index](const std::string &candidate) {
            return delay_line_feedback_internal::HasTokenBoundaryPrefix(
                line_prefixes[line_index], candidate);
          });
      if (matches) owners.push_back(line_index);
    }

    if (owners.size() > 1) {
      result.error = "ambiguous delay-line attribution for constraint " +
                     std::to_string(constraint.constraint_id);
      return result;
    }
    if (owners.empty()) continue;

    DelayLineConstraintAttribution &attribution = result.per_line[owners[0]];
    attribution.constraint_ids.push_back(constraint.constraint_id);
    if (!attribution.has_binding_slack ||
        constraint.slack < attribution.binding_slack) {
      attribution.binding_slack = constraint.slack;
      attribution.binding_constraint_id = constraint.constraint_id;
      attribution.has_binding_slack = true;
    }
  }

  for (std::size_t line_index = 0; line_index < result.per_line.size();
       ++line_index) {
    if (!result.per_line[line_index].has_binding_slack) {
      result.error = "missing delay-line attribution for '" +
                     line_prefixes[line_index] + "'";
      return result;
    }
  }
  return result;
}

bool CaptureConstraintAttributionEvidence(
    const std::vector<std::string> &line_prefixes,
    const std::vector<RelativeTimingConstraintSnapshot> &constraints,
    std::vector<ConstraintAttributionEvidence> *evidence, std::string *error) {
  if (evidence == nullptr || error == nullptr) return false;
  evidence->clear();
  error->clear();
  std::unordered_set<std::string> identities;
  evidence->reserve(constraints.size());
  for (const RelativeTimingConstraintSnapshot &constraint : constraints) {
    ConstraintAttributionEvidence item;
    item.semantic_identity = constraint.SemanticIdentity();
    if (item.semantic_identity.empty() ||
        !identities.insert(item.semantic_identity).second) {
      *error = "missing or duplicate semantic constraint identity";
      evidence->clear();
      return false;
    }
    for (const std::string &prefix : line_prefixes) {
      const bool matches = std::any_of(
          constraint.delay_repair_candidate_nets.begin(),
          constraint.delay_repair_candidate_nets.end(),
          [&prefix](const std::string &candidate) {
            return delay_line_feedback_internal::HasTokenBoundaryPrefix(
                prefix, candidate);
          });
      if (!matches) continue;
      if (!item.owner.empty()) {
        *error = "ambiguous delay-line attribution for semantic constraint '" +
                 item.semantic_identity + "'";
        evidence->clear();
        return false;
      }
      item.owner = prefix;
    }
    evidence->push_back(std::move(item));
  }
  return true;
}

bool RestoreConstraintAttributionEvidence(
    const std::vector<ConstraintAttributionEvidence> &evidence,
    std::vector<RelativeTimingConstraintSnapshot> *constraints,
    std::string *error) {
  if (constraints == nullptr || error == nullptr) return false;
  error->clear();
  std::unordered_map<std::string, std::string> owners;
  owners.reserve(evidence.size());
  for (const ConstraintAttributionEvidence &item : evidence) {
    if (item.semantic_identity.empty() ||
        !owners.emplace(item.semantic_identity, item.owner).second) {
      *error = "missing or duplicate cached semantic constraint identity";
      return false;
    }
  }
  std::unordered_set<std::string> seen;
  seen.reserve(constraints->size());
  for (RelativeTimingConstraintSnapshot &constraint : *constraints) {
    const std::string identity = constraint.SemanticIdentity();
    const auto owner = owners.find(identity);
    if (identity.empty() || owner == owners.end() ||
        !seen.insert(identity).second) {
      *error = "current semantic constraint set does not match cached attribution";
      return false;
    }
    constraint.delay_repair_candidate_nets.clear();
    if (!owner->second.empty()) {
      constraint.delay_repair_candidate_nets.push_back(owner->second);
    }
  }
  if (seen.size() != owners.size()) {
    *error = "current semantic constraint set does not match cached attribution";
    return false;
  }
  return true;
}

} // namespace dali
