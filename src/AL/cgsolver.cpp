//
// Created by Yihang Yang on 2019-06-11.
//

#include "cgsolver.hpp"
#include <cmath>

void cg_solver::initialize_HPWL_flags() {
  HPWLX_new = 0;
  HPWLY_new = 0;
  HPWLX_old = 1e30;
  HPWLY_old = 1e30;
  HPWLx_converge = false;
  HPWLy_converge = false;
}

void cg_solver::cg_init() {
  // this init function allocate memory to Ax and Ay
  // the size of memory allocated for each row is the maximum memory which might be used
  Ax.clear(); Ay.clear(); kx.clear(); ky.clear(); bx.clear(); by.clear();
  for (size_t i=0; i<CELL_NUM; i++) {
    // initialize bx, by
    bx.push_back(0);
    by.push_back(0);
    // initialize kx, ky to track the length of Ax[i] and Ay[i]
    kx.push_back(0);
    ky.push_back(0);
  }
  std::vector<weight_tuple> tempArow;
  for (size_t i=0; i<CELL_NUM; i++) {
    Ax.push_back(tempArow);
    Ay.push_back(tempArow);
  }
  weight_tuple tempWT;
  tempWT.weight = 0;
  for (size_t i=0; i<CELL_NUM; i++) {
    tempWT.pin = i;
    Ax[i].push_back(tempWT);
    Ay[i].push_back(tempWT);
  }
  size_t tempnodenum0, tempnodenum1;
  for (auto &&net: Netlist) {
    if (net.p<=1) continue;
    for (size_t j=0; j<net.pinlist.size(); j++) {
      tempnodenum0 = net.pinlist[j].pinnum;
      for (size_t k=j+1; k<net.pinlist.size(); k++) {
        tempnodenum1 = net.pinlist[k].pinnum;
        if (tempnodenum0 == tempnodenum1) continue;
        if ((Nodelist[tempnodenum0].isterminal() == 0)&&(Nodelist[tempnodenum1].isterminal() == 0)) {
          // when both nodes are movable and are not the same, increase the row length by 1
          Ax[tempnodenum0].push_back(tempWT);
          Ax[tempnodenum1].push_back(tempWT);
          Ay[tempnodenum0].push_back(tempWT);
          Ay[tempnodenum1].push_back(tempWT);
        }
      }
    }
  }
}

void cg_solver::build_problem_clique_x() {
  // before build a new Matrix, clean the information in existing matrix
  for (size_t i=0; i<CELL_NUM; i++) {
    Ax[i][0].weight = 0;
    // make the diagonal elements 0
    kx[i] = 0;
    // mark the length each row of matrix 0, although some data might still exists there
    bx[i] = 0;
    // make each element of b 0
  }

  weight_tuple tempWT;
  float weightx, invp, temppinlocx0, temppinlocx1, tempdiffoffset;
  size_t tempnodenum0, tempnodenum1;

  for (auto &&net: Netlist) {
    //std::cout << i << "\n";
    if (net.p<=1) continue;
    invp = net.invpmin1;
    for (size_t j=0; j<net.pinlist.size(); j++) {
      tempnodenum0 = net.pinlist[j].pinnum;
      temppinlocx0 = Nodelist[tempnodenum0].x0 + net.pinlist[j].xoffset;
      for (size_t k=j+1; k<net.pinlist.size(); k++) {
        tempnodenum1 = net.pinlist[k].pinnum;
        temppinlocx1 = Nodelist[tempnodenum1].x0 + net.pinlist[k].xoffset;
        if (tempnodenum0 == tempnodenum1) continue;
        if ((Nodelist[tempnodenum0].isterminal() == 1)&&(Nodelist[tempnodenum1].isterminal() == 1)) {
          continue;
        }
        weightx = invp/((float)fabs(temppinlocx0 - temppinlocx1) + WEPSI);
        if ((Nodelist[tempnodenum0].isterminal() == 1)&&(Nodelist[tempnodenum1].isterminal() == 0)) {
          bx[tempnodenum1] += (temppinlocx0 - net.pinlist[k].xoffset) * weightx;
          Ax[tempnodenum1][0].weight += weightx;
        }
        else if ((Nodelist[tempnodenum0].isterminal() == 0)&&(Nodelist[tempnodenum1].isterminal() == 1)) {
          bx[tempnodenum0] += (temppinlocx1 - net.pinlist[j].xoffset) * weightx;
          Ax[tempnodenum0][0].weight += weightx;
        }
        else {
          //((Nodelist[tempnodenum0].isterminal() == 0)&&(Nodelist[tempnodenum1].isterminal() == 0))
          tempWT.pin = tempnodenum1;
          tempWT.weight = -weightx;
          kx[tempnodenum0]++;
          Ax[tempnodenum0][kx[tempnodenum0]] = tempWT;

          tempWT.pin = tempnodenum0;
          tempWT.weight = -weightx;
          kx[tempnodenum1]++;
          Ax[tempnodenum1][kx[tempnodenum1]] = tempWT;

          Ax[tempnodenum0][0].weight += weightx;
          Ax[tempnodenum1][0].weight += weightx;
          tempdiffoffset = (net.pinlist[k].xoffset - net.pinlist[j].xoffset) * weightx;
          bx[tempnodenum0] += tempdiffoffset;
          bx[tempnodenum1] -= tempdiffoffset;
        }
      }
    }
  }
}

