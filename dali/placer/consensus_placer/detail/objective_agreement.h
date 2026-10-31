#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace dali::consensus_internal {
// Circuit scales the accumulated grid-space HPWL once, whereas the model
// scales each nonnegative net term before accumulation. For identical pin
// spans, gamma_(n+2) bounds either summation; the extra products and final
// scaling are covered by 8 epsilon. This checks representation agreement only:
// candidate acceptance must still compare native costs against the native
// source, without adding this bound to the allowed objective regression.
inline double hpwl_agreement_bound(double native, double model,
                                   std::size_t net_count) {
  const double eps = std::numeric_limits<double>::epsilon();
  const double neps = (double(net_count) + 2) * eps;
  if (!std::isfinite(native) || !std::isfinite(model) || native < 0 ||
      model < 0 || neps >= .5)
    return 0;
  return (2 * neps / (1 - neps) + 8 * eps) * std::max({1., native, model});
}
inline bool hpwl_agrees(double native, double model, std::size_t net_count) {
  return std::isfinite(native) && std::isfinite(model) && native >= 0 &&
         model >= 0 &&
         std::abs(native - model) <=
             hpwl_agreement_bound(native, model, net_count);
}
} // namespace dali::consensus_internal
