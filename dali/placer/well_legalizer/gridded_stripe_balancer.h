/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 *******************************************************************************/
#ifndef DALI_PLACER_WELL_LEGALIZER_GRIDDED_STRIPE_BALANCER_H_
#define DALI_PLACER_WELL_LEGALIZER_GRIDDED_STRIPE_BALANCER_H_

#include <vector>
#include <unordered_map>

#include "dali/circuit/circuit.h"
#include "dali/placer/well_legalizer/gridded_capacity_estimator.h"
#include "dali/placer/well_legalizer/stripe.h"

namespace dali {

/** Summary of non-geometric component reassignment between gridded stripes. */
struct GriddedStripeBalanceResult {
  int moved_component_count = 0;
  int overflowing_stripes_before = 0;
  int overflowing_stripes_after = 0;
  unsigned long long overflow_area_before = 0;
  unsigned long long overflow_area_after = 0;
};

/** Reassigns components between neighboring stripes without moving them. */
class GriddedStripeBalancer {
 public:
  GriddedStripeBalancer(Circuit* circuit, GriddedCapacityConfig config);

  /** Balance physical capacity and return assignment-level diagnostics. */
  GriddedStripeBalanceResult Balance(
      std::vector<StripeColumn>* stripe_columns) const;

  /** Balance assignments using row heights measured by trial clustering. */
  GriddedStripeBalanceResult BalanceObservedOverflow(
      std::vector<StripeColumn>* stripe_columns) const;

 private:
  struct CandidateMove {
    Component* component = nullptr;
    Stripe* source = nullptr;
    Stripe* target = nullptr;
    double hpwl_delta = 0.0;
    double displacement = 0.0;
    unsigned long long demand_area = 0;
  };

  GriddedCapacityEstimate Estimate(const Stripe& stripe) const;
  Stripe* FindNearestTarget(std::vector<StripeColumn>& columns,
                            int source_column, Component* component,
                            const std::unordered_map<
                                Stripe*, unsigned long long>& available_spare)
      const;
  double EstimateAffectedNetHpwlDelta(Component* component,
                                      const Stripe& target) const;
  unsigned long long EstimateComponentDemand(const Component& component) const;
  int ApplyMoves(
      std::vector<StripeColumn>* stripe_columns,
      std::unordered_map<Stripe*, unsigned long long>* overflow_budget,
      std::unordered_map<Stripe*, unsigned long long>* available_spare) const;

  Circuit* circuit_ = nullptr;
  GriddedCapacityConfig config_;
};

}  // namespace dali

#endif