void cg_solver::build_problem_clique_y() {
  // before build a new Matrix, clean the information in existing matrix
  for (size_t i=0; i<CELL_NUM; i++) {
    Ay[i][0].weight = 0;
    // make the diagonal elements 0
    ky[i] = 0;
    // mark the length each row of matrix 0, although some data might still exists there
    by[i] = 0;
    // make each element of b 0
  }

  weight_tuple tempWT;
  float weighty, invp, temppinlocy0, temppinlocy1, tempdiffoffset;
  size_t tempnodenum0, tempnodenum1;

  for (auto &&net: Netlist) {
    //std::cout << i << "\n";
    if (net.p<=1) continue;
    invp = net.invpmin1;
    for (size_t j=0; j<net.pinlist.size(); j++) {
      tempnodenum0 = net.pinlist[j].pinnum;
      temppinlocy0 = Nodelist[tempnodenum0].y0 + net.pinlist[j].yoffset;
      for (size_t k=j+1; k<net.pinlist.size(); k++) {
        tempnodenum1 = net.pinlist[k].pinnum;
        temppinlocy1 = Nodelist[tempnodenum1].y0 + net.pinlist[k].yoffset;
        if (tempnodenum0 == tempnodenum1) continue;
        if ((Nodelist[tempnodenum0].isterminal() == 1)&&(Nodelist[tempnodenum1].isterminal() == 1)) {
          continue;
        }
        weighty = invp/((float)fabs(temppinlocy0 - temppinlocy1) + HEPSI);
        if ((Nodelist[tempnodenum0].isterminal() == 1)&&(Nodelist[tempnodenum1].isterminal() == 0)) {
          by[tempnodenum1] += (temppinlocy0 - net.pinlist[k].yoffset) * weighty;
          Ay[tempnodenum1][0].weight += weighty;
        }
        else if ((Nodelist[tempnodenum0].isterminal() == 0)&&(Nodelist[tempnodenum1].isterminal() == 1)) {
          by[tempnodenum0] += (temppinlocy1 - net.pinlist[j].yoffset) * weighty;
          Ay[tempnodenum0][0].weight += weighty;
        }
        else {
          //((Nodelist[tempnodenum0].isterminal() == 0)&&(Nodelist[tempnodenum1].isterminal() == 0))
          tempWT.pin = tempnodenum1;
          tempWT.weight = -weighty;
          ky[tempnodenum0]++;
          Ay[tempnodenum0][ky[tempnodenum0]] = tempWT;

          tempWT.pin = tempnodenum0;
          tempWT.weight = -weighty;
          ky[tempnodenum1]++;
          Ay[tempnodenum1][ky[tempnodenum1]] = tempWT;

          Ay[tempnodenum0][0].weight += weighty;
          Ay[tempnodenum1][0].weight += weighty;
          tempdiffoffset = (net.pinlist[k].yoffset - net.pinlist[j].yoffset) * weighty;
          by[tempnodenum0] += tempdiffoffset;
          by[tempnodenum1] -= tempdiffoffset;
        }
      }
    }
  }
}

