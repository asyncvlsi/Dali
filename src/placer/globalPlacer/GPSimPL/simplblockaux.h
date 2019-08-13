//
// Created by Yihang Yang on 2019-08-12.
//

#ifndef HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_SIMPLBLOCKAUX_H_
#define HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_SIMPLBLOCKAUX_H_

#include "circuit/net.h"
#include "circuit/blockaux.h"

class SimPLBlockAux: public BlockAux {
 public:
  SimPLBlockAux(Block *block);
  std::vector< Net * > net_list;
  void AddNet(Net *net);
};

#endif //HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_SIMPLBLOCKAUX_H_
