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
/**
 * Where an out-of-process authority supplies a netlist change.
 *
 * Dali owns global placement: when to stop, whether a checkpoint is worth
 * taking, how much delay to ask for, the circuit, the controller, and the GUI.
 * What Dali cannot own is the netlist itself, which belongs to ACT. This is the
 * one seam between them.
 *
 * A host is asked, at a point Dali chose, to bring its authoritative state up
 * to date and describe the result. It is asked only after every topology-sized
 * placement engine has been closed, so it is free to rebuild PhyDB and the
 * timer underneath. It answers with values -- names, masters, connectivity --
 * and never receives a handle it could use to change Dali's circuit itself.
 *
 * The interface is deliberately this small. Anything larger would let the host
 * start making placement decisions, and the loop would drift out of Dali.
 */
#ifndef DALI_TIMING_TOPOLOGY_CHECKPOINT_HOST_H_
#define DALI_TIMING_TOPOLOGY_CHECKPOINT_HOST_H_

#include "dali/placer/global_placer/placement_checkpoint.h"

namespace dali {

class TopologyCheckpointHost {
 public:
  virtual ~TopologyCheckpointHost() = default;

  /**
   * Apply an already-decided change and describe what ACT produced.
   *
   * An apply operation, not a decision one. Dali has already chosen the site
   * and the count; the host translates that into the authoritative netlist and
   * reports the result. Naming this `Apply` rather than `Request` is the point:
   * the previous shape invited the host to decide, and a host that could decide
   * would be the second place policy lived.
   *
   * Called with every topology-sized placement engine closed, so the host is
   * free to rebuild PhyDB and the timer underneath. Return kApplied with a
   * complete delta, or kFailed with a reason, which fails global placement
   * rather than letting a partial result be finalized. kNoChange is available
   * for a host that has nothing to do; a Dali decision not to change anything
   * never reaches here.
   *
   * The batch is applied whole or not at all. Every request in it was decided
   * together, from one measurement of one placement, and a host that applied
   * some of them would leave a netlist that neither Dali nor ACT described. The
   * host may not add, drop, reorder, resize or substitute a request: it
   * translates each decided pair count through the configured process template
   * and reports what ACT produced.
   */
  virtual TopologyMutationResult ApplyTopologyChange(
      const TopologyCheckpointContext &context,
      const TopologyChangeBatch &batch) = 0;
};

} // namespace dali

#endif // DALI_TIMING_TOPOLOGY_CHECKPOINT_HOST_H_
