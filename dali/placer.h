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
#ifndef DALI_PLACER_H_
#define DALI_PLACER_H_

/****placer base class****/
#include "dali/placer/placer.h"

/****Global placer****/
#include "dali/placer/global_placer/global_placer.h"

/****Legalizer****/
#include "dali/placer/legalizer/LGTetris.h"
#include "dali/placer/legalizer/LGTetrisEx.h"

/****Well Legalizer****/
#include "dali/placer/well_legalizer/griddedrowlegalizer.h"
#include "dali/placer/well_legalizer/stdclusterwelllegalizer.h"
#include "dali/placer/well_legalizer/welllegalizer.h"

/****Well Placement Flow****/
#include "dali/placer/well_place_flow/wellplaceflow.h"
#include "dali/placer/welltap_placer/welltapplacer.h"

/****Filler Cell Placer****/
#include "dali/placer/filler_cell_placer/filler_cell_placer.h"

/****IO Placer****/
#include "dali/placer/io_placer/ioplacer.h"

#endif  // DALI_PLACER_H_
