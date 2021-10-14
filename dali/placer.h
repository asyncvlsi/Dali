//
// Created by Yihang Yang on 9/5/19.
//

#ifndef DALI_SRC_PLACER_H_
#define DALI_SRC_PLACER_H_

/****placer base class****/
#include "dali/placer/placer.h"

/****Global placer****/
#include "dali/placer/global_placer/GPSimPL.h"

/****Legalizer****/
#include "dali/placer/legalizer/LGTetris.h"
#include "dali/placer/legalizer/LGTetrisEx.h"

/****Well Legalizer****/
#include "dali/placer/well_legalizer/clusterwelllegalizer.h"
#include "dali/placer/well_legalizer/stdclusterwelllegalizer.h"
#include "dali/placer/well_legalizer/welllegalizer.h"

/****Well Placement Flow****/
#include "dali/placer/well_place_flow/wellplaceflow.h"
#include "dali/placer/welltap_placer/welltapplacer.h"

/****IO Placer****/
#include "dali/placer/io_placer/ioplacer.h"

#endif //DALI_SRC_PLACER_H_
