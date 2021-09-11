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
    IoPinCluster(bool is_horizontal_init,
                 double boundary_loc_init,
                 double lo_init,
                 double span_init);
    ~IoPinCluster();

    bool is_horizontal; // is this cluster on a horizontal or vertical boundary
    double boundary_loc; // the location of the boundary this cluster is on
    double low; // the low position of this cluster
    double span; // the width or height of this cluster
    bool is_uniform_mode = true; // uniformly distribute IOPINs in this cluster or not
    std::vector<IOPin *> iopin_ptr_list; // IOPINs  in this cluster

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
    IoBoundaryLayerSpace(bool is_horizontal_init,
                         double boundary_loc_init,
                         MetalLayer *metal_layer_init);
    ~IoBoundaryLayerSpace();
    bool is_horizontal;
    double boundary_loc;
    MetalLayer *metal_layer;
    RectD default_horizontal_shape; // unit in micron
    RectD default_vertical_shape; // unit in micron
    bool is_using_horizontal = true;
    std::vector<IOPin *> iopin_ptr_list;
    std::vector<IoPinCluster> pin_clusters;

    void AddCluster(double low, double span);

    void ComputeDefaultShape();
    void UpdateIoPinShapeAndLayer();
    void UniformAssignIoPinToCluster();
    void GreedyAssignIoPinToCluster();
    void AssignIoPinToCluster();
};

/**
 * A structure for storing IOPINs on a boundary for all possible metal layer.
 */
struct IoBoundarySpace {
  private:
    int iopin_limit_ = 0;
    bool is_iopin_limit_set_ = false;
    bool is_horizontal_;
    double boundary_loc_ = 0;
  public:
    std::vector<IoBoundaryLayerSpace> layer_spaces_;

    IoBoundarySpace(bool is_horizontal, double boundary_loc);
    void AddLayer(MetalLayer *metal_layer);
    void SetIoPinLimit(int limit);
    bool AutoPlaceIoPin();
};

}

#endif //DALI_DALI_PLACER_IOPLACER_IOBOUNDARYSPACE_H_