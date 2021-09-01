//
// Created by Yihang Yang on 8/27/21.
//

#include "ioboundaryspace.h"

namespace dali {

double IoPinCluster::Low() const {
    return lo;
}

double IoPinCluster::High() const {
    return lo + span;
}

void IoPinCluster::UniformLegalize() {
    if (is_horizontal_boundary) {
        std::sort(iopin_ptr_list.begin(),
                  iopin_ptr_list.end(),
                  [](const IOPin *lhs, const IOPin *rhs) {
                      return (lhs->X() < rhs->X());
                  });

        double hi = lo + span;
        int sz = (int) iopin_ptr_list.size();
        double step = (hi - lo) / (sz + 1);
        for (int i = 0; i < sz; ++i) {
            iopin_ptr_list[i]->SetLoc((i + 1) * step, boundary_loc, PLACED);
        }
    } else {
        std::sort(iopin_ptr_list.begin(),
                  iopin_ptr_list.end(),
                  [](const IOPin *lhs, const IOPin *rhs) {
                      return (lhs->Y() < rhs->Y());
                  });

        double hi = lo + span;
        int sz = (int) iopin_ptr_list.size();
        double step = (hi - lo) / (sz + 1);
        for (int i = 0; i < sz; ++i) {
            iopin_ptr_list[i]->SetLoc(boundary_loc, (i + 1) * step, PLACED);
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

void IoBoundaryLayerSpace::ComputeDefaultShape() {
    std::cout << boundary_loc << "\n";
    double min_width = metal_layer->Width();
    double min_area = metal_layer->Area();
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

void IoBoundaryLayerSpace::UpdateIoPinShape() {
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
            }
        }
    }
}

void IoBoundaryLayerSpace::UniformAssignIoPinToCluster() {

}

void IoBoundaryLayerSpace::GreedyAssignIoPinToCluster() {
    if (is_horizontal_boundary) {
        std::sort(iopin_ptr_list.begin(),
                  iopin_ptr_list.end(),
                  [](const IOPin *lhs, const IOPin *rhs) {
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
                  [](const IOPin *lhs, const IOPin *rhs) {
                      return (lhs->Y() < rhs->Y());
                  });
        std::cout << iopin_ptr_list.size() << " " << pin_clusters.size() << "\n";
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
            std::cout << min_index << "\n";
            pin_clusters[min_index].iopin_ptr_list.push_back(iopin_ptr);
        }
    }
}

void IoBoundaryLayerSpace::AssignIoPinToCluster() {
    GreedyAssignIoPinToCluster();
}

void IoBoundarySpace::SetIoPinLimit(int limit) {
    iopin_limit = limit;
    is_iopin_limit_set = true;
}

bool IoBoundarySpace::AutoPlaceIoPin() {
    for (auto &layer_space: layer_spaces) {
        layer_space.ComputeDefaultShape();
        layer_space.UpdateIoPinShape();
        layer_space.AssignIoPinToCluster();
        for (auto &pin_cluster: layer_space.pin_clusters) {
            pin_cluster.Legalize();
        }
    }
}

}