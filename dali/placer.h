//
// Created by Yihang Yang on 9/5/19.
//

#ifndef DALI_SRC_PLACER_H_
#define DALI_SRC_PLACER_H_

/****placer base class****/
#include "dali/placer/placer.h"

/****Global placer****/
#include "dali/placer/globalPlacer/GPSimPL.h"

/****Legalizer****/
#include "dali/placer/legalizer/LGTetris.h"
#include "dali/placer/legalizer/LGTetrisEx.h"

/****Well Legalizer****/
#include "dali/placer/wellLegalizer/clusterwelllegalizer.h"
#include "dali/placer/wellLegalizer/stdclusterwelllegalizer.h"
#include "dali/placer/wellLegalizer/welllegalizer.h"

/****Well Placement Flow****/
#include "dali/placer/wellPlaceFlow/wellplaceflow.h"

#endif //DALI_SRC_PLACER_H_
