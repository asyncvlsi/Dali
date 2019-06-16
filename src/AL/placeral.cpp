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
  HPWLx_converge = (std::fabs(1 - HPWLX_new/HPWLX_old) < HPWL_intra_linearSolver_precision);
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
  double weightx, invp, temppinlocx0, temppinlocx1, tempdiffoffset;
  size_t tempnodenum0, tempnodenum1, maxpindex_x, minpindex_x;
  for (auto &&net: net_list) {
    if (net.p()<=1) continue;
    invp = net.inv_p();
    maxpindex_x = net.max_pin_index_x();
    minpindex_x = net.min_pin_index_x();
    for (size_t i=0; i<net.pin_list.size(); i++) {
      tempnodenum0 = net.pin_list[i].get_block()->num();
      temppinlocx0 = block_list[tempnodenum0].dllx() + net.pin_list[i].x_offset();
      for (size_t k=i+1; k<net.pin_list.size(); k++) {
        if ((i!=maxpindex_x)&&(i!=minpindex_x)) {
          if ((k!=maxpindex_x)&&(k!=minpindex_x)) continue;
        }
        tempnodenum1 = net.pin_list[k].get_block()->num();
        if (tempnodenum0 == tempnodenum1) continue;
        temppinlocx1 = block_list[tempnodenum1].dllx() + net.pin_list[k].x_offset();
        weightx = invp/((double)fabs(temppinlocx0 - temppinlocx1) + width_epsilon());
        if ((block_list[tempnodenum0].is_movable() == 0)&&(block_list[tempnodenum1].is_movable() == 0)) {
          continue;
        }
        else if ((block_list[tempnodenum0].is_movable() == 0)&&(block_list[tempnodenum1].is_movable() == 1)) {
          bx[tempnodenum1] += (temppinlocx0 - net.pin_list[k].x_offset()) * weightx;
          Ax[tempnodenum1][0].weight += weightx;
        }
        else if ((block_list[tempnodenum0].is_movable() == 1)&&(block_list[tempnodenum1].is_movable() == 0)) {
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
  HPWLy_converge = (std::fabs(1 - HPWLY_new/HPWLY_old) < HPWL_intra_linearSolver_precision);
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
  double weighty, invp, temppinlocy0, temppinlocy1, tempdiffoffset;
  size_t tempnodenum0, tempnodenum1, maxpindex_y, minpindex_y;
  for (auto &&net: net_list) {
    if (net.p()<=1) continue;
    invp = net.inv_p();
    maxpindex_y = net.max_pin_index_y();
    minpindex_y = net.min_pin_index_y();
    for (size_t i=0; i<net.pin_list.size(); i++) {
      tempnodenum0 = net.pin_list[i].get_block()->num();
      temppinlocy0 = block_list[tempnodenum0].dlly() + net.pin_list[i].y_offset();
      for (size_t k=i+1; k<net.pin_list.size(); k++) {
        if ((i!=maxpindex_y)&&(i!=minpindex_y)) {
          if ((k!=maxpindex_y)&&(k!=minpindex_y)) continue;
        }
        tempnodenum1 = net.pin_list[k].get_block()->num();
        if (tempnodenum0 == tempnodenum1) continue;
        temppinlocy1 = block_list[tempnodenum1].dlly() + net.pin_list[i].y_offset();
        weighty = invp/((double)fabs(temppinlocy0 - temppinlocy1) + height_epsilon());
        if ((block_list[tempnodenum0].is_movable() == 0)&&(block_list[tempnodenum1].is_movable() == 0)) {
          continue;
        }
        else if ((block_list[tempnodenum0].is_movable() == 0)&&(block_list[tempnodenum1].is_movable() == 1)) {
          by[tempnodenum1] += (temppinlocy0 - net.pin_list[k].y_offset()) * weighty;
          Ay[tempnodenum1][0].weight += weighty;
        }
        else if ((block_list[tempnodenum0].is_movable() == 1)&&(block_list[tempnodenum1].is_movable() == 0)) {
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
  static double epsilon = 1e-5, alpha, beta, rsold, rsnew, pAp, solution_distance;
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
  ost << "rectangle('Position',[" << left() << " " << bottom() << " " << right() - left() << " " << top() - bottom() << "],'LineWidth',1)\n";
  ost << "axis auto equal\n";
  ost.close();

  return true;
}

bool placer_al_t::start_placement() {
  cg_init();
  uniform_initialization();
  /* give each node a initial location, which is random inside the placement region, defined by LEFT, RIGHT, BOTTOM, TOP */
  update_max_min_node_x();
  update_max_min_node_y();
  /* update HPWLX, HPWLY, prepare for problem building */
  HPWLx_converge = false;
  HPWLy_converge = false;
  /* set HPWLx_converge and HPWLy_converge to false */
  for (int i=0; ; i++) {
    if (HPWLx_converge && HPWLy_converge) break;
    if (!HPWLx_converge) {
      build_problem_b2b_x();
      // fill elements into matrix Ax, bx
      CG_solver_x();
      // solve the linear equation for x direction
      update_max_min_node_x();
    }

    if (!HPWLy_converge) {
      build_problem_b2b_y();
      // fill elements into matrix Ay, by
      CG_solver_y();
      // solve the linear equation for y direction
      update_max_min_node_y();
    }
    if (HPWLx_converge && HPWLy_converge) break;
  }
  /*for (size_t i=0; i<CELL_NUM; i++) {
    if ((Nodelist[i].llx() < LEFT) || (Nodelist[i].urx() > RIGHT) || (Nodelist[i].lly() < BOTTOM) || (Nodelist[i].ury() > TOP)) {
      std::cout << "Final outboundary" << i << "\n";
    }
  }*/
  std::cout << "Initial Placement Complete\n";
  std::cout << "HPWL: " << HPWLX_new + HPWLY_new << "\n";

  shift_cg_solution_to_region_center();
  draw_block_net_list();

  return true;
}
