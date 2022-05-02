/*******************************************************************************
 *
 * Copyright (c) 2021 Yihang Yang
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
#include "helper.h"

#include "dali/circuit/iopin.h"
#include "dali/common/helper.h"
#include "dali/common/logging.h"
#include "dali/common/phydbhelper.h"

namespace dali {

/****
 * @brief Set all iopins to UNPLACED state
 *
 * The benchmark used for these testcases come from ISPD2019, and in this benchmark,
 * all iopins are PLACED/FIXED. In order to test the ioplacer, we will have to
 * modify this benchmark and make all iopins UNPLACED. This function changes the
 * placement status of all iopins to UNPLACED.
 *
 * @param p_phydb : the PhyDB instance containing the design with iopins to be placed
 */
void SetAllIoPinsToUnplaced(phydb::PhyDB *p_phydb) {
  for (auto &iopin: p_phydb->GetDesignPtr()->GetIoPinsRef()) {
    iopin.SetPlacementStatus(phydb::PlaceStatus::UNPLACED);
    //iopin.Report();
  }
}

bool IoPinPlacedOnBoundary(
    phydb::IOPin &iopin,
    int left,
    int right,
    int bottom,
    int top
) {
  if (iopin.place_status_ != phydb::PlaceStatus::PLACED
      && iopin.place_status_ != phydb::PlaceStatus::FIXED) {
    BOOST_LOG_TRIVIAL(info)
      << "placement status of iopin: " << iopin.GetName()
      << " is not PLACED or FIXED\n";
    return false;
  }
  bool res = true;
  int x = iopin.location_.x;
  int y = iopin.location_.y;
  if (x == left || x == right) {
    if (!(bottom <= y && y <= top)) {
      BOOST_LOG_TRIVIAL(info)
        << "x location of iopin " << iopin.GetName()
        << " is on a vertical boundary,"
        << "but y location is out of the boundary\n";
      res = false;
    }
  } else if (y == bottom || y == top) {
    if (!(left <= x && x <= right)) {
      BOOST_LOG_TRIVIAL(info)
        << "y location of iopin " << iopin.GetName()
        << " is on a horizontal boundary,"
        << "but x location is out of the boundary\n";
      res = false;
    }
  } else {
    BOOST_LOG_TRIVIAL(info)
      << "iopin " << iopin.GetName()
      << " is not on a boundary\n";
    res = false;
  }

  if (!res) {
    BOOST_LOG_TRIVIAL(info)
      << "  Pin location(" << x << ", " << y << ")\n"
      << "    size " << iopin.GetRect() << "\n";
    BOOST_LOG_TRIVIAL(info)
      << "  Placement boundary (" << left << ", " << bottom << "), ("
      << right << ", " << top << ")\n";
  }

  return res;
}

/****
 * @brief Check if every iopin is placed on placement boundaries
 *
 * This function simply checks if all iopins are placed on placement boundaries.
 *
 * @param p_phydb : the PhyDB instance to be checked
 * @return true, if all iopins are on placement boundaries; otherwise, false
 */
bool IsEveryIoPinPlacedOnBoundary(phydb::PhyDB *p_phydb) {
  if (p_phydb == nullptr) {
    BOOST_LOG_TRIVIAL(warning)
      << "Cannot check if every iopin is placed on placement boundaries for nullptr input\n";
    return false;
  }

  int left = p_phydb->GetDesignPtr()->die_area_.LLX();
  int right = p_phydb->GetDesignPtr()->die_area_.URX();
  int bottom = p_phydb->GetDesignPtr()->die_area_.LLY();
  int top = p_phydb->GetDesignPtr()->die_area_.URY();

  auto &iopins = p_phydb->GetDesignPtr()->GetIoPinsRef();

  bool res = true;
  for (auto &iopin: iopins) {
    bool is_on_boundary = IoPinPlacedOnBoundary(
        iopin, left, right, bottom, top
    );
    if (!is_on_boundary) {
      res = false;
    }
  }

  BOOST_LOG_TRIVIAL(info) << "All iopins are placed on placement boundaries? ";
  if (res) {
    BOOST_LOG_TRIVIAL(info) << "Yes\n";
  } else {
    BOOST_LOG_TRIVIAL(info) << "No\n";
  }

  return res;
}

