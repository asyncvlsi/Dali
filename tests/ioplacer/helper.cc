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
 * @param phydb_ptr : the PhyDB instance containing the design with iopins to be placed
 */
void SetAllIoPinsToUnplaced(phydb::PhyDB *phydb_ptr) {
  for (auto &iopin: phydb_ptr->GetDesignPtr()->GetIoPinsRef()) {
    iopin.SetPlacementStatus(phydb::UNPLACED);
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
  if (iopin.place_status_ != phydb::PLACED
      && iopin.place_status_ != phydb::FIXED) {
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
 * @param phydb_ptr : the PhyDB instance to be checked
 * @return true, if all iopins are on placement boundaries; otherwise, false
 */
bool IsEveryIoPinPlacedOnBoundary(phydb::PhyDB *phydb_ptr) {
  if (phydb_ptr == nullptr) {
    BOOST_LOG_TRIVIAL(warning)
      << "Cannot check if every iopin is placed on placement boundaries for nullptr input\n";
    return false;
  }

  int left = phydb_ptr->GetDesignPtr()->die_area_.LLX();
  int right = phydb_ptr->GetDesignPtr()->die_area_.URX();
  int bottom = phydb_ptr->GetDesignPtr()->die_area_.LLY();
  int top = phydb_ptr->GetDesignPtr()->die_area_.URY();

  auto &iopins = phydb_ptr->GetDesignPtr()->GetIoPinsRef();

  bool res = true;
  for (auto &iopin: iopins) {
    bool is_on_boundary = IoPinPlacedOnBoundary(
        iopin, left, right, bottom, top
    );
    if (!is_on_boundary) {
      res = false;
    }
  }

  if (res) {
    BOOST_LOG_TRIVIAL(info)
      << "All iopins are placed on placement boundaries\n";
  } else {
    BOOST_LOG_TRIVIAL(info)
      << "Not all iopins are placed on placement boundaries\n";
  }

  return res;
}

bool IsIoPinNotOverlapping(phydb::IOPin &io_pin0, phydb::IOPin &io_pin1) {
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

/****
 * @brief Check if iopins do not overlap with each other
 *
 * This function simply checks if all iopins have no overlaps.
 *
 * @param phydb_ptr : the PhyDB instance to be checked
 * @return true, if all iopins have no overlaps; otherwise, false
 */
bool IsNoIoPinOverlap(phydb::PhyDB *phydb_ptr) {
  if (phydb_ptr == nullptr) {
    BOOST_LOG_TRIVIAL(warning)
      << "Cannot check if every iopin is placed on placement boundaries for nullptr input\n";
    return false;
  }

  auto &iopins = phydb_ptr->GetDesignPtr()->GetIoPinsRef();
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
    }
  }

  if (res) {
    BOOST_LOG_TRIVIAL(info)
      << "All iopins do not overlap with each other\n";
  } else {
    BOOST_LOG_TRIVIAL(info)
      << "Some iopins overlap with each other\n";
  }

  return res;
}

}