void cg_solver::update_HPWL_x() {
  // update the y direction max and min node in each net
  float max_pin_loc_x, min_pin_loc_x;
  size_t maxpindex_x, minpindex_x;
  HPWLX_new = 0;
  for (auto &&net: Netlist) {
    maxpindex_x = 0;
    max_pin_loc_x = Nodelist[net.pinlist[0].pinnum].x0 + net.pinlist[0].xoffset;
    minpindex_x = 0;
    min_pin_loc_x = Nodelist[net.pinlist[0].pinnum].x0 + net.pinlist[0].xoffset;
    for (size_t i=0; i<net.pinlist.size(); i++) {
      net.pinlist[i].x = Nodelist[net.pinlist[i].pinnum].x0 + net.pinlist[i].xoffset;
      //std::cout << net.pinlist[i].x << " ";
      if (net.pinlist[i].x > max_pin_loc_x) {
        maxpindex_x = i;
        max_pin_loc_x = net.pinlist[i].x;
      }
      else if (net.pinlist[i].x < min_pin_loc_x) {
        minpindex_x = i;
        min_pin_loc_x = net.pinlist[i].x;
      }
      else {
        ;
      }
    }
    net.maxpindex_x = maxpindex_x;
    net.minpindex_x = minpindex_x;
    net.hpwlx = max_pin_loc_x - min_pin_loc_x;
    HPWLX_new += net.hpwlx;
    //std::cout << "Max: " << Nodelist[net.pinlist[net.maxpindex_x].pinnum].x0 + net.pinlist[net.maxpindex_x].xoffset << " ";
    //std::cout << "Min: " << Nodelist[net.pinlist[net.minpindex_x].pinnum].x0 + net.pinlist[net.minpindex_x].xoffset << "\n";
  }
}

void cg_solver::update_max_min_node_x() {
  // update the y direction max and min node in each net
  float max_pin_loc_x, min_pin_loc_x;
  size_t maxpindex_x, minpindex_x;
  HPWLX_new = 0;
  for (auto &&net: Netlist) {
    maxpindex_x = 0;
    max_pin_loc_x = Nodelist[net.pinlist[0].pinnum].x0 + net.pinlist[0].xoffset;
    minpindex_x = 0;
    min_pin_loc_x = Nodelist[net.pinlist[0].pinnum].x0 + net.pinlist[0].xoffset;
    for (size_t i=0; i<net.pinlist.size(); i++) {
      net.pinlist[i].x = Nodelist[net.pinlist[i].pinnum].x0 + net.pinlist[i].xoffset;
      //std::cout << net.pinlist[i].x << " ";
      if (net.pinlist[i].x > max_pin_loc_x) {
        maxpindex_x = i;
        max_pin_loc_x = net.pinlist[i].x;
      }
      else if (net.pinlist[i].x < min_pin_loc_x) {
        minpindex_x = i;
        min_pin_loc_x = net.pinlist[i].x;
      }
      else {
        ;
      }
    }
    net.maxpindex_x = maxpindex_x;
    net.minpindex_x = minpindex_x;
    net.hpwlx = max_pin_loc_x - min_pin_loc_x;
    HPWLX_new += net.hpwlx;
    //std::cout << "Max: " << Nodelist[net.pinlist[net.maxpindex_x].pinnum].x0 + net.pinlist[net.maxpindex_x].xoffset << " ";
    //std::cout << "Min: " << Nodelist[net.pinlist[net.minpindex_x].pinnum].x0 + net.pinlist[net.minpindex_x].xoffset << "\n";
  }
  //std::cout << "HPWLX_old: " << HPWLX_old << "\n";
  //std::cout << "HPWLX_new: " << HPWLX_new << "\n";
  //std::cout << 1 - HPWLX_new/HPWLX_old << "\n";
  if (std::fabs(1 - HPWLX_new/HPWLX_old) < HPWL_intra_linearSolver_precision) {
    HPWLx_converge = true;
  } else {
    HPWLx_converge = false;
  }
  HPWLX_old = HPWLX_new;
}

