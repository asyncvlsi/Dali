//
// Created by Yihang Yang on 2019-05-20.
//

#include <cmath>
#include "placeral.hpp"

placer_al_t::placer_al_t(): placer_t() {

}

placer_al_t::placer_al_t(double aspectRatio, double fillingRate): placer_t(aspectRatio, fillingRate) {

}

size_t placer_al_t::movable_block_num() {
  return _movable_block_num;
}

double placer_al_t::width_epsilon()() {
  return _width_epsilon;
}

double placer_al_t::height_epsilon()() {
  return _height_epsilon;
}

bool placer_al_t::set_input_circuit(circuit_t *circuit) {
  if (circuit->block_list.empty()) {
    std::cout << "Error!\n";
    std::cout << "Invalid input circuit: empty block list!\n";
    return false;
  }
  if (circuit->net_list.empty()) {
    std::cout << "Error!\n";
    std::cout << "Invalid input circuit: empty net list!\n";
    return false;
  }

  _circuit = circuit;
  _movable_block_num = 0;
  _width_epsilon = 0;
  _height_epsilon = 0;
  for (auto &&block: circuit->block_list) {
    block_al_t block_al;
    block_al.retrieve_info_from_database(block);
    block_list.push_back(block_al);
    //std::cout << block_dla << "\n";
    if (block_al.is_movable()) {
      ++_movable_block_num;
      _width_epsilon += block.width();
      _height_epsilon += block.height();
    }
  }
  if (_movable_block_num == 0) {
    std::cout << "Error!\n";
    std::cout << "No movable blocks in circuit!\n";
    return false;
  }
  _width_epsilon /= _movable_block_num*100;
  _height_epsilon /= _movable_block_num*100;

  for (auto &&net: circuit->net_list) {
    net_al_t net_al;
    net_al.retrieve_info_from_database(net);
    for (auto &&pin: net.pin_list) {
      int block_num = circuit->block_name_map.find(pin.get_block()->name())->second;
      block_al_t *block_al_ptr = &block_list[block_num];
      pin_t pin_al(pin.x_offset(), pin.y_offset(), block_al_ptr);
      net_al.pin_list.push_back(pin_al);
    }
    net_list.push_back(net_al);
  }
  return true;
}

void placer_al_t::initialize_HPWL_flags() {
  HPWLX_new = 0;
  HPWLY_new = 0;
  HPWLX_old = 1e30;
  HPWLY_old = 1e30;
  HPWLx_converge = false;
  HPWLy_converge = false;
}

