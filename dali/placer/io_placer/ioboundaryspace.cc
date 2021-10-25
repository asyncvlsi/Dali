//
// Created by Yihang Yang on 8/27/21.
//

#include "ioboundaryspace.h"

namespace dali {

IoPinCluster::IoPinCluster(
    bool is_horizontal_init,
    double boundary_loc_init,
    double lo_init,
    double span_init
) :
    is_horizontal(is_horizontal_init),
    boundary_loc(boundary_loc_init),
    low(lo_init),
    span(span_init) {}

IoPinCluster::~IoPinCluster() {
    for (auto &ptr: iopin_ptr_list) {
        ptr = nullptr;
    }
}

double IoPinCluster::Low() const {
    return low;
}

double IoPinCluster::High() const {
    return low + span;
}

void IoPinCluster::UniformLegalize() {
    if (is_horizontal) {
        std::sort(
            iopin_ptr_list.begin(),
            iopin_ptr_list.end(),
            [](const IoPin *lhs, const IoPin *rhs) {
                return (lhs->X() < rhs->X());
            }
        );

        double hi = low + span;
        int sz = (int) iopin_ptr_list.size();
        double step = (hi - low) / (sz + 1);
        for (int i = 0; i < sz; ++i) {
            double loc = std::round(low + (i + 1) * step);
            iopin_ptr_list[i]->SetLoc(loc, boundary_loc, PLACED);
        }
    } else {
        std::sort(
            iopin_ptr_list.begin(),
            iopin_ptr_list.end(),
            [](const IoPin *lhs, const IoPin *rhs) {
                return (lhs->Y() < rhs->Y());
            }
        );

        double hi = low + span;
        int sz = (int) iopin_ptr_list.size();
        double step = (hi - low) / (sz + 1);
        for (int i = 0; i < sz; ++i) {
            double loc = std::round(low + (i + 1) * step);
            iopin_ptr_list[i]->SetLoc(boundary_loc, loc, PLACED);
        }
    }
}

void IoPinCluster::GreedyLegalize() {
    DaliExpects(false, "to be implemented!");
}

void IoPinCluster::Legalize() {
    if (is_uniform_mode) {
        UniformLegalize();
    } else {
        GreedyLegalize();
    }
}

IoBoundaryLayerSpace::IoBoundaryLayerSpace(
    bool is_horizontal_init,
    double boundary_loc_init,
    MetalLayer *metal_layer_init
) :
    is_horizontal(is_horizontal_init),
    boundary_loc(boundary_loc_init),
    metal_layer(metal_layer_init) {}

IoBoundaryLayerSpace::~IoBoundaryLayerSpace() {
    metal_layer = nullptr;
    for (auto &ptr: iopin_ptr_list) {
        ptr = nullptr;
    }
    pin_clusters.clear();
}

void IoBoundaryLayerSpace::AddCluster(double low, double span) {
    pin_clusters.emplace_back(is_horizontal, boundary_loc, low, span);
}

void IoBoundaryLayerSpace::ComputeDefaultShape() {
    double min_width = metal_layer->Width();
    double min_area = metal_layer->MinArea();
    double height = min_area / min_width;

    default_horizontal_shape.SetValue(-height / 2.0,
                                      0,
                                      height / 2.0,
                                      min_width);
    default_vertical_shape.SetValue(-min_width / 2.0,
                                    0,
                                    min_width / 2.0,
                                    height);
}

void IoBoundaryLayerSpace::UpdateIoPinShapeAndLayer() {
    double llx, lly, urx, ury;
    if (is_using_horizontal) {
        llx = default_horizontal_shape.LLX();
        lly = default_horizontal_shape.LLY();
        urx = default_horizontal_shape.URX();
        ury = default_horizontal_shape.URY();
    } else {
        llx = default_vertical_shape.LLX();
        lly = default_vertical_shape.LLY();
        urx = default_vertical_shape.URX();
        ury = default_vertical_shape.URY();
    }

    for (auto &pin_cluster: pin_clusters) {
        for (auto &pin_ptr: pin_cluster.iopin_ptr_list) {
            if (!pin_ptr->IsShapeSet()) {
                pin_ptr->SetShape(llx, lly, urx, ury);
                pin_ptr->SetLayerPtr(metal_layer);
            }
        }
    }
}

void IoBoundaryLayerSpace::UniformAssignIoPinToCluster() {

}

void IoBoundaryLayerSpace::GreedyAssignIoPinToCluster() {
    if (is_horizontal) {
        std::sort(iopin_ptr_list.begin(),
                  iopin_ptr_list.end(),
                  [](const IoPin *lhs, const IoPin *rhs) {
                      return (lhs->X() < rhs->X());
                  });
        for (auto &iopin_ptr: iopin_ptr_list) {
            int len = (int) pin_clusters.size();
            double min_distance = DBL_MAX;
            int min_index = 0;
            for (int i = 0; i < len; ++i) {
                double distance = std::min(
                    std::fabs(iopin_ptr->X() - pin_clusters[i].Low()),
                    std::fabs(iopin_ptr->X() - pin_clusters[i].High())
                );
                if (distance < min_distance) {
                    min_distance = distance;
                    min_index = i;
                }
            }
            pin_clusters[min_index].iopin_ptr_list.push_back(iopin_ptr);
        }
    } else {
        std::sort(iopin_ptr_list.begin(),
                  iopin_ptr_list.end(),
                  [](const IoPin *lhs, const IoPin *rhs) {
                      return (lhs->Y() < rhs->Y());
                  });
        for (auto &iopin_ptr: iopin_ptr_list) {
            int len = (int) pin_clusters.size();
            double min_distance = DBL_MAX;
            int min_index = 0;
            for (int i = 0; i < len; ++i) {
                double distance = std::min(
                    std::fabs(iopin_ptr->Y() - pin_clusters[i].Low()),
                    std::fabs(iopin_ptr->Y() - pin_clusters[i].High())
                );
                if (distance < min_distance) {
                    min_distance = distance;
                    min_index = i;
                }
            }
            pin_clusters[min_index].iopin_ptr_list.push_back(iopin_ptr);
        }
    }
}

void IoBoundaryLayerSpace::AssignIoPinToCluster() {
    GreedyAssignIoPinToCluster();
}

IoBoundarySpace::IoBoundarySpace(bool is_horizontal, double boundary_loc) :
    is_horizontal_(is_horizontal),
    boundary_loc_(boundary_loc) {}

void IoBoundarySpace::AddLayer(MetalLayer *metal_layer) {
    layer_spaces_.emplace_back(is_horizontal_, boundary_loc_, metal_layer);
}

void IoBoundarySpace::SetIoPinLimit(int limit) {
    iopin_limit_ = limit;
    is_iopin_limit_set_ = true;
}

bool IoBoundarySpace::AutoPlaceIoPin() {
    for (auto &layer_space: layer_spaces_) {
        layer_space.ComputeDefaultShape();
        layer_space.AssignIoPinToCluster();
        layer_space.UpdateIoPinShapeAndLayer();
        for (auto &pin_cluster: layer_space.pin_clusters) {
            pin_cluster.Legalize();
        }
    }
    return true;
}

}