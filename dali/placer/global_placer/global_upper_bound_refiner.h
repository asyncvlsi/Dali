/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#ifndef DALI_PLACER_GLOBAL_PLACER_GLOBAL_UPPER_BOUND_REFINER_H_
#define DALI_PLACER_GLOBAL_PLACER_GLOBAL_UPPER_BOUND_REFINER_H_

namespace dali {

/** Result of replacing an LAL upper bound with a more physical placement. */
struct GlobalUpperBoundRefinement {
  bool feasible = false;
  double hpwl = 0.0;
  double overflow = 0.0;
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
