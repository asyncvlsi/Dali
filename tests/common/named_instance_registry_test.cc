/*******************************************************************************
 *
 * Copyright (c) 2026 Yihang Yang
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
#include "dali/common/named_instance_registry.h"

#include <gtest/gtest.h>

#include <string>

class TestInstance {
 public:
  explicit TestInstance(const std::string* name) : name_(name) {}

  const std::string& Name() const { return *name_; }

 private:
  const std::string* name_;
};

TEST(NamedInstanceRegistryTest, CreatesInstancesWithStableIds) {
  dali::NamedInstanceRegistry<TestInstance> collection;

  auto [first, first_id] = collection.CreateWithId("first");
  EXPECT_EQ(first.Name(), "first");

  auto [second, second_id] = collection.CreateWithId("second");
  EXPECT_EQ(second.Name(), "second");

  EXPECT_EQ(first_id, 0);
  EXPECT_EQ(second_id, 1);
  EXPECT_EQ(collection.GetSize(), 2);
  EXPECT_TRUE(collection.NameExists("first"));
  EXPECT_EQ(collection.GetInstanceIdByName("second"), second_id);
  EXPECT_EQ(collection.GetInstanceById(first_id)->Name(), "first");
  EXPECT_EQ(collection.GetInstanceByName("second")->Name(), "second");
}

TEST(NamedInstanceRegistryTest, SupportsConstLookup) {
  dali::NamedInstanceRegistry<TestInstance> collection;
  collection.Create("only");

  const auto& const_collection = collection;

  EXPECT_EQ(const_collection.GetInstanceByName("only")->Name(), "only");
  EXPECT_EQ(const_collection.GetInstanceById(0)->Name(), "only");
  EXPECT_EQ(const_collection.GetInstanceIdByName("only"), 0);
  EXPECT_EQ(const_collection.NameToIdMap().at("only"), 0);
  EXPECT_EQ(const_collection.Instances().front().Name(), "only");
}

TEST(NamedInstanceRegistryTest, ClearResetsFrozenStateAndContents) {
  dali::NamedInstanceRegistry<TestInstance> collection;
  collection.Create("old");
  collection.Freeze();

  collection.Clear();
  auto [created, created_id] = collection.CreateWithId("new");

  EXPECT_FALSE(collection.IsFrozen());
  EXPECT_EQ(collection.GetSize(), 1);
  EXPECT_EQ(created_id, 0);
  EXPECT_EQ(created.Name(), "new");
  EXPECT_FALSE(collection.NameExists("old"));
}