void cg_solver::build_problem_b2b_x() {
  // before build a new Matrix, clean the information in existing matrix
  for (size_t i=0; i<CELL_NUM; i++) {
    Ax[i][0].weight = 0;
    // make the diagonal elements 0
    kx[i] = 0;
    // mark the length each row of matrix 0, although some data might still exists there
    bx[i] = 0;
    // make each element of b 0
  }
  // update the x direction max and min node in each net
  weight_tuple tempWT;
  float weightx, invp, temppinlocx0, temppinlocx1, tempdiffoffset;
  size_t tempnodenum0, tempnodenum1, maxpindex_x, minpindex_x;
  for (auto &&net: Netlist) {
    if (net.p<=1) continue;
    invp = net.invpmin1;
    maxpindex_x = net.maxpindex_x;
    minpindex_x = net.minpindex_x;
    for (size_t i=0; i<net.pinlist.size(); i++) {
      tempnodenum0 = net.pinlist[i].pinnum;
      temppinlocx0 = net.pinlist[i].x;
      for (size_t k=i+1; k<net.pinlist.size(); k++) {
        if ((i!=maxpindex_x)&&(i!=minpindex_x)) {
          if ((k!=maxpindex_x)&&(k!=minpindex_x)) continue;
        }
        tempnodenum1 = net.pinlist[k].pinnum;
        if (tempnodenum0 == tempnodenum1) continue;
        temppinlocx1 = net.pinlist[k].x;
        weightx = invp/((float)fabs(temppinlocx0 - temppinlocx1) + WEPSI);
        if ((Nodelist[tempnodenum0].isterminal() == 1)&&(Nodelist[tempnodenum1].isterminal() == 1)) {
          continue;
        }
        else if ((Nodelist[tempnodenum0].isterminal() == 1)&&(Nodelist[tempnodenum1].isterminal() == 0)) {
          bx[tempnodenum1] += (temppinlocx0 - net.pinlist[k].xoffset) * weightx;
          Ax[tempnodenum1][0].weight += weightx;
        }
        else if ((Nodelist[tempnodenum0].isterminal() == 0)&&(Nodelist[tempnodenum1].isterminal() == 1)) {
          bx[tempnodenum0] += (temppinlocx1 - net.pinlist[i].xoffset) * weightx;
          Ax[tempnodenum0][0].weight += weightx;
        }
        else {
          //((Nodelist[tempnodenum0].isterminal() == 0)&&(Nodelist[tempnodenum1].isterminal() == 0))
          tempWT.pin = tempnodenum1;
          tempWT.weight = -weightx;
          kx[tempnodenum0]++;
          /*if (kx[tempnodenum0] == Ax[tempnodenum0].size()) {
            std::cout << tempnodenum0 << " " << kx[tempnodenum0] << " Overflowx\n";
          }*/
          Ax[tempnodenum0][kx[tempnodenum0]] = tempWT;

          tempWT.pin = tempnodenum0;
          tempWT.weight = -weightx;
          kx[tempnodenum1]++;
          /*if (kx[tempnodenum1] == Ax[tempnodenum1].size()) {
            std::cout << tempnodenum1 << " " << kx[tempnodenum1] << " Overflowx\n";
          }*/
          Ax[tempnodenum1][kx[tempnodenum1]] = tempWT;

          Ax[tempnodenum0][0].weight += weightx;
          Ax[tempnodenum1][0].weight += weightx;
          tempdiffoffset = (net.pinlist[k].xoffset - net.pinlist[i].xoffset) * weightx;
          bx[tempnodenum0] += tempdiffoffset;
          bx[tempnodenum1] -= tempdiffoffset;
        }
      }
    }
  }
}

