/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#ifndef DALI_PLACER_GLOBAL_PLACER_GLOBAL_UPPER_BOUND_REFINER_H_
#define DALI_PLACER_GLOBAL_PLACER_GLOBAL_UPPER_BOUND_REFINER_H_

#include <vector>

namespace dali {

/**
 * A placement region that a physical upper-bound refiner could not legalize.
 *
 * Bounds and overflow use the circuit's placement-grid units. Component ids
 * refer to the circuit-owned component collection.
 */
struct GlobalUpperBoundViolation {
  double lx = 0.0;
  double ly = 0.0;
  double ux = 0.0;
  double uy = 0.0;
  double overflow = 0.0;
  std::vector<int> component_ids;
};

/** Result of replacing an LAL upper bound with a more physical placement. */
struct GlobalUpperBoundRefinement {
  bool feasible = false;
  double hpwl = 0.0;
  double overflow = 0.0;
  int modified_component_count = 0;
  std::vector<GlobalUpperBoundViolation> violations;
};

/** Optional periodic physical refinement of global-placement upper bounds. */
class GlobalUpperBoundRefiner {
 public:
  virtual ~GlobalUpperBoundRefiner() = default;

  /** Prepare reusable state for the requested placement density. */
  virtual void Initialize(double placement_density) = 0;

  /**
   * Refine the current upper bound.
   *
   * A feasible result commits provisional component coordinates. An
   * infeasible result must leave the incoming placement unchanged.
   */
  virtual GlobalUpperBoundRefinement Refine(int iteration) = 0;

  /** Return accumulated refinement runtime in seconds. */
  virtual double GetTime() const = 0;

  /** Release implementation-specific resources. */
  virtual void Close() = 0;
};

}  // namespace dali

#endif
