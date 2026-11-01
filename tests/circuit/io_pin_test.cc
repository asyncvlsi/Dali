#include "dali/circuit/io_pin.h"

#include <gtest/gtest.h>

#include <unordered_map>

namespace dali {

TEST(IoPinTest, ExposesShapeStatusAndOrientationAccessors) {
  std::unordered_map<std::string, int> names;
  auto [it, inserted] = names.emplace("io0", 0);
  ASSERT_TRUE(inserted);

  IoPin io_pin(&(*it));
  io_pin.SetShape(-1, -2, 3, 4);
  io_pin.SetLoc(10, 20, PLACED);
  io_pin.SetOrient(S);

  EXPECT_TRUE(io_pin.IsShapeSet());
  EXPECT_EQ(io_pin.Shape().LLX(), -1);
  EXPECT_EQ(io_pin.Shape().LLY(), -2);
  EXPECT_EQ(io_pin.Shape().URX(), 3);
  EXPECT_EQ(io_pin.Shape().URY(), 4);
  EXPECT_EQ(io_pin.Status(), PLACED);
  EXPECT_EQ(io_pin.Orient(), S);
}

}  // namespace dali
