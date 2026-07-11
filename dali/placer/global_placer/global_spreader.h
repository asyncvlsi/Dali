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
#ifndef DALI_PLACER_GLOBAL_PLACER_GLOBAL_SPREADER_H_
#define DALI_PLACER_GLOBAL_PLACER_GLOBAL_SPREADER_H_

#include <vector>

#include "dali/circuit/circuit.h"

namespace dali {

/** Replaceable component that produces overlap-reduced global placements. */
class GlobalSpreader {
 public:
  explicit GlobalSpreader(Circuit* circuit);
  virtual ~GlobalSpreader() = default;

  /** Initialize the spreader for the requested placement density. */
  virtual void Initialize(double placement_density) = 0;

  /** Spread components and return the resulting upper-bound HPWL. */
  virtual double Spread() = 0;

  /** Return total spreading runtime in seconds. */
  virtual double GetTime() const = 0;

  /** Release implementation-specific resources. */
  virtual void Close() = 0;

  /** Return upper-bound HPWL history. */
  std::vector<double>& Hpwls() { return upper_bound_hpwl_; }
  const std::vector<double>& Hpwls() const { return upper_bound_hpwl_; }

  /** Return x upper-bound HPWL history. */
  const std::vector<double>& HpwlsX() const { return upper_bound_hpwl_x_; }

  /** Return y upper-bound HPWL history. */
  const std::vector<double>& HpwlsY() const { return upper_bound_hpwl_y_; }

  /** Enable or disable intermediate placement dumps. */
  void SetShouldSaveIntermediateResult(bool should_save);

  /** Update the current global-placement iteration. */
  void SetIteration(int iteration) { iteration_ = iteration; }

 protected:
  Circuit* circuit_ = nullptr;
  double placement_density_ = 1.0;
  std::vector<double> upper_bound_hpwl_;
  std::vector<double> upper_bound_hpwl_x_;
  std::vector<double> upper_bound_hpwl_y_;
  bool should_save_intermediate_result_ = false;
  int iteration_ = 0;
};

}  // namespace dali

#endif  // DALI_PLACER_GLOBAL_PLACER_GLOBAL_SPREADER_H_
