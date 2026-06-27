/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#include "component_cluster.h"

namespace dali {

ComponentCluster::ComponentCluster() = default;

ComponentCluster::ComponentCluster(int well_extension_x_init,
                                   int well_extension_y_init,
                                   int plug_width_init)
    : well_extension_x_(well_extension_x_init),
      well_extension_y_(well_extension_y_init),
      plug_width_(plug_width_init) {}

void ComponentCluster::AppendComponent(Component& component) {
  if (component_ptr_list_.empty()) {
    lx_ = int(component.LLX()) - well_extension_x_;
    modified_lx_ = lx_ - well_extension_x_ - plug_width_;
    ly_ = int(component.LLY()) - well_extension_y_;
    width_ = component.Width() + well_extension_x_ * 2 + plug_width_;
    height_ = component.Height() + well_extension_y_ * 2;
  } else {
    width_ += component.Width();
    if (component.Height() > height_) {
      ly_ -= (component.Height() - height_ + 1) / 2;
      height_ = component.Height() + well_extension_y_ * 2;
    }
  }
  component_ptr_list_.push_back(&component);
}

void ComponentCluster::OptimizeHeight() {
  /****
   * This function aligns all N/P well boundaries of cells inside a cluster
   * ****/
}

void ComponentCluster::UpdateComponentLocation() {
  int current_loc = lx_;
  for (auto& component_ptr : component_ptr_list_) {
    component_ptr->SetLLX(current_loc);
    component_ptr->SetCenterY(this->CenterY());
    current_loc += component_ptr->Width();
  }
}

}  // namespace dali
