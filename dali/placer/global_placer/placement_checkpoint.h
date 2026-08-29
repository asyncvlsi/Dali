/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 *******************************************************************************/
#ifndef DALI_PLACER_GLOBAL_PLACER_PLACEMENT_CHECKPOINT_H_
#define DALI_PLACER_GLOBAL_PLACER_PLACEMENT_CHECKPOINT_H_

#include <cstddef>
#include <string>
#include <vector>

#include "dali/circuit/topology_delta.h"

namespace dali {

/**
 * A point in global placement at which the circuit's topology may be changed.
 *
 * A checkpoint is offered only after an upper-bound refinement that was
 * physical and accepted rather than rolled back. That restriction is the whole
 * point of the mechanism: cell counts and coordinates may only be disturbed at
 * a state the placer has already judged realizable, so that whatever is
 * measured at the checkpoint describes a placement the flow could actually
 * produce, and so that resuming from it starts from legal-ish coordinates
 * rather than from a spread cloud.
 */
struct PlacementCheckpoint {
  int iteration = 0;
  double accepted_hpwl = 0.0;
  /** Change from the previous physical upper bound; 0 for the first one. */
  double hpwl_change_fraction = 0.0;
  double lower_bound_hpwl = 0.0;
  /** How many physical upper bounds have been accepted so far, including this. */
  int physical_upper_bound_count = 0;
};

/**
 * What the placer should do after a checkpoint has been observed.
 *
 * kChangeTopology is a request to leave the iteration loop so the netlist can
 * change. The placement engines are sized by the topology, so they are torn
 * down before the change and rebuilt around the new one; component coordinates
 * survive because they live in the circuit, not in the engines.
 */
enum class CheckpointDecision { kContinue, kChangeTopology };

/**
 * Everything a host is told about the placement, by value.
 *
 * Deliberately not a `Circuit*` or a `GlobalPlacer*`. A host that could reach
 * into the circuit could also leave it half-changed, and then Dali would be
 * finalizing a topology nobody validated. Handing over values instead means the
 * only way to change anything is to describe the whole change and return it,
 * which is what makes the apply transactional.
 */
struct TopologyCheckpointContext {
  int checkpoint_iteration = 0;
  /** The absolute iteration placement will resume at. */
  int resume_iteration = 0;
  size_t component_count = 0;
  size_t net_count = 0;
  /** Spare capacity, so a host can size a request it knows will fit. */
  size_t component_headroom = 0;
  size_t net_headroom = 0;
};

/** Whether a host changed anything, and if not, whether that was a failure. */
enum class TopologyMutationStatus {
  /** Nothing to change. Placement rebuilds and resumes. */
  kNoChange,
  /** A complete delta is supplied. Placement applies it, rebuilds, resumes. */
  kApplied,
  /** The host could not produce a usable delta. Placement fails. */
  kFailed,
};

struct TopologyMutationResult {
  TopologyMutationStatus status = TopologyMutationStatus::kNoChange;
  TopologyDelta delta;
  /** Why, for the log. Required when the status is kFailed. */
  std::string message;

  static TopologyMutationResult NoChange() { return {}; }
  static TopologyMutationResult Applied(TopologyDelta delta) {
    TopologyMutationResult result;
    result.status = TopologyMutationStatus::kApplied;
    result.delta = std::move(delta);
    return result;
  }
  static TopologyMutationResult Failed(std::string message) {
    TopologyMutationResult result;
    result.status = TopologyMutationStatus::kFailed;
    result.message = std::move(message);
    return result;
  }
};

/**
 * What Dali asks the host to do, by value.
 *
 * Every field is already decided. The host formats the count into a configured
 * process template and applies it; it does not choose a site, alter a count, or
 * trim a request. The expected growth travels with the request so Dali can hold
 * the returned delta to what it asked for rather than to whatever arrives.
 */
struct TopologyChangeRequest {
  std::string site;
  int current_pairs = 0;
  int requested_pairs = 0;
  int expected_added_components = 0;
  int expected_added_nets = 0;
};

/**
 * Every site Dali decided to grow, in one request.
 *
 * A batch rather than a sequence of single requests because every request in
 * one decision was measured on one topology and must cross ACT together.
 * Adaptive sizing may issue another batch only after Dali has legalized and
 * measured the changed topology. Sorted by site name so the same decision
 * produces the same request every time.
 *
 * The host applies all of it or none of it. There is no partial batch, because
 * a partially applied batch leaves a netlist that neither Dali nor ACT
 * described.
 */
struct TopologyChangeBatch {
  std::vector<TopologyChangeRequest> requests;

  bool IsEmpty() const { return requests.empty(); }

  int ExpectedAddedComponents() const {
    int total = 0;
    for (const TopologyChangeRequest &request : requests) {
      total += request.expected_added_components;
    }
    return total;
  }
  int ExpectedAddedNets() const {
    int total = 0;
    for (const TopologyChangeRequest &request : requests) {
      total += request.expected_added_nets;
    }
    return total;
  }
};

class GlobalPlacer;

class PlacementCheckpointObserver {
 public:
  virtual ~PlacementCheckpointObserver() = default;

  /**
   * Decide whether to stop for a topology change. Engines are still alive, so
   * an implementation may read the placement but must not change the netlist.
   */
  virtual CheckpointDecision Observe(const PlacementCheckpoint &checkpoint) = 0;

  /**
   * Produce the change. Called only after a kChangeTopology decision and only
   * once every topology-sized engine is closed, so a host is free to rebuild
   * whatever authoritative state it owns before describing the result.
   */
  virtual TopologyMutationResult RequestTopologyChange(
      const PlacementCheckpoint &checkpoint,
      const TopologyCheckpointContext &context) {
    (void)checkpoint;
    (void)context;
    return TopologyMutationResult::NoChange();
  }
};

/**
 * An observer that records checkpoints and never asks for a topology change.
 *
 * This exists so the checkpoint boundary can be exercised, and its no-op path
 * shown to reproduce the placement exactly, before anything is allowed to
 * mutate a netlist through it.
 */
class RecordingCheckpointObserver : public PlacementCheckpointObserver {
 public:
  CheckpointDecision Observe(const PlacementCheckpoint &checkpoint) override;
  const std::vector<PlacementCheckpoint> &Checkpoints() const {
    return checkpoints_;
  }
  size_t Count() const { return checkpoints_.size(); }

 private:
  std::vector<PlacementCheckpoint> checkpoints_;
};

/**
 * Installs an observer for a scope and removes it on every exit path.
 *
 * The placer holds the observer as a bare pointer, so an early return between
 * installing and clearing it leaves a dangling one behind. That is not
 * hypothetical: the observer is usually a local of the caller's frame.
 */
class ScopedCheckpointObserver {
 public:
  ScopedCheckpointObserver(GlobalPlacer &placer,
                           PlacementCheckpointObserver *observer);
  ~ScopedCheckpointObserver();
  ScopedCheckpointObserver(const ScopedCheckpointObserver &) = delete;
  ScopedCheckpointObserver &operator=(const ScopedCheckpointObserver &) = delete;

 private:
  GlobalPlacer &placer_;
};

} // namespace dali

#endif // DALI_PLACER_GLOBAL_PLACER_PLACEMENT_CHECKPOINT_H_
