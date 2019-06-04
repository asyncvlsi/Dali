//
// Created by Yihang Yang on 2019-05-20.
//

#include <queue>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>
#include "placerdla.hpp"

placer_dla_t::placer_dla_t(): placer_t() {
  bin_width = 0;
  bin_height = 0;
}

placer_dla_t::placer_dla_t(double aspectRatio, double fillingRate): placer_t(aspectRatio, fillingRate) {
  bin_width = 0;
  bin_height = 0;
}

bool placer_dla_t::set_input_circuit(circuit_t *circuit) {
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
  for (auto &&block: circuit->block_list) {
    block_dla_t block_dla;
    block_dla.retrieve_info_from_database(block);
    block_list.push_back(block_dla);
    //std::cout << block_dla << "\n";
  }

  for (auto &&net: circuit->net_list) {
    net_dla_t net_dla;
    net_dla.retrieve_info_from_database(net);
    for (auto &&pin: net.pin_list) {
      int block_num = circuit->block_name_map.find(pin.get_block()->name())->second;
      block_dla_t *block_dla_ptr = &block_list[block_num];
      pin_t pin_dla(pin.x_offset(), pin.y_offset(), block_dla_ptr);
      net_dla.pin_list.push_back(pin_dla);
    }
    net_list.push_back(net_dla);
  }
  return true;
}

void placer_dla_t::add_boundary_list() {
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

  block_dla_t tmp_block;
  int multiplier = 500;
  tmp_block.set_movable(false);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_llx(left() - tmp_block.width());
  tmp_block.set_lly(top());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(right() - left());
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_llx(left());
  tmp_block.set_lly(top());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_llx(right());
  tmp_block.set_lly(top());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_width(top() - bottom());
  tmp_block.set_llx(right());
  tmp_block.set_lly(bottom());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_llx(right());
  tmp_block.set_lly(bottom() - tmp_block.height());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(right() - left());
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_llx(left());
  tmp_block.set_lly(bottom() - tmp_block.height());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(multiplier*max_height);
  tmp_block.set_llx(left() - tmp_block.width());
  tmp_block.set_lly(bottom() - tmp_block.height());
  boundary_list.push_back(tmp_block);

  tmp_block.set_width(multiplier*max_width);
  tmp_block.set_height(top() - bottom());
  tmp_block.set_llx(left() - tmp_block.width());
  tmp_block.set_lly(bottom());
  boundary_list.push_back(tmp_block);
}

