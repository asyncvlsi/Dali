//
// Created by Yihang Yang on 8/29/19.
//

#ifndef DALI_SRC_COMMON_GLOBAL_H_
#define DALI_SRC_COMMON_GLOBAL_H_

enum VerboseLevel {
  LOG_NOTHING = 0,
  LOG_CRITICAL = 1,
  LOG_ERROR = 2,
  LOG_WARNING = 3,
  LOG_INFO = 4,
  LOG_DEBUG = 5
};

extern VerboseLevel globalVerboseLevel;

#endif //DALI_SRC_COMMON_GLOBAL_H_
