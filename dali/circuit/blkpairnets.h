//
// Created by Yihang Yang on 10/27/20.
//

#ifndef DALI_DALI_CIRCUIT_BLKPAIRNETS_H_
#define DALI_DALI_CIRCUIT_BLKPAIRNETS_H_

#include <vector>

#include "Eigen/IterativeLinearSolvers"
#include "Eigen/Sparse"

#include "net.h"

namespace dali {
// declares a row-major sparse matrix type of double
typedef Eigen::SparseMatrix<double, Eigen::RowMajor> SpMat;

class BlkBlkEdge {
  public:
    BlkBlkEdge(
        Net *net_ptr,
        int d_index,
        int l_index
    ) : net(net_ptr),
        d(d_index),
        l(l_index) {}
    Net *net; // the pointer to the net containing two blocks
    int d; // driver index
    int l; // load index
};

class BlkPairNets {
  public:
    BlkPairNets(int blk0, int blk1) : blk_num0(blk0), blk_num1(blk1) {}
    std::vector<BlkBlkEdge> edges;
    int blk_num0;
    int blk_num1;

    double e00x = 0;
    double e01x = 0;
    double e10x = 0;
    double e11x = 0;
    double b0x = 0;
    double b1x = 0;
    SpMat::InnerIterator it01x;
    SpMat::InnerIterator it10x;

    double e00y = 0;
    double e01y = 0;
    double e10y = 0;
    double e11y = 0;
    double b0y = 0;
    double b1y = 0;
    SpMat::InnerIterator it01y;
    SpMat::InnerIterator it10y;

    void ClearX();
    void WriteX();
    void ClearY();
    void WriteY();
};

}

#endif //DALI_DALI_CIRCUIT_BLKPAIRNETS_H_