bool IsIoPinNotOverlapping(phydb::IOPin &io_pin0, phydb::IOPin &io_pin1) {
  if (io_pin0.GetLayerName() != io_pin1.GetLayerName()) return true;

  phydb::Rect2D<int> shape0 = io_pin0.GetRect();
  int x0 = io_pin0.GetLocation().x;
  int y0 = io_pin0.GetLocation().y;
  BlockOrient orient0 = OrientPhyDB2Dali(io_pin0.GetOrientation());
  IoPin dali_pin0(x0, y0, orient0,
                  shape0.LLX(), shape0.LLY(), shape0.URX(), shape0.URY());

  phydb::Rect2D<int> shape1 = io_pin1.GetRect();
  int x1 = io_pin1.GetLocation().x;
  int y1 = io_pin1.GetLocation().y;
  BlockOrient orient1 = OrientPhyDB2Dali(io_pin1.GetOrientation());
  IoPin dali_pin1(x1, y1, orient1,
                  shape1.LLX(), shape1.LLY(), shape1.URX(), shape1.URY());

  bool no_overlap = dali_pin0.LX() > dali_pin1.UX() ||
      dali_pin1.LX() > dali_pin0.UX() ||
      dali_pin0.LY() > dali_pin1.UY() ||
      dali_pin1.LY() > dali_pin0.UY();

  if (!no_overlap) {
    BOOST_LOG_TRIVIAL(info)
      << "iopin " << io_pin0.GetName() << "\n"
      << "    loc: (" << x0 << ", " << y0 << ")"
      << "    size: " << shape0 << "\n"
      << "    orin: " << OrientStr(orient0) << "\n"
      << "    shape: (" << dali_pin0.LX() << ", " << dali_pin0.LY() << ") ("
      << dali_pin0.UX() << ", " << dali_pin0.UY() << ")\n"
      << "overlaps with \n"
      << "iopin " << io_pin0.GetName() << "\n"
      << "    loc: (" << x1 << ", " << y1 << ")"
      << "    size: " << shape1 << "\n"
      << "    orin: " << OrientStr(orient1) << "\n"
      << "    shape: (" << dali_pin1.LX() << ", " << dali_pin1.LY() << ") ("
      << dali_pin1.UX() << ", " << dali_pin1.UY() << ")\n";
  }

  return no_overlap;
}