void placer_dla_t::initialize_bin_list(){
  // initialize the bin_list, build up the bin_list matrix
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
  virtual_bin_boundary.set_llx(Left_new);
  virtual_bin_boundary.set_lly(Bottom_new);
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

void placer_dla_t::update_neighbor_list() {
  // update the list of neighbor and the wire numbers connected to its neighbor, and the nets connected to this cell
  block_dla_t *block_dla1, *block_dla2;
  for (auto &&net: net_list) {
    for (auto &&pin: net.pin_list) {
      block_dla1 = (block_dla_t *)(pin.get_block());
      block_dla1->add_to_net(&net);
    }
    for (size_t i=0; i<net.pin_list.size(); i++) {
      block_dla1 = (block_dla_t *)(net.pin_list[i].get_block());
      for (size_t j=i+1; j<net.pin_list.size(); j++) {
        block_dla2 = (block_dla_t *)(net.pin_list[j].get_block());
        block_dla1->add_to_neb_list(block_dla2, net.inv_p());
        block_dla2->add_to_neb_list(block_dla1, net.inv_p());
      }
    }
  }
  for (auto &&block: block_list) {
    block.sort_neb_list();
  }
  /*
  for (auto &&block: block_list) {
    std::cout << "Block: " << block.name() << " is connected to net: ";
    for (auto &&net_ptr: block.net) {
      std::cout << net_ptr->name() << " ";
    }
    std::cout << "\t";

    std::cout << "In total " << block.total_net() << " net(s).\n";
  }
  for (auto &&net: net_list) {
    std::cout << "Net " << net.name() << " connects cells: ";
    for (auto &&pin: net.pin_list) {
      std::cout << "\t" <<  pin.get_block()->name();
    }
    std::cout << "\n";
  }
  for (auto &&block: block_list) {
    double total_wire_weight = 0;
    for (auto &&neb: block.neb_list) {
      std::cout << "Block " << block.name() << " is connected to " << neb.block->name() << " with wire numbers " << neb.total_wire_weight << "\n";
      total_wire_weight += neb.total_wire_weight;
    }
    std::cout << block.num() << " is connected to " << total_wire_weight << " wire(s)\n";
  }
   */
}

void placer_dla_t::prioritize_block_to_place(){
  // creat a new cell list block_to_place_queue to store the sorted block_list based on the number of wires connecting to the cell
  std::vector<std::pair <int, int>> pair_list;
  for (auto &&block: block_list) {
    pair_list.emplace_back(std::make_pair(block.num(), block.total_net()));
  }
  int max_wire_index;
  std::pair <int,int> tmp_pair;
  for (size_t i=0; i<block_list.size(); i++) {
    max_wire_index = i;
    for (size_t j=i+1; j<block_list.size(); j++) {
      if (pair_list[j].second > pair_list[max_wire_index].second) {
        max_wire_index = j;
      }
    }
    tmp_pair = pair_list[i];
    pair_list[i] = pair_list[max_wire_index];
    pair_list[max_wire_index] = tmp_pair;
  }

  for (auto &&pair: pair_list) {
    //std::cout << pair.first << " is connected to " << pair.second << " nets\n";
    block_to_place_queue.push(pair.first); // the queue of sorted cell list
  }

}

void placer_dla_t::update_bin_list(int first_blk_num) {
  int left_most = bin_list[0][0].left(), bottom_most = bin_list[0][0].bottom();
  int X_bin_list_size = bin_list.size(), Y_bin_list_size = bin_list[0].size();
  double block_left = block_list[first_blk_num].llx(), block_right = block_list[first_blk_num].urx();
  double block_bottom = block_list[first_blk_num].lly(), block_top = block_list[first_blk_num].ury();

  //std::cout << "Block: " << first_blk_num << " is in bins: ";
  if (virtual_bin_boundary.overlap_area(block_list[first_blk_num]) < block_list[first_blk_num].area()) {
    block_out_of_bin.push_back(first_blk_num);
    //std::cout << "-1 -1; ";
  }
  int L,B,R,T; // block_left, block_bottom, block_right, block_top of the cell in which bin
  L = std::floor((block_left - left_most)/bin_width);
  B = std::floor((block_bottom - bottom_most)/bin_height);
  R = std::floor((block_right - left_most)/bin_width);
  T = std::floor((block_top - bottom_most)/bin_height);
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
  for (int j=B; j<=T; j++) {
    for (int k=L; k<=R; k++) {
      std::cout << j << " " << k << "; ";
      bin_list[j][k].CIB.push_back(first_blk_num);
      bin_index tmp_bin_loc(j,k);
      block_list[first_blk_num].bin.push_back(tmp_bin_loc);
    }
  }
  std::cout << "\n";
}

bool placer_dla_t::random_release_from_boundaries(int boundary_num, block_dla_t &block) {
  switch (boundary_num) {
    case 0:
      // 2*Left boundry
      block.set_llx(left() - 1*(right() - left()));
      block.set_lly(bottom() + std::rand()%(top() - bottom() - block.height() + 1));
      break;
    case 1:
      // 2*Top boundry
      block.set_llx(left() + std::rand()%(right() - left() - block.width() + 1));
      block.set_lly(top() + 1*(top() - bottom()));
      break;
    case 2:
      // 2*Right boundry
      block.set_llx(right() + 1*(right() - left()));
      block.set_lly(bottom() + std::rand()%(top() - bottom() - block.height() + 1));
      break;
    case 3:
      // 2*Bottom boundry
      block.set_llx(left() + std::rand()%(right() - left() - block.width() + 1));
      block.set_lly(bottom() - 1*(top() - bottom()));
      break;
    default:
      std::cout << "Error\n!";
      std::cout << "Invalid release boundaries\n";
      return false;
  }
  return true;
}

bool placer_dla_t::is_legal(int first_blk_num) {
  // if legal, return -1, if illegal, return the cell with which this first_blk_num has overlap
  int X_bin_list_size = bin_list.size(), Y_bin_list_size = bin_list[0].size();

  block_dla_t *block = &block_list[first_blk_num];
  block_dla_t *block_tmp;
  if (virtual_bin_boundary.overlap_area(*block) < block->area()) {
    for (auto &&block_index: block_out_of_bin) {
      block_tmp = &block_list[block_index];
      if ((block != block_tmp)&&(block->is_overlap(*block_tmp))) {
        return false;
      }
    }
  }
  int L,B,R,T; // left, bottom, right, top of the cell in which bin
  L = floor((block->llx() - virtual_bin_boundary.llx())/bin_width);
  B = floor((block->lly() - virtual_bin_boundary.lly())/bin_height);
  R = floor((block->urx() - virtual_bin_boundary.llx())/bin_width);
  T = floor((block->ury() - virtual_bin_boundary.lly())/bin_height);
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
  for (int j=B; j<=T; j++) {
    for (int k=L; k<=R; k++) {
      std::cout << j << " " << k << " " << bin_list[j][k].CIB.empty() << "\n";
      if (bin_list[j][k].CIB.empty()) {
        continue;
      }
      std::cout << "can you keep going?\n";
      for (auto &&block_index: bin_list[j][k].CIB) {
        block_tmp = &block_list[block_index];
        if ((block != block_tmp)&&(block->is_overlap(*block_tmp))) {
          return false;
        }
      }
    }
  }
  return true;
}

void placer_dla_t::diffuse(int first_blk_num) {
  double WL1, WL2, MW; // MW indicates minimum wirelength
  double modWL1, modWL2;
  //int step;
  double center_x=(left() + right())/2.0, center_y=(bottom() + top())/2.0;
  double cell_center_x, cell_center_y, distance_to_center;
  double p1=0.5, p2; // accept probability
  double r, T=20.0;
  block_dla_t TLUM; // Temporary Location Under Motion
  block_dla_t MWLL; // Minimum Wire-Length Location
  random_release_from_boundaries(0, block_list[first_blk_num]);
  MW = block_list[first_blk_num].wire_length_during_dla();
  for (int i=0; i<4; i++) {
    // release this cell from all 4 boundries and find the location, which gives the smallest total wirelength
    //std::cout << i << "\n";
    random_release_from_boundaries(i, block_list[first_blk_num]);
    //std::cout << "here2\n";
    //int counter = 0;
    while (true) {
      //std::cout << counter++ << "\t";
      //std::cout << "here5\t";
      cell_center_x = block_list[first_blk_num].x();
      cell_center_y = block_list[first_blk_num].y();
      distance_to_center = fabs(cell_center_x - center_x)+fabs(cell_center_y - center_y);
      //std::cout << "here3\t";
      WL1 = block_list[first_blk_num].wire_length_during_dla();
      //std::cout << "here4\t";
      //std::cout << i << " " << "WL1: " << WL1 << "\t";
      modWL1 = WL1 + distance_to_center/4.0;
      TLUM = block_list[first_blk_num];
      //step = (int)(WL1/100.0);
      block_list[first_blk_num].random_move(1);
      cell_center_x = block_list[first_blk_num].x();
      cell_center_y = block_list[first_blk_num].y();
      //std::cout << first_blk_num << " " << cell_center_x << " " << cell_center_y << "\n";
      distance_to_center = fabs(cell_center_x - center_x)+fabs(cell_center_y - center_y);
      WL2 = block_list[first_blk_num].wire_length_during_dla();
      //std::cout << "WL2: " << WL2 << "\t";
      modWL2 = WL2 + distance_to_center/4.0;
      //std::cout << "here0\t";
      if (is_legal(first_blk_num)) {
        //std::cout << "here6\t";
        if (modWL2 > modWL1) {
          // if wire-length becomes larger, reject this move by some probability
          r = ((double) std::rand()/RAND_MAX);
          p2 = exp(-(modWL2-modWL1)/T);
          if (r > p2) block_list[first_blk_num] = TLUM;
        }
        if (WL2<=MW) {
          MW = WL2;
          MWLL = block_list[first_blk_num];
        }
      } else {
        //std::cout << "here7\t";
        block_list[first_blk_num] = TLUM;
        r = ((double)std::rand()/RAND_MAX);
        if (r<=p1) break;
      }
      //std::cout << "here1\n";
    }
  }
  block_list[first_blk_num] = MWLL;

  //cout << first_blk_num << " " << MWLL.cellNo << "\n";
  //for (int j=0; j<block_list.size(); j++)
  //{
    //if (block_list[j].cellNo == -1)
    //{
      //cout << j << " errordiffuse\n";
      //exit(1);
    //}
  //}
  update_bin_list(first_blk_num);
}

bool placer_dla_t::DLA() {
  std::queue<int> Q_place;
  int first_blk_num, tmp_num;
  first_blk_num = block_to_place_queue.front();
  block_to_place_queue.pop();
  Q_place.push(first_blk_num);
  block_list[first_blk_num].set_queued(true);
  int num_of_node_placed = 0;
  while (!Q_place.empty()) {
    std::cout << Q_place.size() << "\n";
    first_blk_num = Q_place.front();
    Q_place.pop();
    std::cout << "Placing cell " << first_blk_num << "\n";
    for (size_t j=0; j<block_list[first_blk_num].neb_list.size(); j++) {
      tmp_num = block_list[first_blk_num].neb_list[j].block->num();
      if (block_list[tmp_num].is_queued()){
        continue;
      } else {
        block_list[tmp_num].set_queued(true);
        Q_place.push(tmp_num);
      }
    }
    block_list[first_blk_num].set_placed(true);
    if (num_of_node_placed == 0) {
      block_list[first_blk_num].set_center_x((left() + right())/2.0);
      block_list[first_blk_num].set_center_y((bottom() + top())/2.0);
      update_bin_list(first_blk_num);
    }
    else {
      diffuse(first_blk_num);
    }
    num_of_node_placed += 1;
    //drawplaced(block_list, net_list, boundries);
    //if (numofcellplaced==2) break;
    if (Q_place.empty()) {
      while (!block_to_place_queue.empty()) {
        first_blk_num = block_to_place_queue.front();
        if (!block_list[first_blk_num].is_queued()) {
          Q_place.push(first_blk_num);
          break;
        } else {
          block_to_place_queue.pop();
        }
      }
    }
  }
  //std::cout << "Total WireLength after placement is " <<  TotalWireLength(block_list, net_list) << "\n";
  return true;
}

bool placer_dla_t::start_placement() {
  add_boundary_list();
  initialize_bin_list();
  update_neighbor_list();
  prioritize_block_to_place();
  draw_bin_list();
  DLA();
  return true;
}

bool placer_dla_t::draw_bin_list(std::string const &filename) {
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

bool placer_dla_t::draw_block_net_list(std::string const &filename) {
  std::ofstream ost(filename.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open output file: " << filename << "\n";
    return false;
  }
  for (auto &&block: block_list) {
    ost << "rectangle('Position',[" << block.llx() << " " << block.lly() << " " << block.width() << " " << block.height() << "], 'LineWidth',3)\n";
  }
  /*
  int block_num1, block_num2;
  int x1, x2, y1, y2;
  int xoff1, xoff2, yoff1, yoff2;
  for (int i=0; i<net_list.size(); i++) {
    block_num1 = net_list[i].pin_list[0].cellNo;
    block_num2 = net_list[i].pin_list[1].cellNo;
    x1 = block_list[block_num1].llx(); y1 = block_list[block_num1].lly();
    x2 = block_list[block_num2].llx(); y2 = block_list[block_num2].lly();
    xoff1 = net_list[i].pin_list[0].x_offset(); yoff1 = net_list[i].pin_list[0].y_offset();
    xoff2 = net_list[i].pin_list[1].x_offset(); yoff2 = net_list[i].pin_list[1].y_offset();
    ost << "line([" << x1+xoff1 << "," << x2+xoff2 << "],[" << y1+yoff1 << "," << y2+yoff2 << "],'lineWidth', 2)\n";
    //ost << "line([" << x1+xoff1 << "," << x2+xoff2 << "],[" << y1+yoff1 << "," << y2+yoff2 << "],'Color',[.5 .5 .5],'lineWidth', 2)\n";
  }
   */
  ost << "rectangle('Position',[" << left() << " " << bottom() << " " << right() - left() << " " << top() - bottom() << "],'LineWidth',1)\n";
  ost << "axis auto equal\n";
  ost.close();

  return true;
}

bool placer_dla_t::draw_placed_blocks(std::string const &filename) {
  std::ofstream ost(filename.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open output file: " << filename << "\n";
    return false;
  }
  for (auto &&block: block_list) {
    if (block.is_placed()) {
      ost << "rectangle('Position',[" << block.llx() << " " << block.lly() << " " << block.width() << " " << block.height() << "], 'LineWidth',3)%" << block.num() << "\n";
    }
  }
  /*
  int block_num1, block_num2, tempnetsize;
  int x1, x2, y1, y2;
  int xoff1, xoff2, yoff1, yoff2;
  for (int i=0; i<net_list.size(); i++)
  {
    tempnetsize = net_list[i].pin_list.size();
    for (int j=0; j<tempnetsize; j++)
    {
      for (int k=j+1; k<tempnetsize; k++)
      {
        block_num1 = net_list[i].pin_list[j].cellNo;
        block_num2 = net_list[i].pin_list[k].cellNo;
        x1 = block_list[block_num1].llx(); y1 = block_list[block_num1].lly();
        x2 = block_list[block_num2].llx(); y2 = block_list[block_num2].lly();
        xoff1 = net_list[i].pin_list[j].x_offset(); yoff1 = net_list[i].pin_list[j].y_offset();
        xoff2 = net_list[i].pin_list[k].x_offset(); yoff2 = net_list[i].pin_list[k].y_offset();
        ost << "line([" << x1+xoff1 << "," << x2+xoff2 << "],[" << y1+yoff1 << "," << y2+yoff2 << "],'lineWidth', " << 3*1.0/(tempnetsize-1) << ")\n";
      }
    }
  }
   */
  ost << "rectangle('Position',[" << left() << " " << bottom() << " " << right() - left() << " " << top() - bottom() << "],'LineWidth',1)\n";
  ost << "axis auto equal\n";
  ost.close();

  return true;
}

bool placer_dla_t::output_result(std::string const &filename) {
  std::ofstream ost(filename.c_str());
  if (ost.is_open()==0) {
    std::cout << "Cannot open output file: " << filename << "\n";
    return false;
  }
  for (auto &&block: block_list) {
    ost << "\t" << block.name() << "\t" << block.llx() << "\t" << block.lly() << "\t : N\n";
  }
  ost.close();

  return true;
}

placer_dla_t::~placer_dla_t() = default;
