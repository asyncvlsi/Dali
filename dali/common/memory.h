#ifndef DALI_COMMON_MEMORY_H_
#define DALI_COMMON_MEMORY_H_

#include <cstddef>

namespace dali {

/** Return peak resident memory usage in bytes, or 0 when unavailable. */
size_t getPeakRSS();

/** Return current resident memory usage in bytes, or 0 when unavailable. */
size_t getCurrentRSS();

}  // namespace dali

#endif  // DALI_COMMON_MEMORY_H_
