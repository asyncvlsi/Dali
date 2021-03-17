//
// Created by Yihang Yang on 11/12/2020.
//

#ifndef DALI_SRC_COMMON_SI2LEFDEF_H_
#define DALI_SRC_COMMON_SI2LEFDEF_H_

#include "dali/circuit/circuit.h"

namespace dali {

void ReadLef(std::string const &lef_file_name, Circuit *circuit);
void ReadDef(std::string const &def_file_name, Circuit *circuit);

}

#endif //DALI_SRC_COMMON_SI2LEFDEF_H_
