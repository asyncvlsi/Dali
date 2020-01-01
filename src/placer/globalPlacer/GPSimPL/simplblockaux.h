//
// Created by Yihang Yang on 2019-08-12.
//

#ifndef HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_SIMPLBLOCKAUX_H_
#define HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_SIMPLBLOCKAUX_H_

#include <set>

#include "circuit/net.h"
#include "circuit/blockaux.h"

class SimPLBlockAux : public BlockAux {
 public:
  explicit SimPLBlockAux(Block *block);
  std::set<Net *> net_set;
  bool NetExist(Net *net);
  void InsertNet(Net *net);
  int B2BRowSizeX();
  int B2BRowSizeY();
  void ReportAux();
};

#endif //HPCC_SRC_PLACER_GLOBALPLACER_GPSIMPL_SIMPLBLOCKAUX_H_