bool IsIoPinNoSpacingViolation(
    phydb::PhyDB *p_phydb,
    phydb::IOPin &io_pin0,
    phydb::IOPin &io_pin1
) {
  if (io_pin0.GetLayerName() != io_pin1.GetLayerName()) return true;

  std::string layer_name = io_pin0.GetLayerName();
  phydb::Layer *p_layer = p_phydb->GetLayerPtr(layer_name);
  double d_spacing = p_layer->GetSpacing();
  int database_micron = p_phydb->tech().GetDatabaseMicron();
  double half_spacing = d_spacing * database_micron / 2.0;

  phydb::Rect2D<int> shape0 = io_pin0.GetRect();
  int x0 = io_pin0.GetLocation().x;
  int y0 = io_pin0.GetLocation().y;
  BlockOrient orient0 = OrientPhyDB2Dali(io_pin0.GetOrientation());
  IoPin dali_pin0(
      x0, y0, orient0,
      shape0.LLX() - half_spacing,
      shape0.LLY() - half_spacing,
      shape0.URX() + half_spacing,
      shape0.URY() + half_spacing
  );

  phydb::Rect2D<int> shape1 = io_pin1.GetRect();
  int x1 = io_pin1.GetLocation().x;
  int y1 = io_pin1.GetLocation().y;
  BlockOrient orient1 = OrientPhyDB2Dali(io_pin1.GetOrientation());
  IoPin dali_pin1(
      x1, y1, orient1,
      shape1.LLX() - half_spacing,
      shape1.LLY() - half_spacing,
      shape1.URX() + half_spacing,
      shape1.URY() + half_spacing
  );

  bool no_overlap = dali_pin0.LX() > dali_pin1.UX() ||
      dali_pin1.LX() > dali_pin0.UX() ||
      dali_pin0.LY() > dali_pin1.UY() ||
      dali_pin1.LY() > dali_pin0.UY();

  if (!no_overlap) {
    BOOST_LOG_TRIVIAL(info)
      << "iopin " << io_pin0.GetName() << "\n"
      << "    loc: (" << x0 << ", " << y0 << ")"
      << "    size: " << shape0 << "\n"
      << "    orin: " << OrientStr(orient0) << "\n"
      << "    shape(+- spacing/2.0): (" << dali_pin0.LX() << ", "
      << dali_pin0.LY() << ") ("
      << dali_pin0.UX() << ", " << dali_pin0.UY() << ")\n"
      << "violates the min-spacing rule with \n"
      << "iopin " << io_pin0.GetName() << "\n"
      << "    loc: (" << x1 << ", " << y1 << ")"
      << "    size: " << shape1 << "\n"
      << "    orin: " << OrientStr(orient1) << "\n"
      << "    shape(+- spacing/2.0): (" << dali_pin1.LX() << ", "
      << dali_pin1.LY() << ") ("
      << dali_pin1.UX() << ", " << dali_pin1.UY() << ")\n";
  }

  return no_overlap;
}

/****
 * @brief Check if iopins do not overlap with each other
 *
 * This function simply checks if all iopins have no overlaps.
 *
 * @param p_phydb : the PhyDB instance to be checked
 * @return true, if all iopins have no overlaps; otherwise, false
 */
bool IsNoIoPinOverlapAndSpacingViolation(phydb::PhyDB *p_phydb) {
  if (p_phydb == nullptr) {
    BOOST_LOG_TRIVIAL(warning)
      << "Cannot check if every iopin is placed on placement boundaries for nullptr input\n";
    return false;
  }

  auto &iopins = p_phydb->GetDesignPtr()->GetIoPinsRef();
  size_t sz = iopins.size();

  bool res = true;
  for (size_t i = 0; i < sz; ++i) {
    phydb::IOPin &io_pin0 = iopins[i];
    for (size_t j = i + 1; j < sz; ++j) {
      phydb::IOPin &io_pin1 = iopins[j];
      bool is_not_overlapping = IsIoPinNotOverlapping(io_pin0, io_pin1);
      if (!is_not_overlapping) {
        res = false;
      }
      bool is_spacing_satisfied = IsIoPinNoSpacingViolation(
          p_phydb, io_pin0, io_pin1
      );
      if (!is_spacing_satisfied) {
        res = false;
      }
    }
  }

  BOOST_LOG_TRIVIAL(info) << "All iopins satisfy spacing rules? ";
  if (res) {
    BOOST_LOG_TRIVIAL(info) << "Yes\n";
  } else {
    BOOST_LOG_TRIVIAL(info) << "No\n";
  }

  return res;
}

/****
 * @brief Check if all I/O pins are placed on the given metal layer
 *
 * This function checks if all I/O pins are placed on the correct metal layer.
 *
 * @param p_phydb
 * @param layer_name
 * @return true, if all on the given metal layer; otherwise, false
 */
bool IsEveryIoPinOnMetal(
    phydb::PhyDB *p_phydb,
    std::string const &layer_name
) {
  if (p_phydb == nullptr) {
    BOOST_LOG_TRIVIAL(warning)
      << "Cannot check if every iopin is placed on placement boundaries for nullptr input\n";
    return false;
  }

  auto &iopins = p_phydb->GetDesignPtr()->GetIoPinsRef();

  bool res = true;
  for (auto &iopin: iopins) {
    if (iopin.GetLayerName() != layer_name) {
      res = false;
      BOOST_LOG_TRIVIAL(info)
        << "iopin " << iopin.GetName()
        << " is not on the give layer: " << layer_name << "\n";
    }
  }

  BOOST_LOG_TRIVIAL(info)
    << "All iopins are on layer: " << layer_name << "? ";
  if (res) {
    BOOST_LOG_TRIVIAL(info) << "Yes\n";
  } else {
    BOOST_LOG_TRIVIAL(info) << "No\n";
  }

  return res;
}

