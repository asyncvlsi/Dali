//
// Created by Yihang Yang on 4/1/20.
//

#ifndef DALI_SRC_COMMON_SI2LEFDEF_H_
#define DALI_SRC_COMMON_SI2LEFDEF_H_

#include "circuit/circuit.h"

namespace dali {

void readLef(std::string &lefFileName, Circuit &circuit);
void readDef(std::string &defFileName, Circuit &circuit);

}

#endif //DALI_SRC_COMMON_SI2LEFDEF_H_
