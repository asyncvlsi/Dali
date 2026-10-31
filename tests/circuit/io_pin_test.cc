#include "dali/circuit/circuit.h"
#include "dali/circuit/io_pin.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
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

TEST(IoPinTest, DefExportPreservesFixedAndCoverStatusAndOrientation) {
  Circuit c;
  c.SetManufacturingGrid(.001);
  c.SetGridValue(.2, .2);
  c.SetUnitsDistanceMicrons(1000);
  c.SetDieArea(0, 0, 10000, 10000);
  auto *layer = c.AddMetalLayer("m1", .01, .01, .0001, .2, .2, HORIZONTAL);
  c.ReserveSpaceForDesignImp(0, 2, 1);
  for (auto name : {"fixed", "cover"}) {
    auto *p = c.AddIoPin(name, name == std::string("fixed") ? FIXED : COVER,
                       SIGNAL, INPUT, 0, 10);
    p->SetInitPlaceStatus(name == std::string("fixed") ? FIXED : COVER);
    p->SetLoc(0, 10, name == std::string("fixed") ? FIXED : COVER);
    p->SetOrient(FS);
    p->SetLayerPtr(layer);
    p->SetShape(-.001, -.002, .001, .002);
  }
  c.AddNet("net", 2);
  c.AddIoPinToNet("fixed", "net");
  c.AddIoPinToNet("cover", "net");
  const auto root =
      std::filesystem::temp_directory_path() /
      ("dali-fixed-io-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directory(root);
  {
    std::ofstream f(root / "input.def");
    f << "VERSION 5.8 ;\nCOMPONENTS 0 ;\nEND COMPONENTS\nEND DESIGN\n";
  }
  for (int mode : {1, 2}) {
    c.SaveDefFile((root / "output").string(), "", (root / "input.def").string(),
                  1, 0, mode, 0);
    std::ifstream f(root / "output.def");
    const std::string text((std::istreambuf_iterator<char>(f)), {});
    EXPECT_NE(text.find("+ FIXED ( 0 2000 ) FS"), std::string::npos);
    EXPECT_NE(text.find("+ COVER ( 0 2000 ) FS"), std::string::npos);
    EXPECT_EQ(text.find("+ PLACED"), std::string::npos);
  }
  std::filesystem::remove_all(root);
}

} // namespace dali
