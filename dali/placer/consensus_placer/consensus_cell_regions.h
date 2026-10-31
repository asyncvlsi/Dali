/*******************************************************************************
 * Copyright (c) 2026 Yihang Yang
 * SPDX-License-Identifier: GPL-2.0-or-later
 ******************************************************************************/
#ifndef DALI_PLACER_CONSENSUS_PLACER_CONSENSUS_CELL_REGIONS_H_
#define DALI_PLACER_CONSENSUS_PLACER_CONSENSUS_CELL_REGIONS_H_

#include <cstddef>
#include <string>

#include "dali/circuit/circuit.h"

namespace dali {
/**
 * Restore complete paired well regions after ordinary LEF/DEF/CELL import.
 *
 * The existing import can retain only one pair per master. This extension
 * preserves Macro/Pin identity. It validates the complete input before
 * updating any master, returns false for malformed/off-grid data, and reports
 * the number of multi-region masters through restored. Call during circuit
 * initialization, before constructing row assignments or stretching.
 */
bool LoadConsensusCellRegions(Circuit &circuit, const std::string &path,
                              std::size_t *restored = nullptr,
                              std::string *error = nullptr);
} // namespace dali
#endif
