/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ******************************************************************************/
#ifndef DALI_COMMON_HASH_H_
#define DALI_COMMON_HASH_H_

#include <cstddef>
#include <functional>

namespace dali {

template <typename T>
inline void HashCombine(std::size_t& seed, const T& value) {
  seed ^= std::hash<T>{}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

}  // namespace dali

#endif  // DALI_COMMON_HASH_H_