void placer_al_t::cg_init() {
  // this init function allocate memory to Ax and Ay
  // the size of memory allocated for each row is the maximum memory which might be used
  Ax.clear(); Ay.clear(); kx.clear(); ky.clear(); bx.clear(); by.clear();
  for (size_t i=0; i<movable_block_num(); i++) {
    // initialize bx, by
    bx.push_back(0);
    by.push_back(0);
    // initialize kx, ky to track the length of Ax[i] and Ay[i]
    kx.push_back(0);
    ky.push_back(0);
  }
  std::vector<weight_tuple> tempArow;
  for (size_t i=0; i<movable_block_num(); i++) {
    Ax.push_back(tempArow);
    Ay.push_back(tempArow);
  }
  weight_tuple tempWT;
  tempWT.weight = 0;
  for (size_t i=0; i<movable_block_num(); i++) {
    tempWT.pin = i;
    Ax[i].push_back(tempWT);
    Ay[i].push_back(tempWT);
  }
  size_t tempnodenum0, tempnodenum1;
  for (auto &&net: net_list) {
    if (net.p()<=1) continue;
    for (size_t j=0; j<net.pin_list.size(); j++) {
      tempnodenum0 = net.pin_list[j].get_block()->num();
      for (size_t k=j+1; k<net.pin_list.size(); k++) {
        tempnodenum1 = net.pin_list[k].get_block()->num();
        if (tempnodenum0 == tempnodenum1) continue;
        if ((block_list[tempnodenum0].is_movable())&&(block_list[tempnodenum1].is_movable())) {
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

void placer_al_t::build_problem_clique_x() {
  // before build a new Matrix, clean the information in existing matrix
  for (size_t i=0; i<movable_block_num(); i++) {
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

  for (auto &&net: net_list) {
    //std::cout << i << "\n";
    if (net.p() <= 1) continue;
    invp = net.inv_p();
    for (size_t j=0; j<net.pin_list.size(); j++) {
      tempnodenum0 = net.pin_list[j].get_block()->num();
      temppinlocx0 = block_list[tempnodenum0].x() + net.pin_list[j].x_offset();
      for (size_t k=j+1; k<net.pin_list.size(); k++) {
        tempnodenum1 = net.pin_list[k].get_block()->num();
        temppinlocx1 = block_list[tempnodenum1].x() + net.pin_list[k].x_offset();
        if (tempnodenum0 == tempnodenum1) continue;
        if ((block_list[tempnodenum0].is_movable() == 0)&&(block_list[tempnodenum1].is_movable() == 0)) {
          continue;
        }
        weightx = invp/((float)fabs(temppinlocx0 - temppinlocx1) + width_epsilon());
        if ((block_list[tempnodenum0].is_movable() == 0)&&(block_list[tempnodenum1].is_movable() == 1)) {
          bx[tempnodenum1] += (temppinlocx0 - net.pin_list[k].x_offset()) * weightx;
          Ax[tempnodenum1][0].weight += weightx;
        }
        else if ((block_list[tempnodenum0].is_movable() == 1)&&(block_list[tempnodenum1].is_movable() == 0)) {
          bx[tempnodenum0] += (temppinlocx1 - net.pin_list[j].x_offset()) * weightx;
          Ax[tempnodenum0][0].weight += weightx;
        }
        else {
          //((block_list[tempnodenum0].isterminal() == 0)&&(block_list[tempnodenum1].isterminal() == 0))
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
          tempdiffoffset = (net.pin_list[k].x_offset() - net.pin_list[j].x_offset()) * weightx;
          bx[tempnodenum0] += tempdiffoffset;
          bx[tempnodenum1] -= tempdiffoffset;
        }
      }
    }
  }
}

void placer_al_t::build_problem_clique_y() {
  // before build a new Matrix, clean the information in existing matrix
  for (size_t i=0; i<movable_block_num(); i++) {
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

  for (auto &&net: net_list) {
    //std::cout << i << "\n";
    if (net.p() <= 1) continue;
    invp = net.inv_p();
    for (size_t j=0; j<net.pin_list.size(); j++) {
      tempnodenum0 = net.pin_list[j].get_block()->num();
      temppinlocy0 = block_list[tempnodenum0].y() + net.pin_list[j].y_offset();
      for (size_t k=j+1; k<net.pin_list.size(); k++) {
        tempnodenum1 = net.pin_list[k].get_block()->num();
        temppinlocy1 = block_list[tempnodenum1].y() + net.pin_list[k].y_offset();
        if (tempnodenum0 == tempnodenum1) continue;
        if ((block_list[tempnodenum0].is_movable() == 0)&&(block_list[tempnodenum1].is_movable() == 0)) {
          continue;
        }
        weighty = invp/((float)fabs(temppinlocy0 - temppinlocy1) + height_epsilon());
        if ((block_list[tempnodenum0].is_movable() == 0)&&(block_list[tempnodenum1].is_movable() == 1)) {
          by[tempnodenum1] += (temppinlocy0 - net.pin_list[k].y_offset()) * weighty;
          Ay[tempnodenum1][0].weight += weighty;
        }
        else if ((block_list[tempnodenum0].is_movable() == 1)&&(block_list[tempnodenum1].is_movable() == 0)) {
          by[tempnodenum0] += (temppinlocy1 - net.pin_list[j].y_offset()) * weighty;
          Ay[tempnodenum0][0].weight += weighty;
        }
        else {
          //((block_list[tempnodenum0].isterminal() == 0)&&(block_list[tempnodenum1].isterminal() == 0))
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
          tempdiffoffset = (net.pin_list[k].y_offset() - net.pin_list[j].y_offset()) * weighty;
          by[tempnodenum0] += tempdiffoffset;
          by[tempnodenum1] -= tempdiffoffset;
        }
      }
    }
  }
}

void placer_al_t::update_HPWL_x() {
  // update the y direction max and min node in each net
  float max_pin_loc_x, min_pin_loc_x;
  size_t maxpindex_x, minpindex_x;
  HPWLX_new = 0;
  for (auto &&net: net_list) {
    maxpindex_x = 0;
    max_pin_loc_x = block_list[net.pin_list[0].get_block()->num()].x() + net.pin_list[0].x_offset();
    minpindex_x = 0;
    min_pin_loc_x = block_list[net.pin_list[0].get_block()->num()].x() + net.pin_list[0].x_offset();
    for (size_t i=0; i<net.pin_list.size(); i++) {
      net.pin_list[i].x = block_list[net.pin_list[i].get_block()->num()].x() + net.pin_list[i].x_offset();
      //std::cout << net.pin_list[i].x << " ";
      if (net.pin_list[i].x > max_pin_loc_x) {
        maxpindex_x = i;
        max_pin_loc_x = net.pin_list[i].x;
      }
      else if (net.pin_list[i].x < min_pin_loc_x) {
        minpindex_x = i;
        min_pin_loc_x = net.pin_list[i].x;
      }
      else {
        ;
      }
    }
    net.maxpindex_x = maxpindex_x;
    net.minpindex_x = minpindex_x;
    net.hpwlx = max_pin_loc_x - min_pin_loc_x;
    HPWLX_new += net.hpwlx;
    //std::cout << "Max: " << block_list[net.pin_list[net.maxpindex_x]..get_block()->num()].x() + net.pin_list[net.maxpindex_x].x_offset() << " ";
    //std::cout << "Min: " << block_list[net.pin_list[net.minpindex_x]..get_block()->num()].x() + net.pin_list[net.minpindex_x].x_offset() << "\n";
  }
}

void placer_al_t::update_max_min_node_x() {
  // update the y direction max and min node in each net
  float max_pin_loc_x, min_pin_loc_x;
  size_t maxpindex_x, minpindex_x;
  HPWLX_new = 0;
  for (auto &&net: net_list) {
    maxpindex_x = 0;
    max_pin_loc_x = block_list[net.pin_list[0]..get_block()->num()].x() + net.pin_list[0].x_offset();
    minpindex_x = 0;
    min_pin_loc_x = block_list[net.pin_list[0]..get_block()->num()].x() + net.pin_list[0].x_offset();
    for (size_t i=0; i<net.pin_list.size(); i++) {
      net.pin_list[i].x = block_list[net.pin_list[i]..get_block()->num()].x() + net.pin_list[i].x_offset();
      //std::cout << net.pin_list[i].x << " ";
      if (net.pin_list[i].x > max_pin_loc_x) {
        maxpindex_x = i;
        max_pin_loc_x = net.pin_list[i].x;
      }
      else if (net.pin_list[i].x < min_pin_loc_x) {
        minpindex_x = i;
        min_pin_loc_x = net.pin_list[i].x;
      }
      else {
        ;
      }
    }
    net.maxpindex_x = maxpindex_x;
    net.minpindex_x = minpindex_x;
    net.hpwlx = max_pin_loc_x - min_pin_loc_x;
    HPWLX_new += net.hpwlx;
    //std::cout << "Max: " << block_list[net.pin_list[net.maxpindex_x]..get_block()->num()].x() + net.pin_list[net.maxpindex_x].x_offset() << " ";
    //std::cout << "Min: " << block_list[net.pin_list[net.minpindex_x]..get_block()->num()].x() + net.pin_list[net.minpindex_x].x_offset() << "\n";
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

void placer_al_t::build_problem_b2b_x() {
  // before build a new Matrix, clean the information in existing matrix
  for (size_t i=0; i<movable_block_num(); i++) {
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
  for (auto &&net: net_list) {
    if (net.p<=1) continue;
    invp = net.invpmin1;
    maxpindex_x = net.maxpindex_x;
    minpindex_x = net.minpindex_x;
    for (size_t i=0; i<net.pin_list.size(); i++) {
      tempnodenum0 = net.pin_list[i]..get_block()->num();
      temppinlocx0 = net.pin_list[i].x;
      for (size_t k=i+1; k<net.pin_list.size(); k++) {
        if ((i!=maxpindex_x)&&(i!=minpindex_x)) {
          if ((k!=maxpindex_x)&&(k!=minpindex_x)) continue;
        }
        tempnodenum1 = net.pin_list[k]..get_block()->num();
        if (tempnodenum0 == tempnodenum1) continue;
        temppinlocx1 = net.pin_list[k].x;
        weightx = invp/((float)fabs(temppinlocx0 - temppinlocx1) + width_epsilon());
        if ((block_list[tempnodenum0].isterminal() == 1)&&(block_list[tempnodenum1].isterminal() == 1)) {
          continue;
        }
        else if ((block_list[tempnodenum0].isterminal() == 1)&&(block_list[tempnodenum1].isterminal() == 0)) {
          bx[tempnodenum1] += (temppinlocx0 - net.pin_list[k].x_offset()) * weightx;
          Ax[tempnodenum1][0].weight += weightx;
        }
        else if ((block_list[tempnodenum0].isterminal() == 0)&&(block_list[tempnodenum1].isterminal() == 1)) {
          bx[tempnodenum0] += (temppinlocx1 - net.pin_list[i].x_offset()) * weightx;
          Ax[tempnodenum0][0].weight += weightx;
        }
        else {
          //((block_list[tempnodenum0].isterminal() == 0)&&(block_list[tempnodenum1].isterminal() == 0))
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
          tempdiffoffset = (net.pin_list[k].x_offset() - net.pin_list[i].x_offset()) * weightx;
          bx[tempnodenum0] += tempdiffoffset;
          bx[tempnodenum1] -= tempdiffoffset;
        }
      }
    }
  }
}

void placer_al_t::build_problem_b2b_x_nooffset() {
  // before build a new Matrix, clean the information in existing matrix
  for (size_t i=0; i<movable_block_num(); i++) {
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

  for (auto &&net: net_list) {
    if (net.p<=1) continue;
    invp = net.invpmin1;
    maxpindex_x = net.maxpindex_x;
    minpindex_x = net.minpindex_x;
    for (size_t i=0; i<net.pin_list.size(); i++) {
      tempnodenum0 = net.pin_list[i]..get_block()->num();
      temppinlocx0 = block_list[tempnodenum0].x();
      for (size_t k=i+1; k<net.pin_list.size(); k++) {
        if ((i!=maxpindex_x)&&(i!=minpindex_x)) {
          if ((k!=maxpindex_x)&&(k!=minpindex_x)) continue;
        }
        tempnodenum1 = net.pin_list[k]..get_block()->num();
        if (tempnodenum0 == tempnodenum1) continue;
        temppinlocx1 = block_list[tempnodenum1].x();
        weightx = invp/((float)fabs(temppinlocx0 - temppinlocx1) + width_epsilon());
        if ((block_list[tempnodenum0].isterminal() == 1)&&(block_list[tempnodenum1].isterminal() == 1)) {
          continue;
        }
        else if ((block_list[tempnodenum0].isterminal() == 1)&&(block_list[tempnodenum1].isterminal() == 0)) {
          bx[tempnodenum1] += temppinlocx0 * weightx;
          Ax[tempnodenum1][0].weight += weightx;
        }
        else if ((block_list[tempnodenum0].isterminal() == 0)&&(block_list[tempnodenum1].isterminal() == 1)) {
          bx[tempnodenum0] += temppinlocx1 * weightx;
          Ax[tempnodenum0][0].weight += weightx;
        }
        else {
          //((block_list[tempnodenum0].isterminal() == 0)&&(block_list[tempnodenum1].isterminal() == 0))
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

void placer_al_t::update_HPWL_y() {
  // update the y direction max and min node in each net
  float maxpinlocy, minpinlocy;
  size_t maxpindex_y, minpindex_y;
  HPWLY_new = 0;
  for (auto &&net: net_list) {
    maxpindex_y = 0;
    maxpinlocy = block_list[net.pin_list[0]..get_block()->num()].y() + net.pin_list[0].y_offset();
    minpindex_y = 0;
    minpinlocy = block_list[net.pin_list[0]..get_block()->num()].y() + net.pin_list[0].y_offset();
    for (size_t i=0; i<net.pin_list.size(); i++) {
      net.pin_list[i].y = block_list[net.pin_list[i]..get_block()->num()].y() + net.pin_list[i].y_offset();
      //std::cout << net.pin_list[i].y << " ";
      if (net.pin_list[i].y > maxpinlocy) {
        maxpindex_y = i;
        maxpinlocy = net.pin_list[i].y;
      }
      else if (net.pin_list[i].y < minpinlocy) {
        minpindex_y = i;
        minpinlocy = net.pin_list[i].y;
      }
      else {
        ;
      }
    }
    net.maxpindex_y = maxpindex_y;
    net.minpindex_y = minpindex_y;
    net.hpwly = maxpinlocy - minpinlocy;
    HPWLY_new += net.hpwly;
    //std::cout << "Max: " << block_list[net.pin_list[net.maxpindex_y]..get_block()->num()].y() + net.pin_list[net.maxpindex_y].y_offset() << " ";
    //std::cout << "Min: " << block_list[net.pin_list[net.minpindex_y]..get_block()->num()].y() + net.pin_list[net.minpindex_y].y_offset() << "\n";
  }
}

void placer_al_t::update_max_min_node_y() {
  // update the y direction max and min node in each net
  float maxpinlocy, minpinlocy;
  size_t maxpindex_y, minpindex_y;
  HPWLY_new = 0;
  for (auto &&net: net_list) {
    maxpindex_y = 0;
    maxpinlocy = block_list[net.pin_list[0]..get_block()->num()].y() + net.pin_list[0].y_offset();
    minpindex_y = 0;
    minpinlocy = block_list[net.pin_list[0]..get_block()->num()].y() + net.pin_list[0].y_offset();
    for (size_t i=0; i<net.pin_list.size(); i++) {
      net.pin_list[i].y = block_list[net.pin_list[i]..get_block()->num()].y() + net.pin_list[i].y_offset();
      //std::cout << net.pin_list[i].y << " ";
      if (net.pin_list[i].y > maxpinlocy) {
        maxpindex_y = i;
        maxpinlocy = net.pin_list[i].y;
      }
      else if (net.pin_list[i].y < minpinlocy) {
        minpindex_y = i;
        minpinlocy = net.pin_list[i].y;
      }
      else {
        ;
      }
    }
    net.maxpindex_y = maxpindex_y;
    net.minpindex_y = minpindex_y;
    net.hpwly = maxpinlocy - minpinlocy;
    HPWLY_new += net.hpwly;
    //std::cout << "Max: " << block_list[net.pin_list[net.maxpindex_y]..get_block()->num()].y() + net.pin_list[net.maxpindex_y].y_offset() << " ";
    //std::cout << "Min: " << block_list[net.pin_list[net.minpindex_y]..get_block()->num()].y() + net.pin_list[net.minpindex_y].y_offset() << "\n";
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

void placer_al_t::build_problem_b2b_y() {
  // before build a new Matrix, clean the information in existing matrix
  for (size_t i=0; i<movable_block_num(); i++) {
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
  for (auto &&net: net_list) {
    if (net.p<=1) continue;
    invp = net.invpmin1;
    maxpindex_y = net.maxpindex_y;
    minpindex_y = net.minpindex_y;
    for (size_t i=0; i<net.pin_list.size(); i++) {
      tempnodenum0 = net.pin_list[i]..get_block()->num();
      temppinlocy0 = net.pin_list[i].y;
      for (size_t k=i+1; k<net.pin_list.size(); k++) {
        if ((i!=maxpindex_y)&&(i!=minpindex_y)) {
          if ((k!=maxpindex_y)&&(k!=minpindex_y)) continue;
        }
        tempnodenum1 = net.pin_list[k]..get_block()->num();
        if (tempnodenum0 == tempnodenum1) continue;
        temppinlocy1 = net.pin_list[k].y;
        weighty = invp/((float)fabs(temppinlocy0 - temppinlocy1) + height_epsilon());
        if ((block_list[tempnodenum0].isterminal() == 1)&&(block_list[tempnodenum1].isterminal() == 1)) {
          continue;
        }
        else if ((block_list[tempnodenum0].isterminal() == 1)&&(block_list[tempnodenum1].isterminal() == 0)) {
          by[tempnodenum1] += (temppinlocy0 - net.pin_list[k].y_offset()) * weighty;
          Ay[tempnodenum1][0].weight += weighty;
        }
        else if ((block_list[tempnodenum0].isterminal() == 0)&&(block_list[tempnodenum1].isterminal() == 1)) {
          by[tempnodenum0] += (temppinlocy1 - net.pin_list[i].y_offset()) * weighty;
          Ay[tempnodenum0][0].weight += weighty;
        }
        else {
          //((block_list[tempnodenum0].isterminal() == 0)&&(block_list[tempnodenum1].isterminal() == 0))
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
          tempdiffoffset = (net.pin_list[k].y_offset() - net.pin_list[i].y_offset()) * weighty;
          by[tempnodenum0] += tempdiffoffset;
          by[tempnodenum1] -= tempdiffoffset;
        }
      }
    }
  }
}

void placer_al_t::build_problem_b2b_y_nooffset() {
  // before build a new Matrix, clean the information in existing matrix
  for (size_t i=0; i<movable_block_num(); i++) {
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
  for (auto &&net: net_list) {
    if (net.p<=1) continue;
    invp = net.invpmin1;
    maxpindex_y = net.maxpindex_y;
    minpindex_y = net.minpindex_y;
    for (size_t i=0; i<net.pin_list.size(); i++) {
      tempnodenum0 = net.pin_list[i]..get_block()->num();
      temppinlocy0 = block_list[tempnodenum0].y();
      for (size_t k=i+1; k<net.pin_list.size(); k++) {
        if ((i!=maxpindex_y)&&(i!=minpindex_y)) {
          if ((k!=maxpindex_y)&&(k!=minpindex_y)) continue;
        }
        tempnodenum1 = net.pin_list[k]..get_block()->num();
        if (tempnodenum0 == tempnodenum1) continue;
        temppinlocy1 = block_list[tempnodenum1].y();
        weighty = invp/((float)fabs(temppinlocy0 - temppinlocy1) + height_epsilon());
        if ((block_list[tempnodenum0].isterminal() == 1)&&(block_list[tempnodenum1].isterminal() == 1)) {
          continue;
        }
        else if ((block_list[tempnodenum0].isterminal() == 1)&&(block_list[tempnodenum1].isterminal() == 0)) {
          by[tempnodenum1] += temppinlocy0 * weighty;
          Ay[tempnodenum1][0].weight += weighty;
        }
        else if ((block_list[tempnodenum0].isterminal() == 0)&&(block_list[tempnodenum1].isterminal() == 1)) {
          by[tempnodenum0] += temppinlocy1 * weighty;
          Ay[tempnodenum0][0].weight += weighty;
        }
        else {
          //((block_list[tempnodenum0].isterminal() == 0)&&(block_list[tempnodenum1].isterminal() == 0))
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

void placer_al_t::CG_solver(std::string const &dimension, std::vector< std::vector<weight_tuple> > &A, std::vector<float> &b, std::vector<size_t> &k) {
  static float epsilon = 1e-5, alpha, beta, rsold, rsnew, pAp, solution_distance;
  static std::vector<float> ax(movable_block_num()), ap(movable_block_num()), z(movable_block_num()), p(movable_block_num()), JP(movable_block_num());
  // JP = Jacobi_preconditioner
  for (size_t i=0; i<movable_block_num(); i++) {
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
  for (size_t i=0; i<movable_block_num(); i++) {
    // calculate Ax
    ax[i] = 0;
    if (dimension=="x") {
      for (size_t j=0; j<=k[i]; j++) {
        ax[i] += A[i][j].weight * block_list[A[i][j].pin].x();
      }
    }
    else {
      for (size_t j=0; j<=k[i]; j++) {
        ax[i] += A[i][j].weight * block_list[A[i][j].pin].y();
      }
    }
  }
  rsold = 0;
  for (size_t i=0; i<movable_block_num(); i++) {
    // calculate z and p and rsold
    z[i] = JP[i]*(b[i] - ax[i]);
    p[i] = z[i];
    rsold += z[i]*z[i]/JP[i];
  }

  for (size_t t=0; ; t++) {
    pAp = 0;
    for (size_t i=0; i<movable_block_num(); i++) {
      ap[i] = 0;
      for (size_t j=0; j<=k[i]; j++) {
        ap[i] += A[i][j].weight * p[A[i][j].pin];
      }
      pAp += p[i]*ap[i];
    }
    alpha = rsold/pAp;
    rsnew = 0;
    if (dimension=="x") {
      for (size_t i=0; i<movable_block_num(); i++) {
        block_list[i].x() += alpha * p[i];
      }
    }
    else {
      for (size_t i=0; i<movable_block_num(); i++) {
        block_list[i].y() += alpha * p[i];
      }
    }
    solution_distance = 0;
    for (size_t i=0; i<movable_block_num(); i++) {
      z[i] -= JP[i]*alpha * ap[i];
      solution_distance += z[i]*z[i]/JP[i]/JP[i];
      rsnew += z[i]*z[i]/JP[i];
    }
    //std::cout << "solution_distance: " << solution_distance/movable_block_num() << "\n";
    if (solution_distance/movable_block_num() < cg_precision) break;
    beta = rsnew/rsold;
    for (size_t i=0; i<movable_block_num(); i++) {
      p[i] = z[i] + beta*p[i];
    }
    rsold = rsnew;
  }

  /* if the cell is out of boundary, just shift it back to the placement region */
  if (dimension == "x") {
    for (size_t i=0; i<movable_block_num(); i++) {
      if (block_list[i].llx() < left()) {
        block_list[i].x() = (float) left() + block_list[i].width()/(float)2 + (float)1;
        //std::cout << i << "\n";
      }
      else if (block_list[i].urx() > right()) {
        block_list[i].x() = (float) right() - block_list[i].width()/(float)2 - (float)1;
        //std::cout << i << "\n";
      }
      else {
        continue;
      }
    }
  }
  else{
    for (size_t i=0; i<movable_block_num(); i++) {
      if (block_list[i].lly() < bottom()) {
        block_list[i].y() = (float) bottom() + block_list[i].height()/(float)2 + (float)1;
        //std::cout << i << "\n";
      }
      else if (block_list[i].ury() > top()) {
        block_list[i].y() = (float) top() - block_list[i].height()/(float)2 - (float)1;
        //std::cout << i << "\n";
      }
      else {
        continue;
      }
    }
  }
}

void placer_al_t::CG_solver_x() {
  CG_solver("x", Ax, bx, kx);
}

void placer_al_t::CG_solver_y() {
  CG_solver("y", Ay, by, ky);
}

void placer_al_t::cg_close() {
  Ax.clear();
  Ay.clear();
  bx.clear();
  by.clear();
  kx.clear();
  ky.clear();
