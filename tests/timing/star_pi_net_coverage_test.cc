/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 ******************************************************************************/
/*
 * Which nets RC extraction covers.
 *
 * Two properties are load-bearing and were each broken at some point.
 *
 * The first is agreement: extraction walks every net twice, once to create
 * parasitic edges and once to set R and C on them, and setting R on an edge
 * that was never created aborts. Both walks now ask ShouldExtractNetRC, so
 * these tests pin the decision rather than either copy of it.
 *
 * The second is scope. A blanket `if (!net.GetIoPinIdsRef().empty()) continue;`
 * previously excluded every net carrying an I/O pin -- 193 of 1948 nets on
 * bd_pipeline, all of which map to ACT. The justification given was that an I/O
 * pin has no component-pin ACT pointer and so aborts the SPEF lookup, but
 * neither walk ever asks for an I/O pin's SPEF node: both iterate component
 * pins only. The condition that actually matters is whether a *component* pin
 * drives the net.
 */
#include "dali/timing/star_pi_model_estimator.h"

#include <gtest/gtest.h>

namespace dali {
namespace {

// The case the blanket I/O skip got wrong. Such a net maps to ACT and has a
// component driver; carrying an I/O pin as well is irrelevant, because the I/O
// pin is not one of the endpoints either walk considers. This fails with the
// blanket skip, which returned false for it.
TEST(StarPiNetCoverageTest, NetWithAComponentDriverIsExtracted) {
  EXPECT_TRUE(ShouldExtractNetRC(true, false, 0, 8, 0));
  EXPECT_TRUE(ShouldExtractNetRC(true, false, 7, 8, 1));
}

// The case the current endpoint handling adds. A design input port drives the
// net it attaches to; PhyDB records that as a driver index into the net's I/O
// pin list. Before this existed such a net had no driver at all and every
// segment on it was lost -- 129 nets on bd_pipeline, 64 of them with eight
// loads each.
TEST(StarPiNetCoverageTest, NetDrivenByAnInputPortIsExtracted) {
  EXPECT_TRUE(ShouldExtractNetRC(true, true, 0, 8, 1));
}

// An output port is a load, not a driver, so its net is extracted on the
// strength of its component driver and the port becomes one more endpoint.
TEST(StarPiNetCoverageTest, NetDrivingAnOutputPortIsExtracted) {
  EXPECT_TRUE(ShouldExtractNetRC(true, false, 0, 1, 1));
}

// The driver index is read against whichever list the flag selects. Reading an
// I/O driver index against the component-pin list is how a port-driven net was
// previously mistaken for a driverless one.
TEST(StarPiNetCoverageTest, DriverIndexIsCheckedAgainstTheSelectedList) {
  // Index 0 is valid among one I/O pin, even with no component pins at all.
  EXPECT_TRUE(ShouldExtractNetRC(true, true, 0, 0, 1));
  // ...and invalid when the I/O list is empty, however many component pins.
  EXPECT_FALSE(ShouldExtractNetRC(true, true, 0, 8, 0));
}

// A net nothing drives -- neither a component pin nor a port -- has no wire to
// extract. PhyDB reports that as -1 whichever list the flag selects.
TEST(StarPiNetCoverageTest, NetWithNoDriverAtAllIsSkipped) {
  EXPECT_FALSE(ShouldExtractNetRC(true, false, -1, 8, 0));
  EXPECT_FALSE(ShouldExtractNetRC(true, true, -1, 8, 1));
}

// An unmapped net has no parasitics graph to attach to at all.
TEST(StarPiNetCoverageTest, NetWithoutAnActPointerIsSkipped) {
  EXPECT_FALSE(ShouldExtractNetRC(false, false, 0, 8, 0));
  EXPECT_FALSE(ShouldExtractNetRC(false, true, 0, 0, 1));
}

// A driver index past the end of the component-pin list would read out of
// bounds; it is refused rather than clamped, because a wrong driver silently
// produces plausible, wrong RC.
TEST(StarPiNetCoverageTest, OutOfRangeDriverIndexIsSkipped) {
  EXPECT_FALSE(ShouldExtractNetRC(true, false, 8, 8, 0));
  EXPECT_FALSE(ShouldExtractNetRC(true, false, 9, 8, 0));
  EXPECT_FALSE(ShouldExtractNetRC(true, false, 0, 0, 0));
  EXPECT_FALSE(ShouldExtractNetRC(true, true, 1, 8, 1));
}

// A single-pin net has a driver and no loads. It is extracted, and the walk
// over its loads simply finds none -- this must not be confused with the
// no-driver case, which is excluded for a different reason.
TEST(StarPiNetCoverageTest, DriverOnlyNetIsExtractedAndYieldsNoSegments) {
  EXPECT_TRUE(ShouldExtractNetRC(true, false, 0, 1, 0));
}

} // namespace
} // namespace dali