bool IsEveryIoPinManufacturable(phydb::PhyDB *p_phydb) {
  if (p_phydb == nullptr) {
    BOOST_LOG_TRIVIAL(warning)
      << "Cannot check if every iopin is placed on placement boundaries for nullptr input\n";
    return false;
  }

  auto &iopins = p_phydb->GetDesignPtr()->GetIoPinsRef();
  bool res = true;
  for (auto &iopin: iopins) {
    int database_microns = p_phydb->tech().GetDatabaseMicron();
    double manufacturing_grid = p_phydb->tech().GetManufacturingGrid();
    phydb::Rect2D<int> shape = iopin.GetRect();
    double tmp_grid = database_microns * manufacturing_grid;
    DaliExpects(AbsResidual(tmp_grid, 1.0) < 1e-5,
                "DATABASE_MICRONS * MANUFACTURINGGRID is not close to an integer?");
    int grid = static_cast<int>(tmp_grid);
    bool is_manufacturable = (shape.LLX() % grid == 0) &&
        (shape.LLY() % grid == 0) &&
        (shape.URX() % grid == 0) &&
        (shape.URY() % grid == 0);

    if (!is_manufacturable) {
      res = false;
      BOOST_LOG_TRIVIAL(info)
        << "iopin " << iopin.GetName() << " has a rect: "
        << shape << " not on manufacturing grid\n"
        << "manufacturing grid: " << manufacturing_grid << " um, or "
        << grid << " database unit\n";
    }
  }

  BOOST_LOG_TRIVIAL(info) << "All iopins are manufacturable? ";
  if (res) {
    BOOST_LOG_TRIVIAL(info) << "Yes\n";
  } else {
    BOOST_LOG_TRIVIAL(info) << "No\n";
  }

  return res;
}

bool IsEveryIoPinInsideDieArea(phydb::PhyDB *p_phydb) {
  if (p_phydb == nullptr) {
    BOOST_LOG_TRIVIAL(warning)
      << "Cannot check if every iopin is placed on placement boundaries for nullptr input\n";
    return false;
  }

  auto &iopins = p_phydb->GetDesignPtr()->GetIoPinsRef();
  bool res = true;
  phydb::Rect2D<int> die_area = p_phydb->design().GetDieArea();
  for (auto &iopin: iopins) {
    phydb::Rect2D<int> shape = iopin.GetRect();
    int x = iopin.GetLocation().x;
    int y = iopin.GetLocation().y;
    BlockOrient orient = OrientPhyDB2Dali(iopin.GetOrientation());
    IoPin pin(x, y, orient,
              shape.LLX(), shape.LLY(), shape.URX(), shape.URY());

    bool is_in_die_area =
        (pin.LX() >= die_area.LLX() && pin.LX() <= die_area.URX())
            && (pin.UX() >= die_area.LLX() && pin.UX() <= die_area.URX())
            && (pin.LY() >= die_area.LLY() && pin.LY() <= die_area.URY())
            && (pin.UY() >= die_area.LLY() && pin.UY() <= die_area.URY());
    if (!is_in_die_area) {
      res = false;
      BOOST_LOG_TRIVIAL(info)
        << "iopin " << iopin.GetName() << "is out of die area: "
        << "(" << pin.LX() << ", " << pin.LY() << ") ("
        << pin.UX() << ", " << pin.UY() << ")\n"
        << " die area: " << die_area << "\n";
    }
  }

  BOOST_LOG_TRIVIAL(info) << "All iopins are inside the diea area? ";
  if (res) {
    BOOST_LOG_TRIVIAL(info) << "Yes\n";
  } else {
    BOOST_LOG_TRIVIAL(info) << "No\n";
  }
  return res;
}

