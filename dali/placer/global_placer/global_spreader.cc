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
#include "dali/placer/global_placer/global_spreader.h"

#include "dali/common/helper.h"

namespace dali {

GlobalSpreader::GlobalSpreader(Circuit* circuit) : circuit_(circuit) {
  DaliExpects(circuit_ != nullptr, "Global spreader requires a circuit");
}

void GlobalSpreader::SetShouldSaveIntermediateResult(bool should_save) {
  should_save_intermediate_result_ = should_save;
}

}  // namespace dali
