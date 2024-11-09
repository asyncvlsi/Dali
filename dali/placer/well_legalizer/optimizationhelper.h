/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#ifndef DALI_PLACER_WELL_LEGALIZER_OPTIMIZATIONHELPER_H_
#define DALI_PLACER_WELL_LEGALIZER_OPTIMIZATIONHELPER_H_

#include <cfloat>
#include <vector>

#include "dali/placer/well_legalizer/blockhelper.h"

namespace dali {

void MinimizeQuadraticDisplacement(std::vector<BlkDispVar> &vars,
                                   double lower_limit = -DBL_MAX,
                                   double upper_limit = DBL_MAX);

void MinimizeLinearDisplacement(std::vector<BlkDispVar> &vars,
                                double lower_limit = -DBL_MAX,
                                double upper_limit = DBL_MAX);

void AbacusPlaceRow(std::vector<BlkDispVar> &vars,
                    double lower_limit = -DBL_MAX,
                    double upper_limit = DBL_MAX);

}  // namespace dali

#endif  // DALI_PLACER_WELL_LEGALIZER_OPTIMIZATIONHELPER_H_
