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
#ifndef DALI_DALI_COMMON_NAMED_INSTANCE_REGISTRY_H_
#define DALI_DALI_COMMON_NAMED_INSTANCE_REGISTRY_H_

#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "dali/common/logging.h"

namespace dali {

/**
 * Registry for objects that are addressed by both name and stable numeric id.
 *
 * Instances are stored densely in insertion order, so ids are vector indices
 * and remain stable until Clear() is called. Names are stored once in the
 * lookup table; each instance receives a pointer to that canonical name string
 * during construction. T must therefore be constructible from `const
 * std::string*`.
 *
 * Creation can be frozen after setup to catch accidental late mutations. Direct
 * mutable access to the map/vector remains available for legacy callers, but
 * new code should prefer the registry methods so the name/id invariant stays in
 * one place.
 */
template <typename T>
class NamedInstanceRegistry {
  static_assert(std::is_constructible<T, const std::string*>::value,
                "NamedInstanceRegistry<T> requires T(const std::string*)");

 public:
  /** Object reference and id assigned by a successful CreateWithId() call. */
  struct CreationResult {
    T& object;
    size_t id;
  };

  /**
   * Create and return a new named instance and its stable id.
   *
   * Prefer structured binding at call sites:
   *   auto [component, component_id] = registry.CreateWithId(name);
   *
   * The returned reference follows std::vector invalidation rules and must not
   * be kept across later registry mutations.
   */
  [[nodiscard]] CreationResult CreateWithId(std::string const& name) {
    DaliExpects(!frozen_, "Cannot create new instance: registry is frozen.");

    size_t id = instances_.size();
    auto [it, inserted] = name_to_id_map_.emplace(name, id);
    DaliExpects(inserted,
                "An instance with this name already exists: " << name);

    // Instances keep a pointer to the canonical name stored in the map.
    instances_.emplace_back(&(it->first));

    return {instances_.back(), id};
  }

  /**
   * Create and return a new named instance.
   *
   * The returned reference follows std::vector invalidation rules and must not
   * be kept across later registry mutations.
   */
  T& Create(std::string const& name) { return CreateWithId(name).object; }

  /** Return the instance with the given name. Exits if the name is unknown. */
  T* GetInstanceByName(std::string const& name) {
    auto it = name_to_id_map_.find(name);
    DaliExpects(it != name_to_id_map_.end(),
                "Cannot find instance by name: " << name);
    return &instances_[it->second];
  }

  /** Return the instance with the given name. Exits if the name is unknown. */
  const T* GetInstanceByName(std::string const& name) const {
    auto it = name_to_id_map_.find(name);
    DaliExpects(it != name_to_id_map_.end(),
                "Cannot find instance by name: " << name);
    return &instances_[it->second];
  }

  /** Return the instance id for the given name. Exits if the name is unknown.
   */
  [[nodiscard]] size_t GetInstanceIdByName(std::string const& name) const {
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

  /** Return the instance at id. Exits if id is out of range. */
  const T* GetInstanceById(size_t id) const {
    DaliExpects(id < instances_.size(),
                "Cannot find instance by id, out of range: " << id);
    return &instances_[id];
  }

  /** Return true when name exists in the registry. */
  [[nodiscard]] bool NameExists(std::string const& name) const {
    return name_to_id_map_.find(name) != name_to_id_map_.end();
  }

  /** Return the number of stored instances. */
  [[nodiscard]] size_t GetSize() const { return instances_.size(); }

  /** Return the name-to-id map. */
  const std::unordered_map<std::string, size_t>& NameToIdMap() const {
    return name_to_id_map_;
  }

  /** Return the mutable name-to-id map. Prefer lookup helpers when possible. */
  std::unordered_map<std::string, size_t>& NameToIdMap() {
    return name_to_id_map_;
  }

  /** Return the instance storage. */
  const std::vector<T>& Instances() const { return instances_; }

  /** Return the mutable instance storage. Prefer lookup helpers when possible.
   */
  std::vector<T>& Instances() { return instances_; }

  /** Prevent future Create() calls until Unfreeze() is called. */
  void Freeze() { frozen_ = true; }

  /** Allow future Create() calls. */
  void Unfreeze() { frozen_ = false; }

  /** Return true when new instance creation is disabled. */
  [[nodiscard]] bool IsFrozen() const { return frozen_; }

  /** Remove all instances, lookup entries, and the frozen state. */
  void Clear() {
    name_to_id_map_.clear();
    instances_.clear();
    frozen_ = false;
  }

  /** Reserve storage for instances and lookup entries. */
  void Reserve(size_t size) {
    instances_.reserve(size);
    name_to_id_map_.reserve(size);
  }

 private:
  std::unordered_map<std::string, size_t> name_to_id_map_;
  std::vector<T> instances_;
  bool frozen_ = false;
};

}  // namespace dali

#endif  // DALI_DALI_COMMON_NAMED_INSTANCE_REGISTRY_H_