void RemoveAllIoPins(phydb::PhyDB *p_phydb) {
  if (p_phydb == nullptr) {
    BOOST_LOG_TRIVIAL(warning)
      << "Cannot remove I/O pins for a nullptr input\n";
    return;
  }
  p_phydb->design().iopins_.clear();
  p_phydb->design().iopin_2_id_.clear();
  for (auto &phydb_net: p_phydb->design().GetNetsRef()) {
    phydb_net.GetIoPinIdsRef().clear();
  }
}

bool IsEveryIoPinAddedAndPlacedCorrectly(
    phydb::PhyDB *p_phydb0,
    phydb::PhyDB *p_phydb1
) {
  if (p_phydb0 == nullptr || p_phydb1 == nullptr) {
    BOOST_LOG_TRIVIAL(warning)
      << "Cannot check the correctness for null PhyDB pointers\n";
    return false;
  }

  bool res = true;
  for (auto &iopin0: p_phydb0->design().iopins_) {
    auto *iopin1 = p_phydb1->GetIoPinPtr(iopin0.GetName());
    if (iopin1 == nullptr) {
      BOOST_LOG_TRIVIAL(info)
        << "Cannot find " << iopin0.GetName() << " in the second instance\n";
      res = false;
      break;
    }
    if (iopin0.GetDirection() != iopin1->GetDirection()) {
      BOOST_LOG_TRIVIAL(info) << "different direction\n";
      res = false;
      break;
    }
    if (iopin0.GetUse() != iopin1->GetUse()) {
      BOOST_LOG_TRIVIAL(info) << "different use\n";
      res = false;
      break;
    }
    if (iopin0.GetNetName() != iopin1->GetNetName()) {
      BOOST_LOG_TRIVIAL(info) << "different net name\n";
      res = false;
      break;
    }
    if (iopin0.GetLayerName() != iopin1->GetLayerName()) {
      BOOST_LOG_TRIVIAL(info) << "different layer name\n";
      res = false;
      break;
    }
    if (iopin0.GetRect().LLX() != iopin1->GetRect().LLX()) {
      BOOST_LOG_TRIVIAL(info) << "different shape llx\n";
      res = false;
      break;
    }
    if (iopin0.GetRect().LLY() != iopin1->GetRect().LLY()) {
      BOOST_LOG_TRIVIAL(info) << "different shape lly\n";
      res = false;
      break;
    }
    if (iopin0.GetRect().URX() != iopin1->GetRect().URX()) {
      BOOST_LOG_TRIVIAL(info) << "different shape urx\n";
      res = false;
      break;
    }
    if (iopin0.GetRect().URY() != iopin1->GetRect().URY()) {
      BOOST_LOG_TRIVIAL(info) << "different shape ury\n";
      res = false;
      break;
    }
    if (iopin0.GetPlacementStatus() != iopin1->GetPlacementStatus()) {
      BOOST_LOG_TRIVIAL(info) << "different placement status\n";
      res = false;
      break;
    }
    if (iopin0.GetOrientation() != iopin1->GetOrientation()) {
      BOOST_LOG_TRIVIAL(info) << "different orientation\n";
      res = false;
      break;
    }
    if (iopin0.GetLocation().x != iopin1->GetLocation().x) {
      BOOST_LOG_TRIVIAL(info) << "different location x\n";
      res = false;
      break;
    }
    if (iopin0.GetLocation().y != iopin1->GetLocation().y) {
      BOOST_LOG_TRIVIAL(info) << "different location y\n";
      res = false;
      break;
    }
  }

  BOOST_LOG_TRIVIAL(info) << "All iopins are correctly added and placed? ";
  if (res) {
    BOOST_LOG_TRIVIAL(info) << "Yes\n";
  } else {
    BOOST_LOG_TRIVIAL(info) << "No\n";
  }
  return res;
}

}
