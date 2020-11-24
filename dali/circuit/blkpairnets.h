//
// Created by Yihang Yang on 10/27/20.
//

#ifndef DALI_SRC_CIRCUIT_BLKPAIRNETS_H_
#define DALI_SRC_CIRCUIT_BLKPAIRNETS_H_

#include <vector>

#include "dali/solver.h"

#include "net.h"

namespace dali {
// declares a row-major sparse matrix type of double
typedef Eigen::SparseMatrix<double, Eigen::RowMajor> SpMat;

struct BlkBlkEdge {
  BlkBlkEdge(Net *net_ptr, int d_index, int l_index) : net(net_ptr), d(d_index), l(l_index) {}
  Net *net; // the pointer to the net containing two blocks
  int d; // driver index
  int l; // load index
};

struct BlkPairNets {
  BlkPairNets(int blk0, int blk1) : blk_num0(blk0), blk_num1(blk1) {}
  std::vector<BlkBlkEdge> edges;
  int blk_num0;
  int blk_num1;

  double e00x;
  double e01x;
  double e10x;
  double e11x;
  double b0x;
  double b1x;
  SpMat::InnerIterator it01x;
  SpMat::InnerIterator it10x;

  double e00y;
  double e01y;
  double e10y;
  double e11y;
  double b0y;
  double b1y;
  SpMat::InnerIterator it01y;
  SpMat::InnerIterator it10y;

  void ClearX() {
    e00x = 0;
    e01x = 0;
    e10x = 0;
    e11x = 0;
    b0x = 0;
    b1x = 0;
  }
  void WriteX() {
    if (blk_num0 != blk_num1) {
      it01x.valueRef() = e01x;
      it10x.valueRef() = e10x;
    }
  }

  void ClearY() {
    e00y = 0;
    e01y = 0;
    e10y = 0;
    e11y = 0;
    b0y = 0;
    b1y = 0;
  }
  void WriteY() {
    if (blk_num0 != blk_num1) {
      it01y.valueRef() = e01y;
      it10y.valueRef() = e10y;
    }
  }
};

}

#endif //DALI_SRC_CIRCUIT_BLKPAIRNETS_H_
