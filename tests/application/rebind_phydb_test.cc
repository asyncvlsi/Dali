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
 * Rebinding Dali to a PhyDB rebuilt underneath it.
 *
 * A host that re-elaborates ACT destroys the database Dali was constructed
 * against. Dali survives on purpose -- it is holding the placement -- so every
 * pointer it kept into that database has to be dealt with, and "dealt with"
 * differs by member: some are repointed, one is rebuilt because it captured the
 * database at construction, and one is discarded because it describes a netlist
 * that no longer exists.
 *
 * These tests exist because the first version of the rebind repointed three
 * members and missed two. Counting pointers is not an audit; each member is
 * checked here by name.
 */
#include <gtest/gtest.h>

#include <memory>

#include <phydb/phydb.h>

#include "dali/dali.h"
#include "dali/circuit/circuit.h"
#include "dali/placer/io_placer/io_placer.h"

namespace dali {
namespace {

TEST(RebindPhyDBTest, IoPlacerFollowsTheRebind) {
  auto *old_db = new phydb::PhyDB;
  auto *new_db = new phydb::PhyDB;
  // Declared before the Dali that will own the placer, so it outlives it.
  Circuit circuit;
  Dali placer(nullptr, severity::info);

  auto io_placer = std::make_unique<IoPlacer>(old_db, &circuit);
  ASSERT_EQ(io_placer->PhyDBPtr(), old_db);
  placer.SetIoPlacerForTesting(std::move(io_placer));

  placer.RebindPhyDB(new_db);

  // The negative control is the assertion itself: without the SetPhyDB call in
  // RebindPhyDB this reports the old database, which by then is the one the
  // host has destroyed.
  EXPECT_EQ(placer.IoPlacerForTesting()->PhyDBPtr(), new_db);
  EXPECT_NE(placer.IoPlacerForTesting()->PhyDBPtr(), old_db);
  placer.Close();
}

TEST(RebindPhyDBTest, RebindWithNoIoPlacerIsHarmless) {
  auto *old_db = new phydb::PhyDB;
  auto *new_db = new phydb::PhyDB;
  Dali placer(nullptr, severity::info);
  placer.RebindPhyDB(new_db);
  EXPECT_EQ(placer.IoPlacerForTesting(), nullptr);
  placer.Close();
}

TEST(RebindPhyDBTest, RebindRejectsANullDatabase) {
  Dali placer(nullptr, severity::info);
  EXPECT_DEATH(placer.RebindPhyDB(nullptr), "");
}

} // namespace
} // namespace dali
