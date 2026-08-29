/*******************************************************************************
 *
 * Constraint attribution for delay-line timing feedback.
 *
 *******************************************************************************/

#ifndef DALI_TIMING_DELAY_LINE_FEEDBACK_H_
#define DALI_TIMING_DELAY_LINE_FEEDBACK_H_

#include <string>
#include <vector>

#include "dali/timing/timing_snapshot.h"

namespace dali {

/** Timing evidence attributed to one registered delay line. */
struct DelayLineConstraintAttribution {
  std::vector<int> constraint_ids;
  double binding_slack = 0.0;
  bool has_binding_slack = false;
  /**
   * Which constraint the binding slack came from.
   *
   * The slack alone cannot say whether two samples describe the same timing
   * path or two different ones that happen to be equally tight, and a sizing
   * rule that switched paths between samples would look perfectly stable in
   * slack while meaning something different each time.
   */
  int binding_constraint_id = -1;
};

/**
 * Existing timing-feedback data carried into an optional placement snapshot.
 *
 * `proposed_separation` is what the controller computed from slack and measured
 * gain; `new_separation` is what the request actually carries away. They differ
 * whenever separation escalation is disabled and the proposal was an increase,
 * and a reader that cannot tell them apart concludes the geometry moved when it
 * did not -- which is exactly how the width-1 controller fixture came to log
 * `new_separation 10` on every iteration while holding separation at 0.
 */
struct DelayLineFeedbackEvent {
  int iteration = -1;
  std::string line_name;
  std::vector<int> constraint_ids;
  double binding_slack_ps = 0.0;
  int old_separation = -1;
  int proposed_separation = -1;
  int new_separation = -1;
  double measured_gain_ps_per_row = 0.0;
  bool escalation_enabled = false;
};

/**
 * The separation a request carries away, given what the controller proposed.
 *
 * Escalation off suppresses increases only. A decrease still applies, because
 * the gate exists to stop separation from consuming die height chasing a
 * deficit, not to freeze geometry: a line asked to give height back should give
 * it back whichever mechanism is in charge.
 *
 * Pure and separate from the controller so that the difference between a
 * proposed and an applied separation can be tested without a timing host, and
 * so that the log schema and the request cannot disagree about which is which.
 */
int ApplySeparationEscalationGate(int current_separation, int proposed_separation,
                                  bool escalation_enabled);

/** Result of matching all relative constraints to registered delay lines. */
struct DelayLineAttributionResult {
  std::vector<DelayLineConstraintAttribution> per_line;
  std::string error;

  bool valid() const { return error.empty(); }
};

/** Stable owner evidence retained across an ACT-authoritative topology change. */
struct ConstraintAttributionEvidence {
  std::string semantic_identity;
  std::string owner;
};

/**
 * Attribute constraints from slow-only candidate-net evidence.
 *
 * A prefix matches an exact net or a net whose next character is a hierarchy
 * or name separator. Every constraint must have at most one owner; each line
 * must have at least one constraint. A line's binding slack is the minimum of
 * all slacks attributed to it.
 */
DelayLineAttributionResult AttributeDelayLineConstraints(
    const std::vector<std::string> &line_prefixes,
    const std::vector<RelativeTimingConstraintSnapshot> &constraints);

/** Freeze the current witness-derived owner of every semantic constraint. */
bool CaptureConstraintAttributionEvidence(
    const std::vector<std::string> &line_prefixes,
    const std::vector<RelativeTimingConstraintSnapshot> &constraints,
    std::vector<ConstraintAttributionEvidence> *evidence, std::string *error);

/**
 * Restore frozen owners onto an endpoint-and-slack-only timing capture.
 *
 * The semantic identity set must match exactly, so a topology change cannot
 * silently inherit attribution for a different constraint graph.
 */
bool RestoreConstraintAttributionEvidence(
    const std::vector<ConstraintAttributionEvidence> &evidence,
    std::vector<RelativeTimingConstraintSnapshot> *constraints,
    std::string *error);

namespace delay_line_feedback_internal {

/**
 * Whether `candidate` is `prefix`, or continues it past a name separator.
 *
 * Exposed because selecting cells by name prefix is not unique to delay-line
 * attribution -- stage bands select the same way -- and two prefix rules that
 * disagree about whether `lat1` covers `lat10` would be worse than one shared
 * rule in a slightly odd place.
 */
bool HasTokenBoundaryPrefix(const std::string &prefix,
                            const std::string &candidate);

} // namespace delay_line_feedback_internal

} // namespace dali

#endif // DALI_TIMING_DELAY_LINE_FEEDBACK_H_
