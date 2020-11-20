//
// Created by Yihang Yang on 9/5/19.
//

#ifndef DALI_SRC_PLACER_H_
#define DALI_SRC_PLACER_H_

/****placer base class****/
#include "placer/placer.h"

/****Global placer****/
#include "placer/globalPlacer/GPSimPL.h"

/****Detailed placer****/
#include "placer/detailedPlacer/DPAbacus.h"
#include "placer/detailedPlacer/DPAdam.h"
#include "placer/detailedPlacer/DPSwap.h"
#include "placer/detailedPlacer/MDPlacer.h"

/****Legalizer****/
#include "placer/legalizer/LGTetris.h"
#include "placer/legalizer/LGTetrisEx.h"

/****Post Legalization Optimization****/
#include "placer/postLegalOptimizer/PLOSlide.h"

/****Well Legalizer****/
#include "placer/wellLegalizer/clusterwelllegalizer.h"
#include "placer/wellLegalizer/stdclusterwelllegalizer.h"
#include "placer/wellLegalizer/welllegalizer.h"

/****Well Placement Flow****/
#include "placer/wellPlaceFlow/wellplaceflow.h"

#endif //DALI_SRC_PLACER_H_