void cg_solver::build_problem_b2b_x_nooffset() {
  // before build a new Matrix, clean the information in existing matrix
  for (size_t i=0; i<CELL_NUM; i++) {
    Ax[i][0].weight = 0;
    // make the diagonal elements 0
    kx[i] = 0;
    // mark the length each row of matrix 0, although some data might still exists there
    bx[i] = 0;
    // make each element of b 0
  }
  weight_tuple tempWT;
  float weightx, invp, temppinlocx0, temppinlocx1;
  size_t tempnodenum0, tempnodenum1, maxpindex_x, minpindex_x;

  for (auto &&net: Netlist) {
    if (net.p<=1) continue;
    invp = net.invpmin1;
    maxpindex_x = net.maxpindex_x;
    minpindex_x = net.minpindex_x;
    for (size_t i=0; i<net.pinlist.size(); i++) {
      tempnodenum0 = net.pinlist[i].pinnum;
      temppinlocx0 = Nodelist[tempnodenum0].x0;
      for (size_t k=i+1; k<net.pinlist.size(); k++) {
        if ((i!=maxpindex_x)&&(i!=minpindex_x)) {
          if ((k!=maxpindex_x)&&(k!=minpindex_x)) continue;
        }
        tempnodenum1 = net.pinlist[k].pinnum;
        if (tempnodenum0 == tempnodenum1) continue;
        temppinlocx1 = Nodelist[tempnodenum1].x0;
        weightx = invp/((float)fabs(temppinlocx0 - temppinlocx1) + WEPSI);
        if ((Nodelist[tempnodenum0].isterminal() == 1)&&(Nodelist[tempnodenum1].isterminal() == 1)) {
          continue;
        }
        else if ((Nodelist[tempnodenum0].isterminal() == 1)&&(Nodelist[tempnodenum1].isterminal() == 0)) {
          bx[tempnodenum1] += temppinlocx0 * weightx;
          Ax[tempnodenum1][0].weight += weightx;
        }
        else if ((Nodelist[tempnodenum0].isterminal() == 0)&&(Nodelist[tempnodenum1].isterminal() == 1)) {
          bx[tempnodenum0] += temppinlocx1 * weightx;
          Ax[tempnodenum0][0].weight += weightx;
        }
        else {
          //((Nodelist[tempnodenum0].isterminal() == 0)&&(Nodelist[tempnodenum1].isterminal() == 0))
          tempWT.pin = tempnodenum1;
          tempWT.weight = -weightx;
          kx[tempnodenum0]++;
          /*if (kx[tempnodenum0] == Ax[tempnodenum0].size()) {
            std::cout << tempnodenum0 << " " << kx[tempnodenum0] << " Overflowx\n";
          }*/
          Ax[tempnodenum0][kx[tempnodenum0]] = tempWT;

          tempWT.pin = tempnodenum0;
          tempWT.weight = -weightx;
          kx[tempnodenum1]++;
          /*if (kx[tempnodenum1] == Ax[tempnodenum1].size()) {
            std::cout << tempnodenum1 << " " << kx[tempnodenum1] << " Overflowx\n";
          }*/
          Ax[tempnodenum1][kx[tempnodenum1]] = tempWT;

          Ax[tempnodenum0][0].weight += weightx;
          Ax[tempnodenum1][0].weight += weightx;
        }
      }
    }
  }
}

void cg_solver::update_HPWL_y() {
  // update the y direction max and min node in each net
  float maxpinlocy, minpinlocy;
  size_t maxpindex_y, minpindex_y;
  HPWLY_new = 0;
  for (auto &&net: Netlist) {
    maxpindex_y = 0;
    maxpinlocy = Nodelist[net.pinlist[0].pinnum].y0 + net.pinlist[0].yoffset;
    minpindex_y = 0;
    minpinlocy = Nodelist[net.pinlist[0].pinnum].y0 + net.pinlist[0].yoffset;
    for (size_t i=0; i<net.pinlist.size(); i++) {
      net.pinlist[i].y = Nodelist[net.pinlist[i].pinnum].y0 + net.pinlist[i].yoffset;
      //std::cout << net.pinlist[i].y << " ";
      if (net.pinlist[i].y > maxpinlocy) {
        maxpindex_y = i;
        maxpinlocy = net.pinlist[i].y;
      }
      else if (net.pinlist[i].y < minpinlocy) {
        minpindex_y = i;
        minpinlocy = net.pinlist[i].y;
      }
      else {
        ;
      }
    }
    net.maxpindex_y = maxpindex_y;
    net.minpindex_y = minpindex_y;
    net.hpwly = maxpinlocy - minpinlocy;
    HPWLY_new += net.hpwly;
    //std::cout << "Max: " << Nodelist[net.pinlist[net.maxpindex_y].pinnum].y0 + net.pinlist[net.maxpindex_y].yoffset << " ";
    //std::cout << "Min: " << Nodelist[net.pinlist[net.minpindex_y].pinnum].y0 + net.pinlist[net.minpindex_y].yoffset << "\n";
  }
}

