//
// Created by Yihang Yang on 4/1/2020.
//

#ifndef DALI_SRC_COMMON_SI2LEFDEF_H_
#define DALI_SRC_COMMON_SI2LEFDEF_H_

#include "dali/circuit/circuit.h"

namespace dali {

void ReadLEF(std::string const &lef_file_name, Circuit *circuit);
void ReadDEF(std::string const &def_file_name, Circuit *circuit);

}

#endif //DALI_SRC_COMMON_SI2LEFDEF_H_
