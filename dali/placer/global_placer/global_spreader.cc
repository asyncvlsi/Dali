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

/**
 * @file
 * Interface for the spreading step of global placement, which produces the
 * upper bound the wirelength solver is pulled toward.
 */
#include "dali/placer/global_placer/global_spreader.h"

#include "dali/common/helper.h"

namespace dali {

GlobalSpreader::GlobalSpreader(Circuit* circuit) : circuit_(circuit) {
  DaliExpects(circuit_ != nullptr, "Global spreader requires a circuit");
}

}  // namespace dali