void cg_solver::update_max_min_node_y() {
  // update the y direction max and min node in each net
  float maxpinlocy, minpinlocy;
  size_t maxpindex_y, minpindex_y;
  HPWLY_new = 0;
  for (auto &&net: Netlist) {
    maxpindex_y = 0;
    maxpinlocy = Nodelist[net.pinlist[0].pinnum].y0 + net.pinlist[0].yoffset;
    minpindex_y = 0;
    minpinlocy = Nodelist[net.pinlist[0].pinnum].y0 + net.pinlist[0].yoffset;
    for (size_t i=0; i<net.pinlist.size(); i++) {
      net.pinlist[i].y = Nodelist[net.pinlist[i].pinnum].y0 + net.pinlist[i].yoffset;
      //std::cout << net.pinlist[i].y << " ";
      if (net.pinlist[i].y > maxpinlocy) {
        maxpindex_y = i;
        maxpinlocy = net.pinlist[i].y;
      }
      else if (net.pinlist[i].y < minpinlocy) {
        minpindex_y = i;
        minpinlocy = net.pinlist[i].y;
      }
      else {
        ;
      }
    }
    net.maxpindex_y = maxpindex_y;
    net.minpindex_y = minpindex_y;
    net.hpwly = maxpinlocy - minpinlocy;
    HPWLY_new += net.hpwly;
    //std::cout << "Max: " << Nodelist[net.pinlist[net.maxpindex_y].pinnum].y0 + net.pinlist[net.maxpindex_y].yoffset << " ";
    //std::cout << "Min: " << Nodelist[net.pinlist[net.minpindex_y].pinnum].y0 + net.pinlist[net.minpindex_y].yoffset << "\n";
  }
  //std::cout << "HPWLY_old: " << HPWLY_old << "\n";
  //std::cout << "HPWLY_new: " << HPWLY_new << "\n";
  //std::cout << 1 - HPWLY_new/HPWLY_old << "\n";
  if (std::fabs(1 - HPWLY_new/HPWLY_old) < HPWL_intra_linearSolver_precision) {
    HPWLy_converge = true;
  } else {
    HPWLy_converge = false;
  }
  HPWLY_old = HPWLY_new;
}

void cg_solver::build_problem_b2b_y() {
  // before build a new Matrix, clean the information in existing matrix
  for (size_t i=0; i<CELL_NUM; i++) {
    Ay[i][0].weight = 0;
    // make the diagonal elements 0
    ky[i] = 0;
    // mark the length each row of matrix 0, although some data might still exists there
    by[i] = 0;
    // make each element of b 0
  }
  weight_tuple tempWT;
  float weighty, invp, temppinlocy0, temppinlocy1, tempdiffoffset;
  size_t tempnodenum0, tempnodenum1, maxpindex_y, minpindex_y;
  for (auto &&net: Netlist) {
    if (net.p<=1) continue;
    invp = net.invpmin1;
    maxpindex_y = net.maxpindex_y;
    minpindex_y = net.minpindex_y;
    for (size_t i=0; i<net.pinlist.size(); i++) {
      tempnodenum0 = net.pinlist[i].pinnum;
      temppinlocy0 = net.pinlist[i].y;
      for (size_t k=i+1; k<net.pinlist.size(); k++) {
        if ((i!=maxpindex_y)&&(i!=minpindex_y)) {
          if ((k!=maxpindex_y)&&(k!=minpindex_y)) continue;
        }
        tempnodenum1 = net.pinlist[k].pinnum;
        if (tempnodenum0 == tempnodenum1) continue;
        temppinlocy1 = net.pinlist[k].y;
        weighty = invp/((float)fabs(temppinlocy0 - temppinlocy1) + HEPSI);
        if ((Nodelist[tempnodenum0].isterminal() == 1)&&(Nodelist[tempnodenum1].isterminal() == 1)) {
          continue;
        }
        else if ((Nodelist[tempnodenum0].isterminal() == 1)&&(Nodelist[tempnodenum1].isterminal() == 0)) {
          by[tempnodenum1] += (temppinlocy0 - net.pinlist[k].yoffset) * weighty;
          Ay[tempnodenum1][0].weight += weighty;
        }
        else if ((Nodelist[tempnodenum0].isterminal() == 0)&&(Nodelist[tempnodenum1].isterminal() == 1)) {
          by[tempnodenum0] += (temppinlocy1 - net.pinlist[i].yoffset) * weighty;
          Ay[tempnodenum0][0].weight += weighty;
        }
        else {
          //((Nodelist[tempnodenum0].isterminal() == 0)&&(Nodelist[tempnodenum1].isterminal() == 0))
          tempWT.pin = tempnodenum1;
          tempWT.weight = -weighty;
          ky[tempnodenum0]++;
          /*if (ky[tempnodenum0] == Ay[tempnodenum0].size()) {
            std::cout << tempnodenum0 << " " << ky[tempnodenum0] << " Overflowy\n";
          }*/
          Ay[tempnodenum0][ky[tempnodenum0]] = tempWT;

          tempWT.pin = tempnodenum0;
          tempWT.weight = -weighty;
          ky[tempnodenum1]++;
          /*if (ky[tempnodenum1] == Ay[tempnodenum1].size()) {
            std::cout << tempnodenum1 << " " << ky[tempnodenum1] << " Overflowy\n";
          }*/
          Ay[tempnodenum1][ky[tempnodenum1]] = tempWT;

          Ay[tempnodenum0][0].weight += weighty;
          Ay[tempnodenum1][0].weight += weighty;
          tempdiffoffset = (net.pinlist[k].yoffset - net.pinlist[i].yoffset) * weighty;
          by[tempnodenum0] += tempdiffoffset;
          by[tempnodenum1] -= tempdiffoffset;
        }
      }
    }
  }
}

