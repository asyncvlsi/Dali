/*******************************************************************************
 *
 * Copyright (c) 2024 Yihang Yang
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
#ifndef DALI_DALI_COMMON_NAMED_INSTANCE_COLLECTION_H_
#define DALI_DALI_COMMON_NAMED_INSTANCE_COLLECTION_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "dali/common/logging.h"

namespace dali {

template <typename T>
class NamedInstanceCollection {
 public:
  // Adds an instance of T to the collection
  T &CreateInstance(std::string const &name) {
    DaliExpects(!frozen_, "Cannot create new instance: collection is frozen.");

    // Check if the name already exists
    auto it = name_to_id_map_.find(name);
    DaliExpects(it == name_to_id_map_.end(),
                "An instance with this name already exists: " << name);
    name_to_id_map_[name] = instances_.size();

    // instance contains a pointer to its name
    it = name_to_id_map_.find(name);
    instances_.emplace_back(T(&(it->first)));
    return instances_.back();
  }

  // Retrieves an instance of T by name
  T *GetInstanceByName(std::string const &name) {
    auto it = name_to_id_map_.find(name);
    DaliExpects(it != name_to_id_map_.end(),
                "Cannot find instance by name: " << name);
    return &instances_[it->second];
  }

  // Retrieves an instance id by name
  size_t GetInstanceIdByName(std::string const &name) {
    auto it = name_to_id_map_.find(name);
    DaliExpects(it != name_to_id_map_.end(),
                "Cannot find instance by name: " << name);
    return it->second;
  }

  // Retrieves an instance of T by index (id)
  T *GetInstanceById(size_t id) {
    DaliExpects(id < instances_.size(),
                "Cannot find instance by id, out of range: " << id);
    return &instances_[id];
  }

  // Check if a name exists in the collection
  [[nodiscard]] bool NameExists(std::string const &name) const {
    return name_to_id_map_.find(name) != name_to_id_map_.end();
  }

  // Get the size of the collection
  [[nodiscard]] size_t GetSize() const { return instances_.size(); }

  std::unordered_map<std::string, size_t> &NameToIdMap() {
    return name_to_id_map_;
  }

  std::vector<T> &Instances() { return instances_; }

  // Freeze the collection to prevent creating new instances
  void Freeze() { frozen_ = true; }

  // Unfreeze the collection to allow creating new instances
  void Unfreeze() { frozen_ = false; }

  // Check if the collection is frozen
  [[nodiscard]] bool IsFrozen() const { return frozen_; }

  // Clear all instances
  void Clear() {
    name_to_id_map_.clear();
    instances_.clear();
    frozen_ = false;
  }

  // Reserve space
  void Reserve(size_t size) { instances_.reserve(size); }

 private:
  std::unordered_map<std::string, size_t> name_to_id_map_;
  std::vector<T> instances_;
  bool frozen_ = false;
};

}  // namespace dali

#endif  // DALI_DALI_COMMON_NAMED_INSTANCE_COLLECTION_H_
