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

double placer_al_t::width_epsilon() {
  return _width_epsilon;
}

double placer_al_t::height_epsilon() {
  return _height_epsilon;
}

bool placer_al_t::set_input_circuit(circuit_t *circuit) {
  block_list.clear();
  net_list.clear();
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

void placer_al_t::uniform_initialization() {
  int Nx = right() - left();
  int Ny = top() - bottom();

  std::default_random_engine generator{0};
  std::uniform_real_distribution<float> distribution(0, 1);
  for (auto &&node: block_list) {
    if (!node.is_movable()) continue;
    node.set_center_dx(left() + Nx*distribution(generator));
    node.set_center_dy(bottom() + Ny*distribution(generator));
    // uniform distribution around the center of placement region
  }
}

void placer_al_t::cg_init() {
  // this init function allocate memory to Ax and Ay
  // the size of memory allocated for each row is the maximum memory which might be used
  Ax.clear();
  Ay.clear();
  kx.clear();
  ky.clear();
  bx.clear();
  by.clear();
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
  double weightx, invp, temppinlocx0, temppinlocx1, tempdiffoffset;
  size_t tempnodenum0, tempnodenum1;

  for (auto &&net: net_list) {
    //std::cout << i << "\n";
    if (net.p() <= 1) continue;
    invp = net.inv_p();
    for (size_t j=0; j<net.pin_list.size(); j++) {
      tempnodenum0 = net.pin_list[j].get_block()->num();
      temppinlocx0 = block_list[tempnodenum0].dllx() + net.pin_list[j].x_offset();
      for (size_t k=j+1; k<net.pin_list.size(); k++) {
        tempnodenum1 = net.pin_list[k].get_block()->num();
        temppinlocx1 = block_list[tempnodenum1].dllx() + net.pin_list[k].x_offset();
        if (tempnodenum0 == tempnodenum1) continue;
        if ((block_list[tempnodenum0].is_movable() == 0)&&(block_list[tempnodenum1].is_movable() == 0)) {
          continue;
        }
        weightx = invp/(fabs(temppinlocx0 - temppinlocx1) + width_epsilon());
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
  double weighty, invp, temppinlocy0, temppinlocy1, tempdiffoffset;
  size_t tempnodenum0, tempnodenum1;

  for (auto &&net: net_list) {
    //std::cout << i << "\n";
    if (net.p() <= 1) continue;
    invp = net.inv_p();
    for (size_t j=0; j<net.pin_list.size(); j++) {
      tempnodenum0 = net.pin_list[j].get_block()->num();
      temppinlocy0 = block_list[tempnodenum0].dlly() + net.pin_list[j].y_offset();
      for (size_t k=j+1; k<net.pin_list.size(); k++) {
        tempnodenum1 = net.pin_list[k].get_block()->num();
        temppinlocy1 = block_list[tempnodenum1].dlly() + net.pin_list[k].y_offset();
        if (tempnodenum0 == tempnodenum1) continue;
        if ((block_list[tempnodenum0].is_movable() == 0)&&(block_list[tempnodenum1].is_movable() == 0)) {
          continue;
        }
        weighty = invp/((double)fabs(temppinlocy0 - temppinlocy1) + height_epsilon());
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
  HPWLX_new = 0;
  for (auto &&net: net_list) {
    HPWLX_new += net.dhpwlx();
  }
}

void placer_al_t::update_max_min_node_x() {
  HPWLX_new = 0;
  for (auto &&net: net_list) {
    HPWLX_new += net.dhpwlx();
  }
  //std::cout << "HPWLX_old: " << HPWLX_old << "\n";
  //std::cout << "HPWLX_new: " << HPWLX_new << "\n";
  //std::cout << 1 - HPWLX_new/HPWLX_old << "\n";
  if (HPWLX_new == 0) { // this is for 1 degree net, this happens in extremely rare cases
    HPWLx_converge = true;
  } else {
    HPWLx_converge = (std::fabs(1 - HPWLX_new / HPWLX_old) < HPWL_intra_linearSolver_precision);
    HPWLX_old = HPWLX_new;
  }
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
  double weight_x, inv_p, temp_pin_loc_x0, temp_pin_loc_x1, temp_diff_offset;
  size_t temp_node_num0, temp_node_num1, max_pindex_x, min_pindex_x;
  for (auto &&net: net_list) {
    if (net.p()<=1) {
      continue;
    }
    inv_p = net.inv_p();
    max_pindex_x = net.max_pin_index_x();
    min_pindex_x = net.min_pin_index_x();
    for (size_t i=0; i<net.pin_list.size(); i++) {
      temp_node_num0 = net.pin_list[i].get_block()->num();
      temp_pin_loc_x0 = block_list[temp_node_num0].dllx() + net.pin_list[i].x_offset();
      for (size_t k=i+1; k<net.pin_list.size(); k++) {
        if ((i!=max_pindex_x)&&(i!=min_pindex_x)) {
          if ((k!=max_pindex_x)&&(k!=min_pindex_x)) continue;
        }
        temp_node_num1 = net.pin_list[k].get_block()->num();
        if (temp_node_num0 == temp_node_num1) continue;
        temp_pin_loc_x1 = block_list[temp_node_num1].dllx() + net.pin_list[k].x_offset();
        weight_x = inv_p/(std::fabs(temp_pin_loc_x0 - temp_pin_loc_x1) + width_epsilon());
        if ((block_list[temp_node_num0].is_movable() == 0)&&(block_list[temp_node_num1].is_movable() == 0)) {
          continue;
        }
        else if ((block_list[temp_node_num0].is_movable() == 0)&&(block_list[temp_node_num1].is_movable() == 1)) {
          bx[temp_node_num1] += (temp_pin_loc_x0 - net.pin_list[k].x_offset()) * weight_x;
          Ax[temp_node_num1][0].weight += weight_x;
        }
        else if ((block_list[temp_node_num0].is_movable() == 1)&&(block_list[temp_node_num1].is_movable() == 0)) {
          bx[temp_node_num0] += (temp_pin_loc_x1 - net.pin_list[i].x_offset()) * weight_x;
          Ax[temp_node_num0][0].weight += weight_x;
        }
        else {
          //((block_list[temp_node_num0].is_movable())&&(block_list[temp_node_num1].is_movable()))
          tempWT.pin = temp_node_num1;
          tempWT.weight = -weight_x;
          kx[temp_node_num0]++;
          Ax[temp_node_num0][kx[temp_node_num0]] = tempWT;

          tempWT.pin = temp_node_num0;
          tempWT.weight = -weight_x;
          kx[temp_node_num1]++;
          Ax[temp_node_num1][kx[temp_node_num1]] = tempWT;

          Ax[temp_node_num0][0].weight += weight_x;
          Ax[temp_node_num1][0].weight += weight_x;
          temp_diff_offset = (net.pin_list[k].x_offset() - net.pin_list[i].x_offset()) * weight_x;
          bx[temp_node_num0] += temp_diff_offset;
          bx[temp_node_num1] -= temp_diff_offset;
        }
      }
    }
  }
  for (size_t i=0; i<Ax.size(); i++) { // this is for cells with tiny force applied on them
    if (Ax[i][0].weight < 1e-10) {
      Ax[i][0].weight = 1;
      bx[i] = block_list[i].dllx();
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
  double weightx, invp, temppinlocx0, temppinlocx1;
  size_t tempnodenum0, tempnodenum1, maxpindex_x, minpindex_x;

  for (auto &&net: net_list) {
    if (net.p()<=1) continue;
    invp = net.inv_p();
    maxpindex_x = net.max_pin_index_x();
    minpindex_x = net.min_pin_index_x();
    for (size_t i=0; i<net.pin_list.size(); i++) {
      tempnodenum0 = net.pin_list[i].get_block()->num();
      temppinlocx0 = block_list[tempnodenum0].dllx();
      for (size_t k=i+1; k<net.pin_list.size(); k++) {
        if ((i!=maxpindex_x)&&(i!=minpindex_x)) {
          if ((k!=maxpindex_x)&&(k!=minpindex_x)) continue;
        }
        tempnodenum1 = net.pin_list[k].get_block()->num();
        if (tempnodenum0 == tempnodenum1) continue;
        temppinlocx1 = block_list[tempnodenum1].dllx();
        weightx = invp/((double)fabs(temppinlocx0 - temppinlocx1) + width_epsilon());
        if ((block_list[tempnodenum0].is_movable() == 0)&&(block_list[tempnodenum1].is_movable() == 0)) {
          continue;
        }
        else if ((block_list[tempnodenum0].is_movable() == 0)&&(block_list[tempnodenum1].is_movable() == 1)) {
          bx[tempnodenum1] += temppinlocx0 * weightx;
          Ax[tempnodenum1][0].weight += weightx;
        }
        else if ((block_list[tempnodenum0].is_movable() == 1)&&(block_list[tempnodenum1].is_movable() == 0)) {
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
  HPWLY_new = 0;
  for (auto &&net: net_list) {
    HPWLY_new += net.dhpwly();
  }
}

void placer_al_t::update_max_min_node_y() {
  // update the y direction max and min node in each net
  HPWLY_new = 0;
  for (auto &&net: net_list) {
    HPWLY_new += net.dhpwly();
  }
  //std::cout << "HPWLY_old: " << HPWLY_old << "\n";
  //std::cout << "HPWLY_new: " << HPWLY_new << "\n";
  //std::cout << 1 - HPWLY_new/HPWLY_old << "\n";
  if (HPWLY_new == 0) { // this is for 1 degree net, this happens in extremely rare cases
    HPWLy_converge = true;
  } else {
    HPWLy_converge = (std::fabs(1 - HPWLY_new / HPWLY_old) < HPWL_intra_linearSolver_precision);
    HPWLY_old = HPWLY_new;
  }
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
  double weighty, inv_p, temp_pin_loc_y0, temp_pin_loc_y1, temp_diff_offset;
  size_t temp_node_num0, temp_node_num1, max_pindex_y, min_pindex_y;
  for (auto &&net: net_list) {
    if (net.p()<=1) {
      continue;
    }
    inv_p = net.inv_p();
    max_pindex_y = net.max_pin_index_y();
    min_pindex_y = net.min_pin_index_y();
    for (size_t i=0; i<net.pin_list.size(); i++) {
      temp_node_num0 = net.pin_list[i].get_block()->num();
      temp_pin_loc_y0 = block_list[temp_node_num0].dlly() + net.pin_list[i].y_offset();
      for (size_t k=i+1; k<net.pin_list.size(); k++) {
        if ((i!=max_pindex_y)&&(i!=min_pindex_y)) {
          if ((k!=max_pindex_y)&&(k!=min_pindex_y)) continue;
        }
        temp_node_num1 = net.pin_list[k].get_block()->num();
        if (temp_node_num0 == temp_node_num1) continue;
        temp_pin_loc_y1 = block_list[temp_node_num1].dlly() + net.pin_list[i].y_offset();
        weighty = inv_p/((double)fabs(temp_pin_loc_y0 - temp_pin_loc_y1) + height_epsilon());
        if ((block_list[temp_node_num0].is_movable() == 0)&&(block_list[temp_node_num1].is_movable() == 0)) {
          continue;
        }
        else if ((block_list[temp_node_num0].is_movable() == 0)&&(block_list[temp_node_num1].is_movable() == 1)) {
          by[temp_node_num1] += (temp_pin_loc_y0 - net.pin_list[k].y_offset()) * weighty;
          Ay[temp_node_num1][0].weight += weighty;
        }
        else if ((block_list[temp_node_num0].is_movable() == 1)&&(block_list[temp_node_num1].is_movable() == 0)) {
          by[temp_node_num0] += (temp_pin_loc_y1 - net.pin_list[i].y_offset()) * weighty;
          Ay[temp_node_num0][0].weight += weighty;
        }
        else {
          //((block_list[temp_node_num0].is_movable())&&(block_list[temp_node_num1].is_movable()))
          tempWT.pin = temp_node_num1;
          tempWT.weight = -weighty;
          ky[temp_node_num0]++;
          Ay[temp_node_num0][ky[temp_node_num0]] = tempWT;

          tempWT.pin = temp_node_num0;
          tempWT.weight = -weighty;
          ky[temp_node_num1]++;
          Ay[temp_node_num1][ky[temp_node_num1]] = tempWT;

          Ay[temp_node_num0][0].weight += weighty;
          Ay[temp_node_num1][0].weight += weighty;
          temp_diff_offset = (net.pin_list[k].y_offset() - net.pin_list[i].y_offset()) * weighty;
          by[temp_node_num0] += temp_diff_offset;
          by[temp_node_num1] -= temp_diff_offset;
        }
      }
    }
  }
  for (size_t i=0; i<Ay.size(); i++) { // // this is for cells with tiny force applied on them
    if (Ay[i][0].weight < 1e-10) {
      Ay[i][0].weight = 1;
      by[i] = block_list[i].dlly();
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
  double weighty, invp, temppinlocy0, temppinlocy1;
  size_t tempnodenum0, tempnodenum1, maxpindex_y, minpindex_y;
  for (auto &&net: net_list) {
    if (net.p()<=1) continue;
    invp = net.inv_p();
    maxpindex_y = net.max_pin_index_y();
    minpindex_y = net.min_pin_index_y();
    for (size_t i=0; i<net.pin_list.size(); i++) {
      tempnodenum0 = net.pin_list[i].get_block()->num();
      temppinlocy0 = block_list[tempnodenum0].dlly();
      for (size_t k=i+1; k<net.pin_list.size(); k++) {
        if ((i!=maxpindex_y)&&(i!=minpindex_y)) {
          if ((k!=maxpindex_y)&&(k!=minpindex_y)) continue;
        }
        tempnodenum1 = net.pin_list[k].get_block()->num();
        if (tempnodenum0 == tempnodenum1) continue;
        temppinlocy1 = block_list[tempnodenum1].dlly();
        weighty = invp/((double)fabs(temppinlocy0 - temppinlocy1) + height_epsilon());
        if ((block_list[tempnodenum0].is_movable() == 0)&&(block_list[tempnodenum1].is_movable() == 0)) {
          continue;
        }
        else if ((block_list[tempnodenum0].is_movable() == 0)&&(block_list[tempnodenum1].is_movable() == 1)) {
          by[tempnodenum1] += temppinlocy0 * weighty;
          Ay[tempnodenum1][0].weight += weighty;
        }
        else if ((block_list[tempnodenum0].is_movable() == 1)&&(block_list[tempnodenum1].is_movable() == 0)) {
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

void placer_al_t::CG_solver(std::string const &dimension, std::vector< std::vector<weight_tuple> > &A, std::vector<double> &b, std::vector<size_t> &k) {
  static double epsilon = 1e-15, alpha, beta, rsold, rsnew, pAp, solution_distance;
  static std::vector<double> ax(movable_block_num()), ap(movable_block_num()), z(movable_block_num()), p(movable_block_num()), JP(movable_block_num());
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
        ax[i] += A[i][j].weight * block_list[A[i][j].pin].dx();
      }
    }
    else {
      for (size_t j=0; j<=k[i]; j++) {
        ax[i] += A[i][j].weight * block_list[A[i][j].pin].dy();
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

  for (size_t t=0; t<100; t++) {
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
        block_list[i].x_increment(alpha * p[i]);
      }
    }
    else {
      for (size_t i=0; i<movable_block_num(); i++) {
        block_list[i].y_increment(alpha * p[i]);
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
      if (block_list[i].dllx() < left()) {
        block_list[i].set_center_dx(left() + block_list[i].width()/2.0 + 1);
        //std::cout << i << "\n";
      }
      else if (block_list[i].urx() > right()) {
        block_list[i].set_center_dx(right() - block_list[i].width()/2.0 - 1);
        //std::cout << i << "\n";
      }
      else {
        continue;
      }
    }
  }
  else{
    for (size_t i=0; i<movable_block_num(); i++) {
      if (block_list[i].dlly() < bottom()) {
        block_list[i].set_center_dy(bottom() + block_list[i].height()/2.0 + 1);
        //std::cout << i << "\n";
      }
      else if (block_list[i].ury() > top()) {
        block_list[i].set_center_dy(top() - block_list[i].height()/2.0 - 1);
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
}

void placer_al_t::add_boundary_list() {
  boundary_list.clear();
  int max_width=0, max_height=0;
  for (auto &&block: block_list) {
    // find maximum width and maximum height
    if (!block.is_movable()) {
      continue;
    }
    if (block.width() > max_width) {
      max_width = block.width();
    }
    if (block.height() > max_height) {
      max_height = block.height();
    }
  }

  block_al_t tmp_block;
  int multiplier = 500;
  tmp_block.set_movable(false);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_dllx(left() - tmp_block.width());
  tmp_block.set_dlly(top());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(right() - left());
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_dllx(left());
  tmp_block.set_dlly(top());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_dllx(right());
  tmp_block.set_dlly(top());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_width(top() - bottom());
  tmp_block.set_dllx(right());
  tmp_block.set_dlly(bottom());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_dllx(right());
  tmp_block.set_dlly(bottom() - tmp_block.height());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(right() - left());
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_dllx(left());
  tmp_block.set_dlly(bottom() - tmp_block.height());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_dllx(left() - tmp_block.width());
  tmp_block.set_dlly(bottom() - tmp_block.height());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(top() - bottom());
  tmp_block.set_dllx(left() - tmp_block.width());
  tmp_block.set_dlly(bottom());
  boundary_list.push_back(tmp_block);
}

void placer_al_t::initialize_bin_list(){
  // initialize the bin_list, build up the bin_list matrix
  bin_list.clear();
  int max_width=0, max_height=0;
  for (auto &&block: block_list) {
    // find maximum width and maximum height
    if (!block.is_movable()) {
      continue;
    }
    if (block.width() > max_width) {
      max_width = block.width();
    }
    if (block.height() > max_height) {
      max_height = block.height();
    }
  }
  size_t x_bin_num, y_bin_num;
  x_bin_num = (size_t)std::ceil((right() - left() + 2*max_width)/(double)max_width); // determine the bin numbers in x direction
  y_bin_num = (size_t)std::ceil((top() - bottom() + 2*max_height)/(double)max_height); // determine the bin numbers in y direction
  bin_width = max_width;
  bin_height = max_height;
  std::vector<bin_t> tmp_bin_column(y_bin_num);
  for (size_t x=0; x<x_bin_num; x++) {
    // bin_list[x][y] indicates the bin in the  x-th x, and y-th y bin
    bin_list.push_back(tmp_bin_column);
  }
  int Left_new = left() - (x_bin_num * bin_width - (right() - left()))/2;
  int Bottom_new = bottom() - (y_bin_num * bin_height - (top() - bottom()))/2;
  virtual_bin_boundary.set_dllx(Left_new);
  virtual_bin_boundary.set_dlly(Bottom_new);
  virtual_bin_boundary.set_width(x_bin_num * bin_width);
  virtual_bin_boundary.set_height(y_bin_num * bin_height);
  for (size_t x=0; x<bin_list.size(); x++) {
    for (size_t y=0; y<bin_list[x].size(); y++) {
      bin_list[x][y].set_left(Left_new + x*bin_width);
      bin_list[x][y].set_bottom(Bottom_new + y*bin_height);
      bin_list[x][y].set_width(bin_width);
      bin_list[x][y].set_height(bin_height);
    }
  }
}

bool placer_al_t::draw_bin_list(std::string const &filename) {
  std::ofstream ost(filename.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open output file: " << filename << "\n";
    return false;
  }
  for (auto &&bin_column: bin_list) {
    for (auto &&bin: bin_column) {
      ost << "rectangle('Position',[" << bin.left() << " " << bin.bottom() << " " << bin.width() << " " << bin.height() << "],'LineWidth',1)\n";
    }
  }
  ost << "rectangle('Position',[" << left() << " " << bottom() << " " << right() - left() << " " << top() - bottom() << "],'LineWidth',1)\n";
  ost << "axis auto equal\n";
  ost.close();

  return true;
}

void placer_al_t::shift_cg_solution_to_region_center() {
  double leftmost = block_list[0].dllx(), bottommost = block_list[0].dlly(), rightmost = block_list[0].durx(), topmost = block_list[0].dury();
  for (auto &&block: block_list) {
    if (block.dllx() < leftmost) leftmost = block.dllx();
    if (block.dlly() < bottommost) bottommost = block.dlly();
    if (block.durx() > rightmost) rightmost = block.durx();
    if (block.dury() > topmost) topmost = block.dury();
  }
  for (auto &&block: block_list) {
    block.set_dllx(block.dllx() - leftmost + 1/2.0*(right() + left() - rightmost + leftmost));
    block.set_dlly(block.dlly() - bottommost + 1/2.0*(top() + bottom() - topmost + bottommost));
  }
}

bool placer_al_t::draw_block_net_list(std::string const &filename) {
  std::ofstream ost(filename.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open output file: " << filename << "\n";
    return false;
  }
  for (auto &&block: block_list) {
    ost << "rectangle('Position',[" << block.dllx() << " " << block.dlly() << " " << block.width() << " " << block.height() << "], 'LineWidth', 1, 'EdgeColor','blue')\n";
  }
  block_al_t *block_ptr0, *block_ptr1;
  for (auto &&net: net_list) {
    for (size_t i=0; i<net.pin_list.size(); i++) {
      block_ptr0 = (block_al_t *)net.pin_list[i].get_block();
      for (size_t j=i+1; j<net.pin_list.size(); j++) {
        block_ptr1 = (block_al_t *)net.pin_list[j].get_block();
        ost << "line([" << block_ptr0->dllx() + net.pin_list[i].x_offset() << "," << block_ptr1->dllx() + net.pin_list[j].x_offset()
               << "],[" << block_ptr0->dlly() + net.pin_list[i].y_offset() << "," << block_ptr1->dlly() + net.pin_list[j].y_offset() << "],'lineWidth', 0.5)\n";
      }
    }
  }
  //ost << "rectangle('Position',[" << left() << " " << bottom() << " " << right() - left() << " " << top() - bottom() << "],'LineWidth',1)\n";
  ost << "rectangle('Position',[" << left() << " " << bottom() << " " << right()-left() << " " << top()-bottom() << "],'LineWidth',1)\n";
  ost << "axis auto equal\n";
  ost.close();

  return true;
}

void placer_al_t::expansion_legalization() {
  double b_left, b_right, b_bottom, b_top;
  b_left = right();
  b_right = left();
  b_top = bottom();
  b_bottom = top();
  int max_height = 0;
  int max_width = 0;
  for (auto &&block: block_list) {
    if (block.is_movable()) {
      if (block.dx() < b_left) {
        b_left = block.dx();
      }
      if (block.dx() > b_right) {
        b_right = block.dx();
      }
      if (block.dy() < b_bottom) {
        b_bottom = block.dy();
      }
      if (block.dy() > b_top) {
        b_top = block.dy();
      }
      if (block.height() > max_height) {
        max_height = block.height();
      }
      if (block.width() > max_width) {
        max_width = block.width();
      }
    }
  }
  double box_width = b_right -b_left;
  double box_height = b_top - b_bottom;

  double mod_region_width = (right() - left())*sqrt(_filling_rate);
  double mod_region_height = (top() - bottom())*sqrt(_filling_rate);
  double mod_left = (left() + right())/2.0 - mod_region_width/2.0;
  double mod_bottom = (bottom() + top())/2.0 - mod_region_height/2.0;

  for (auto &&block: block_list) {
    if (block.is_movable()) {
      block.set_center_dx(mod_left + (block.dx() - b_left)/box_width * mod_region_width);
      block.set_center_dy(mod_bottom + (block.dy() - b_bottom)/box_height * mod_region_height);
    }
  }

  /*
  double gravity_center_x = 0;
  double gravity_center_y = 0;
  for (auto &&block: block_list) {
    if (block.is_movable()) {
      gravity_center_x += block.dx();
      gravity_center_y += block.dy();
    }
  }
  gravity_center_x /= block_list.size();
  gravity_center_y /= block_list.size();
  for (auto &&block: block_list) {
    if (block.is_movable()) {
      block.set_center_dx(block.dx() - (gravity_center_x - (right() - left())/2.0));
      block.set_center_dy(block.dy() - (gravity_center_y - (top() - bottom())/2.0));
    }
  }
   */

}

void placer_al_t::update_block_in_bin() {
  double leftmost=bin_list[0][0].left(), bottommost=bin_list[0][0].bottom();
  int X_bin_list_size = bin_list.size(), Y_bin_list_size = bin_list[0].size();
  int L,B,R,T; // left, bottom, right, top of the cell in which bin
  for (auto &&bin_column: bin_list) {
    for (auto &&bin: bin_column) {
      bin.CIB.clear();
    }
  }
  for (size_t i=0; i<block_list.size(); i++) {
    block_list[i].bin.clear();
    L = floor((block_list[i].dllx() - leftmost)/bin_width);
    B = floor((block_list[i].dlly() - bottommost)/bin_height);
    R = floor((block_list[i].durx() - leftmost)/bin_width);
    T = floor((block_list[i].dury() - bottommost)/bin_height);
    if (R >= X_bin_list_size) {
      R = X_bin_list_size - 1;
    }
    if (T >= Y_bin_list_size) {
      T = Y_bin_list_size - 1;
    }
    if (L < 0) {
      L = 0;
    }
    if (B < 0) {
      B = 0;
    }
    for (int x=L; x<=R; x++) {
      for (int y=B; y<=T; y++) {
        bin_list[x][y].CIB.push_back(i);
        bin_index tmp_bin_loc(x,y);
        block_list[i].bin.push_back(tmp_bin_loc);
      }
    }
  }
}

bool placer_al_t::check_legal() {
  for (size_t i=0; i<block_list.size(); i++) {
    for (size_t j=i+1; j<block_list.size(); j++) {
      if (block_list[i].is_overlap(block_list[j])) {
        return false;
      }
    }
    for (auto &&boundary: boundary_list) {
      if (block_list[i].is_overlap(boundary)) {
        return false;
      }
    }
  }
  return true;
}

void placer_al_t::integerize() {
  for (auto &&block: block_list) {
    block.set_dllx((int)block.dllx());
    block.set_dlly((int)block.dlly());
  }
}

void placer_al_t::update_velocity() {
  double rij, overlap = 0, areai;
  //double maxv1 = 10, maxv2 = 20; // for layout
  double maxv1 = (_circuit->ave_width() + _circuit->ave_height())/20, maxv2 = 2*maxv1;
  for (auto &&block: block_list) {
    block.vx = 0;
    block.vy = 0;
  }

  double leftmost = bin_list[0][0].left(), bottommost = bin_list[0][0].bottom();
  int Ybinlistsize = bin_list[0].size(), Xbinlistsize = bin_list.size();
  double Left, Right, Bottom, Top;

  int L,B,R,T; // left, bottom, right, top of the cell in which bin
  std::set<int> temp_physical_neighbor_set;
  for (size_t i=0; i<block_list.size(); i++) {
    if (!block_list[i].is_movable()) continue;
    Left = block_list[i].dllx(), Right = block_list[i].durx();
    Bottom = block_list[i].dlly(), Top = block_list[i].dury();
    L = floor((Left - leftmost)/bin_width);
    B = floor((Bottom - bottommost)/bin_height);
    R = floor((Right - leftmost)/bin_width);
    T = floor((Top - bottommost)/bin_height);
    for (int y=B-1; y<=T+1; y++) { // find all cells around cell i, and put all these cells into the set "temp_physical_neighbor_set", there is a reason to do so is because a cell might appear in different bins
      if ((y>=0)&&(y<Ybinlistsize)) {
        for (int x=L-1; x<=R+1; x++) {
          if ((x>=0)&&(x<Xbinlistsize)) {
            for (auto &&cell_num_in_bin: bin_list[x][y].CIB) {
              if (i==(size_t)cell_num_in_bin) continue;
              temp_physical_neighbor_set.insert(cell_num_in_bin);
            }
          }
        }
      }
    }

    for (auto &&block_num: temp_physical_neighbor_set) {
      overlap = block_list[i].overlap_area(block_list[block_num]);
      rij = sqrt(pow(block_list[i].dx() - block_list[block_num].dx(), 2) + pow(block_list[i].dy()-block_list[block_num].dy(), 2));
      block_list[i].vx += maxv1*overlap*(block_list[i].dx() - block_list[block_num].dx())/rij; // the maximum velocity of each cell is 5 at maxmimum
      block_list[i].vy += maxv1*overlap*(block_list[i].dy() - block_list[block_num].dy())/rij;
    }
    temp_physical_neighbor_set.clear(); // clear this set after calculate the velocity due to overlap of cells

    for (size_t j=0; j<boundary_list.size(); j++) {
      overlap = block_list[i].overlap_area(boundary_list[j]);
      rij = sqrt(pow(block_list[i].dx() - boundary_list[j].dx(), 2) + pow(block_list[i].dy() - boundary_list[j].dy(), 2));

      //block_list[i].vx += maxv2*overlap*(block_list[i].dx() - boundary_list[j].dx())/rij;
      //block_list[i].vy += maxv2*overlap*(block_list[i].dy() - boundary_list[j].dy())/rij;

      if ((boundary_list[j].dx() > left())&&(boundary_list[j].dx() < right())) {
        block_list[i].vy += maxv2*overlap*(block_list[i].dy() - boundary_list[j].dy())/rij;
      }
      else if ((block_list[j].dy() > bottom())&&(block_list[j].dy() < top())) {
        block_list[i].vx += maxv2*overlap*(block_list[i].dx() - boundary_list[j].dx())/rij;
      }
      else {
        block_list[i].vx += maxv2*overlap*(block_list[i].dx() - boundary_list[j].dx())/rij;
        block_list[i].vy += maxv2*overlap*(block_list[i].dy() - boundary_list[j].dy())/rij;
      }

    }
    areai = block_list[i].area();
    block_list[i].vx = block_list[i].vx/areai;
    block_list[i].vy = block_list[i].vy/areai;
    block_list[i].modif_vx();
    block_list[i].modif_vy();
  }
}

void placer_al_t::update_velocity_force_damping() {
  double rij, overlap = 0, areai;
  double maxv1 = 5, maxv2 = 30;
  for (auto &&block: block_list) {
    block.vx = 0;
    block.vy = 0;
  }

  double leftmost = bin_list[0][0].left(), bottommost = bin_list[0][0].bottom();
  int Ybinlistsize = bin_list[0].size(), Xbinlistsize = bin_list.size();
  double Left, Right, Bottom, Top;

  int L,B,R,T; // left, bottom, right, top of the cell in which bin
  std::set<int> temp_physical_neighbor_set;
  for (size_t i=0; i<block_list.size(); i++) {
    if (!block_list[i].is_movable()) continue;
    Left = block_list[i].dllx(), Right = block_list[i].durx();
    Bottom = block_list[i].dlly(), Top = block_list[i].dury();
    L = floor((Left - leftmost)/bin_width);
    B = floor((Bottom - bottommost)/bin_height);
    R = floor((Right - leftmost)/bin_width);
    T = floor((Top - bottommost)/bin_height);
    for (int y=B-1; y<=T+1; y++) { // find all cells around cell i, and put all these cells into the set "temp_physical_neighbor_set", there is a reason to do so is because a cell might appear in different bins
      if ((y>=0)&&(y<Ybinlistsize)) {
        for (int x=L-1; x<=R+1; x++) {
          if ((x>=0)&&(x<Xbinlistsize)) {
            for (auto &&cell_num_in_bin: bin_list[x][y].CIB) {
              if (i==(size_t)cell_num_in_bin) continue;
              temp_physical_neighbor_set.insert(cell_num_in_bin);
            }
          }
        }
      }
    }

    double force_x = 0;
    double force_y = 0;
    for (auto &&block_num: temp_physical_neighbor_set) {
      overlap = block_list[i].overlap_area(block_list[block_num]);
      rij = sqrt(pow(block_list[i].dx() - block_list[block_num].dx(), 2) + pow(block_list[i].dy()-block_list[block_num].dy(), 2));
      force_x += maxv1*overlap*(block_list[i].dx() - block_list[block_num].dx())/rij;
      force_y += maxv1*overlap*(block_list[i].dy() - block_list[block_num].dy())/rij;
    }
    temp_physical_neighbor_set.clear(); // clear this set after calculate the velocity due to overlap of cells

    for (size_t j=0; j<boundary_list.size(); j++) {
      overlap = block_list[i].overlap_area(boundary_list[j]);
      rij = sqrt(pow(block_list[i].dx() - boundary_list[j].dx(), 2) + pow(block_list[i].dy() - boundary_list[j].dy(), 2));
      if ((boundary_list[j].dx() > left())&&(boundary_list[j].dx() < right())) {
        force_y += maxv2*overlap*(block_list[i].dy() - boundary_list[j].dy())/rij;
      }
      else if ((block_list[j].dy() > bottom())&&(block_list[j].dy() < top())) {
        force_x += maxv2*overlap*(block_list[i].dx() - boundary_list[j].dx())/rij;
      }
      else {
        force_x += maxv2*overlap*(block_list[i].dx() - boundary_list[j].dx())/rij;
        force_y += maxv2*overlap*(block_list[i].dy() - boundary_list[j].dy())/rij;
      }

    }
    areai = block_list[i].area();
    force_x = force_x/areai;
    force_y = force_y/areai;
    block_list[i].modif_vx();
    block_list[i].modif_vy();
  }
}

void placer_al_t::update_position() {
  for (auto &&block:block_list) {
    block.update_loc(time_step);
  }
}

void placer_al_t::diffusion_legalization() {
  for (int i=0; i< iteration_limit_diffusion; i++) {
    update_block_in_bin();
    update_velocity();
    update_position();
  }
}

bool placer_al_t::legalization() {
  update_block_in_bin();
  time_step = 5;
  for (int i=0; i<max_legalization_iteration; i++) {
    if (check_legal()) {
      integerize();
      if (check_legal()) {
        std::cout << i << " iterations\n";
        break;
      }
    }
    diffusion_legalization();
    if (time_step > 1) time_step -= 1;
    if (i==max_legalization_iteration-1) {
      std::cout << "Molecular-dynamic legalization finish\n";
      return false;
    }
  }
  std::cout << "Molecular-dynamic legalization succeeds\n";
  return true;
}

void placer_al_t::diffusion_with_gravity() {
  double drift_v = -(_circuit->ave_width() + _circuit->ave_height())/300;
  for (int i=0; i<max_legalization_iteration; i++) {
    update_block_in_bin();
    update_velocity();
    for (auto &&block: block_list) {
      if (i%2==0) {
        block.add_gravity_vx(drift_v);
      } else {
        block.add_gravity_vy(drift_v);
      }
    }
    update_position();
  }
}

bool placer_al_t::gravity_legalization() {
  update_block_in_bin();
  diffusion_with_gravity();
  time_step = 5;
  for (int i=0; i<max_legalization_iteration; i++) {
    if (check_legal()) {
      integerize();
      if (check_legal()) {
        std::cout << i << " iterations\n";
        break;
      }
    }
    diffusion_legalization();
    if (time_step > 1) time_step -= 1;
    if (i==max_legalization_iteration-1) {
      std::cout << "Gravity legalization finish\n";
      return false;
    }
  }
  std::cout << "Gravity legalization succeeds\n";
  return true;
}

bool placer_al_t::post_legalization_optimization() {

  return true;
}

bool placer_al_t::start_placement() {
  cg_init();
  uniform_initialization();
  update_max_min_node_x();
  update_max_min_node_y();
  HPWLx_converge = false;
  HPWLy_converge = false;
  for (int i=0; i<50; i++) {
    if (!HPWLx_converge) {
      build_problem_b2b_x();
      CG_solver_x();
      update_max_min_node_x();
    }
    if (!HPWLy_converge) {
      build_problem_b2b_y();
      CG_solver_y();
      update_max_min_node_y();
    }
    if (HPWLx_converge && HPWLy_converge)  {
      std::cout << i << " iterations in cg\n";
      break;
    }
  }
  cg_close();
  std::cout << "Initial Placement Complete\n";
  report_hpwl();

  shift_cg_solution_to_region_center();
  expansion_legalization();
  add_boundary_list();
  initialize_bin_list();
  if (!legalization()) {
    gravity_legalization();
  }
  post_legalization_optimization();
  std::cout << "Legalization Complete\n";
  report_hpwl();
  //draw_bin_list();
  //draw_block_net_list();

  return true;
}

void placer_al_t::report_placement_result() {
  for (size_t i=0; i<block_list.size(); i++) {
    if (block_list[i].is_movable()) {
      _circuit->block_list[i].set_llx((int)block_list[i].dllx());
      _circuit->block_list[i].set_lly((int)block_list[i].dlly());
    }
  }
}

void placer_al_t::report_hpwl() {
  update_HPWL_x();
  update_HPWL_y();
  std::cout << "HPWL: " << HPWLX_new + HPWLY_new << "\n";
}
