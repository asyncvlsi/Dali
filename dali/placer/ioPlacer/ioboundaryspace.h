//
// Created by Yihang Yang on 8/27/21.
//

#ifndef DALI_DALI_PLACER_IOPLACER_IOBOUNDARYSPACE_H_
#define DALI_DALI_PLACER_IOPLACER_IOBOUNDARYSPACE_H_

#include <string>
#include <vector>

#include "dali/circuit/iopin.h"

namespace dali {

/**
 * A structure for storing IOPINs on a boundary segment for a given metal layer.
 */
struct IoPinCluster {
    IoPinCluster(bool is_horizontal_boundary_init,
                 double boundary_loc_init,
                 double lo_init,
                 double span_init) :
        is_horizontal_boundary(is_horizontal_boundary_init),
        boundary_loc(boundary_loc_init),
        lo(lo_init),
        span(span_init) {}
    ~IoPinCluster() {
        for (auto &ptr: iopin_ptr_list) {
            ptr = nullptr;
        }
    }
    bool is_horizontal_boundary;
    double boundary_loc;
    double lo;
    double span;
    bool is_uniform_mode = true;
    std::vector<IOPin *> iopin_ptr_list;

    double Low() const;
    double High() const;
    void UniformLegalize();
    void GreedyLegalize();
    void Legalize();
};

/**
 * A structure for storing IOPINs on a boundary for a given metal layer.
 * If there is no blockage on the boundary, then it only contains one IoPinCluster.
 */
struct IoBoundaryLayerSpace {
    IoBoundaryLayerSpace(bool is_horizontal_boundary_init,
                         double boundary_loc_init,
                         MetalLayer *metal_layer_init) :
        is_horizontal_boundary(is_horizontal_boundary_init),
        boundary_loc(boundary_loc_init),
        metal_layer(metal_layer_init) {}
    ~IoBoundaryLayerSpace() {
        metal_layer = nullptr;
        for (auto &ptr: iopin_ptr_list) {
            ptr = nullptr;
        }
        pin_clusters.clear();
    }
    bool is_horizontal_boundary;
    double boundary_loc;
    MetalLayer *metal_layer;
    RectD default_horizontal_shape;
    RectD default_vertical_shape;
    bool is_using_horizontal = true;
    std::vector<IOPin *> iopin_ptr_list;
    std::vector<IoPinCluster> pin_clusters;

    void ComputeDefaultShape();
    void UpdateIoPinShape();
    void UniformAssignIoPinToCluster();
    void GreedyAssignIoPinToCluster();
    void AssignIoPinToCluster();
};

/**
 * A structure for storing IOPINs on a boundary for all possible metal layer.
 */
struct IoBoundarySpace {
    ~IoBoundarySpace() {
        layer_spaces.clear();
    }
    int iopin_limit = 0;
    bool is_iopin_limit_set = false;
    std::vector<IoBoundaryLayerSpace> layer_spaces;

    void SetIoPinLimit(int limit);
    bool AutoPlaceIoPin();
};

}

#endif //DALI_DALI_PLACER_IOPLACER_IOBOUNDARYSPACE_H_