void cg_solver::build_problem_b2b_y_nooffset() {
  // before build a new Matrix, clean the information in existing matrix
  for (size_t i=0; i<CELL_NUM; i++) {
    Ay[i][0].weight = 0;
    // make the diagonal elements 0
    ky[i] = 0;
    // mark the length each row of matrix 0, although some data might still exists there
    by[i] = 0;
    // make each element of b 0
  }
  weight_tuple tempWT;
  float weighty, invp, temppinlocy0, temppinlocy1;
  size_t tempnodenum0, tempnodenum1, maxpindex_y, minpindex_y;
  for (auto &&net: Netlist) {
    if (net.p<=1) continue;
    invp = net.invpmin1;
    maxpindex_y = net.maxpindex_y;
    minpindex_y = net.minpindex_y;
    for (size_t i=0; i<net.pinlist.size(); i++) {
      tempnodenum0 = net.pinlist[i].pinnum;
      temppinlocy0 = Nodelist[tempnodenum0].y0;
      for (size_t k=i+1; k<net.pinlist.size(); k++) {
        if ((i!=maxpindex_y)&&(i!=minpindex_y)) {
          if ((k!=maxpindex_y)&&(k!=minpindex_y)) continue;
        }
        tempnodenum1 = net.pinlist[k].pinnum;
        if (tempnodenum0 == tempnodenum1) continue;
        temppinlocy1 = Nodelist[tempnodenum1].y0;
        weighty = invp/((float)fabs(temppinlocy0 - temppinlocy1) + HEPSI);
        if ((Nodelist[tempnodenum0].isterminal() == 1)&&(Nodelist[tempnodenum1].isterminal() == 1)) {
          continue;
        }
        else if ((Nodelist[tempnodenum0].isterminal() == 1)&&(Nodelist[tempnodenum1].isterminal() == 0)) {
          by[tempnodenum1] += temppinlocy0 * weighty;
          Ay[tempnodenum1][0].weight += weighty;
        }
        else if ((Nodelist[tempnodenum0].isterminal() == 0)&&(Nodelist[tempnodenum1].isterminal() == 1)) {
          by[tempnodenum0] += temppinlocy1 * weighty;
          Ay[tempnodenum0][0].weight += weighty;
        }
        else {
          //((Nodelist[tempnodenum0].isterminal() == 0)&&(Nodelist[tempnodenum1].isterminal() == 0))
          tempWT.pin = tempnodenum1;
          tempWT.weight = -weighty;
          ky[tempnodenum0]++;
          /*if (ky[tempnodenum0] == Ay[tempnodenum0].size()) {
            std::cout << tempnodenum0 << " " << ky[tempnodenum0] << " Overflowy\n";
          }*/
          Ay[tempnodenum0][ky[tempnodenum0]] = tempWT;

          tempWT.pin = tempnodenum0;
          tempWT.weight = -weighty;
          ky[tempnodenum1]++;
          /*if (ky[tempnodenum1] == Ay[tempnodenum1].size()) {
            std::cout << tempnodenum1 << " " << ky[tempnodenum1] << " Overflowy\n";
          }*/
          Ay[tempnodenum1][ky[tempnodenum1]] = tempWT;

          Ay[tempnodenum0][0].weight += weighty;
          Ay[tempnodenum1][0].weight += weighty;
        }
      }
    }
  }
}

void cg_solver::add_anchor_x() {
  float weight_x;
  Node *node;
  for (size_t i=0; i < CELL_NUM; i++) {
    node = &Nodelist[i];
    weight_x = 1 / ((float) fabs(node->x0 - node->anchorx) + WEPSI);
    Ax[i][0].weight += weight_x * ALPHA;
    bx[i] += weight_x * node->anchorx * ALPHA;
  }
}

void cg_solver::add_anchor_y() {
  float weight_y;
  Node *node;
  for (size_t i=0; i < CELL_NUM; i++) {
    node = &Nodelist[i];
    weight_y = 1 / ((float) fabs(node->y0 - node->anchory) + HEPSI);
    Ay[i][0].weight += weight_y * ALPHA;
    by[i] += weight_y * node->anchory * ALPHA;
  }
}

