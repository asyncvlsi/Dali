//
// Created by Yihang Yang on 9/10/2021.
//

#include "helper.h"

#include "dali/common/logging.h"

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
            << "placement status of iopin: "
            << iopin.GetName()
            << " is not PLACED or FIXED\n";
        return false;
    }
    bool res = true;
    int x = iopin.location_.x;
    int y = iopin.location_.y;
    if (x == left || x == right) {
        if (!(bottom <= y && y <= top)) {
            BOOST_LOG_TRIVIAL(info)
                << "x location of iopin: "
                << iopin.GetName()
                << " is on a vertical boundary,"
                << "but y location is out of the boundary\n";
            res = false;
        }
    } else if (y == bottom || y == top) {
        if (!(left <= x && x <= right)) {
            BOOST_LOG_TRIVIAL(info)
                << "y location of iopin: "
                << iopin.GetName()
                << " is on a horizontal boundary,"
                << "but x location is out of the boundary\n";
            res = false;
        }
    } else {
        BOOST_LOG_TRIVIAL(info)
            << "iopin: "
            << iopin.GetName()
            << " is not on a boundary\n";
        res = false;
    }

    if (!res) {
        BOOST_LOG_TRIVIAL(info)
            << "  Placement boundary (" << left << ", " << bottom << "), ("
            << right << ", " << top << ")\n";
        BOOST_LOG_TRIVIAL(info)
            << "  Pin location(" << x << ", " << y << ")\n";
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
            iopin,
            left,
            right,
            bottom,
            top
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

    return true;
}

bool IsNoIoPinOverlap(phydb::PhyDB *phydb_ptr) {

    return true;
}

}