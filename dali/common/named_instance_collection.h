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

/**
 * Owns a vector of named instances and provides stable name-to-id lookup.
 *
 * T must be constructible from a pointer to the stored name string. Creation
 * can be frozen after setup to catch accidental late mutations.
 */
template <typename T>
class NamedInstanceCollection {
 public:
  /** Create and return a new named instance. */
  T& CreateInstance(std::string const& name) {
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

  /** Return the instance with the given name. Exits if the name is unknown. */
  T* GetInstanceByName(std::string const& name) {
    auto it = name_to_id_map_.find(name);
    DaliExpects(it != name_to_id_map_.end(),
                "Cannot find instance by name: " << name);
    return &instances_[it->second];
  }

  /** Return the instance id for the given name. Exits if the name is unknown.
   */
  size_t GetInstanceIdByName(std::string const& name) {
    auto it = name_to_id_map_.find(name);
    DaliExpects(it != name_to_id_map_.end(),
                "Cannot find instance by name: " << name);
    return it->second;
  }

  /** Return the instance at id. Exits if id is out of range. */
  T* GetInstanceById(size_t id) {
    DaliExpects(id < instances_.size(),
                "Cannot find instance by id, out of range: " << id);
    return &instances_[id];
  }

  /** Return true when name exists in the collection. */
  [[nodiscard]] bool NameExists(std::string const& name) const {
    return name_to_id_map_.find(name) != name_to_id_map_.end();
  }

  /** Return the number of stored instances. */
  [[nodiscard]] size_t GetSize() const { return instances_.size(); }

  /** Return the mutable name-to-id map. */
  std::unordered_map<std::string, size_t>& NameToIdMap() {
    return name_to_id_map_;
  }

  /** Return the mutable instance storage. */
  std::vector<T>& Instances() { return instances_; }

  /** Prevent future CreateInstance calls until Unfreeze() is called. */
  void Freeze() { frozen_ = true; }

  /** Allow future CreateInstance calls. */
  void Unfreeze() { frozen_ = false; }

  /** Return true when new instance creation is disabled. */
  [[nodiscard]] bool IsFrozen() const { return frozen_; }

  /** Remove all instances, lookup entries, and the frozen state. */
  void Clear() {
    name_to_id_map_.clear();
    instances_.clear();
    frozen_ = false;
  }

  /** Reserve storage for instances. */
  void Reserve(size_t size) { instances_.reserve(size); }

 private:
  std::unordered_map<std::string, size_t> name_to_id_map_;
  std::vector<T> instances_;
  bool frozen_ = false;
};

}  // namespace dali

#endif  // DALI_DALI_COMMON_NAMED_INSTANCE_COLLECTION_H_