void cg_solver::CG_solver(std::string const &dimension, std::vector< std::vector<weight_tuple> > &A, std::vector<float> &b, std::vector<size_t> &k) {
  static float epsilon = 1e-5, alpha, beta, rsold, rsnew, pAp, solution_distance;
  static std::vector<float> ax(CELL_NUM), ap(CELL_NUM), z(CELL_NUM), p(CELL_NUM), JP(CELL_NUM);
  // JP = Jacobi_preconditioner
  for (size_t i=0; i<CELL_NUM; i++) {
    // initialize the Jacobi preconditioner
    if (A[i][0].weight > epsilon) {
      // JP[i] is the reverse of diagonal value
      JP[i] = 1/A[i][0].weight;
    }
    else {
      // if the diagonal is too small, set JP[i] to 1 to prevent diverge
      JP[i] = 1;
    }
  }
  for (size_t i=0; i<CELL_NUM; i++) {
    // calculate Ax
    ax[i] = 0;
    if (dimension=="x") {
      for (size_t j=0; j<=k[i]; j++) {
        ax[i] += A[i][j].weight * Nodelist[A[i][j].pin].x0;
      }
    }
    else {
      for (size_t j=0; j<=k[i]; j++) {
        ax[i] += A[i][j].weight * Nodelist[A[i][j].pin].y0;
      }
    }
  }
  rsold = 0;
  for (size_t i=0; i<CELL_NUM; i++) {
    // calculate z and p and rsold
    z[i] = JP[i]*(b[i] - ax[i]);
    p[i] = z[i];
    rsold += z[i]*z[i]/JP[i];
  }

  for (size_t t=0; ; t++) {
    pAp = 0;
    for (size_t i=0; i<CELL_NUM; i++) {
      ap[i] = 0;
      for (size_t j=0; j<=k[i]; j++) {
        ap[i] += A[i][j].weight * p[A[i][j].pin];
      }
      pAp += p[i]*ap[i];
    }
    alpha = rsold/pAp;
    rsnew = 0;
    if (dimension=="x") {
      for (size_t i=0; i<CELL_NUM; i++) {
        Nodelist[i].x0 += alpha * p[i];
      }
    }
    else {
      for (size_t i=0; i<CELL_NUM; i++) {
        Nodelist[i].y0 += alpha * p[i];
      }
    }
    solution_distance = 0;
    for (size_t i=0; i<CELL_NUM; i++) {
      z[i] -= JP[i]*alpha * ap[i];
      solution_distance += z[i]*z[i]/JP[i]/JP[i];
      rsnew += z[i]*z[i]/JP[i];
    }
    //std::cout << "solution_distance: " << solution_distance/CELL_NUM << "\n";
    if (solution_distance/CELL_NUM < cg_precision) break;
    beta = rsnew/rsold;
    for (size_t i=0; i<CELL_NUM; i++) {
      p[i] = z[i] + beta*p[i];
    }
    rsold = rsnew;
  }

  /* if the cell is out of boundary, just shift it back to the placement region */
  if (dimension == "x") {
    for (size_t i=0; i<CELL_NUM; i++) {
      if (Nodelist[i].llx() < LEFT) {
        Nodelist[i].x0 = (float) LEFT + Nodelist[i].width()/(float)2 + (float)1;
        //std::cout << i << "\n";
      }
      else if (Nodelist[i].urx() > RIGHT) {
        Nodelist[i].x0 = (float) RIGHT - Nodelist[i].width()/(float)2 - (float)1;
        //std::cout << i << "\n";
      }
      else {
        continue;
      }
    }
  }
  else{
    for (size_t i=0; i<CELL_NUM; i++) {
      if (Nodelist[i].lly() < BOTTOM) {
        Nodelist[i].y0 = (float) BOTTOM + Nodelist[i].height()/(float)2 + (float)1;
        //std::cout << i << "\n";
      }
      else if (Nodelist[i].ury() > TOP) {
        Nodelist[i].y0 = (float) TOP - Nodelist[i].height()/(float)2 - (float)1;
        //std::cout << i << "\n";
      }
      else {
        continue;
      }
    }
  }
}

void cg_solver::CG_solver_x() {
  CG_solver("x", Ax, bx, kx);
}

void cg_solver::CG_solver_y() {
  CG_solver("y", Ay, by, ky);
}

void cg_solver::cg_close() {
  Ax.clear();
  Ay.clear();
  bx.clear();
  by.clear();
  kx.clear();
  ky.clear();
}