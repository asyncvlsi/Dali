//
// Created by Yihang Yang on 2019-05-20.
//

#include <queue>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>
#include "diffusionlimitedaggregationplacer.hpp"
#include "../circuit_node_net.hpp"

diffusion_limited_aggregation_placer::diffusion_limited_aggregation_placer() {
  circuit = nullptr;
  net_list = nullptr;
  LEFT = 0;
  RIGHT = 0;
  BOTTOM = 0;
  TOP = 0;
  bin_height = 0;
  bin_width = 0;
}

diffusion_limited_aggregation_placer::diffusion_limited_aggregation_placer(circuit_t &input_circuit) {
  bin_height = 0;
  bin_width = 0;
  circuit = &input_circuit;
  net_list = &input_circuit.net_list;
  node_list.resize(input_circuit.block_list.size());
  node_dla *node;
  for (size_t i=0; i<node_list.size(); i++) {
    node = &node_list[i];
    node->retrieve_info_from_database(input_circuit.block_list[i]);
  }
  LEFT = input_circuit.LEFT;
  RIGHT = input_circuit.RIGHT;
  BOTTOM = input_circuit.BOTTOM;
  TOP = input_circuit.TOP;
}

void diffusion_limited_aggregation_placer::set_input(circuit_t &input_circuit) {
  node_list.clear();
  net_list = &input_circuit.net_list;
  node_list.resize(input_circuit.block_list.size());
  node_dla *node;
  for (size_t i=0; i<node_list.size(); i++) {
    node = &node_list[i];
    node->retrieve_info_from_database(input_circuit.block_list[i]);
  }
  LEFT = input_circuit.LEFT;
  RIGHT = input_circuit.RIGHT;
  BOTTOM = input_circuit.BOTTOM;
  TOP = input_circuit.TOP;
}

void diffusion_limited_aggregation_placer::report_result() {
  node_dla *node;
  for (size_t i=0; i<node_list.size(); i++) {
    node = &node_list[i];
    node->write_info_to_database(circuit->block_list[i]);
  }
}

void diffusion_limited_aggregation_placer::clear() {
  circuit = nullptr;
  net_list = nullptr;
  node_list.clear();
}

void diffusion_limited_aggregation_placer::add_boundary_list() {
  int max_width=0, max_height=0;
  for (auto &&node: node_list) {
    // find maximum width and maximum height
    if (node.is_terminal) {
      continue;
    }
    if (node.w>max_width) {
      max_width = node.w;
    }
    if (node.h>max_height) {
      max_height = node.h;
    }
  }

  node_dla tmp_node;
  int multiplier = 500;
  tmp_node.is_terminal = true;

  tmp_node.w = multiplier*max_width; tmp_node.h = multiplier*max_height;
  tmp_node.x0 = LEFT - tmp_node.w/2.0; tmp_node.y0 = TOP + tmp_node.h/2.0;
  boundary_list.push_back(tmp_node);

  tmp_node.w = RIGHT - LEFT; tmp_node.h = multiplier*max_height;
  tmp_node.x0 = LEFT + tmp_node.w/2.0; tmp_node.y0 = TOP + tmp_node.h/2.0;
  boundary_list.push_back(tmp_node);

  tmp_node.w = multiplier*max_width; tmp_node.h = multiplier*max_height;
  tmp_node.x0 = RIGHT + tmp_node.w/2.0; tmp_node.y0 = TOP + tmp_node.h/2.0;
  boundary_list.push_back(tmp_node);

  tmp_node.w = multiplier*max_width; tmp_node.h = TOP - BOTTOM;
  tmp_node.x0 = RIGHT + tmp_node.w/2.0; tmp_node.y0 = BOTTOM + tmp_node.h/2.0;
  boundary_list.push_back(tmp_node);

  tmp_node.w = multiplier*max_width; tmp_node.h = multiplier*max_height;
  tmp_node.x0 = RIGHT + tmp_node.w/2.0; tmp_node.y0 = BOTTOM - tmp_node.h/2.0;
  boundary_list.push_back(tmp_node);

  tmp_node.w = RIGHT - LEFT; tmp_node.h = multiplier*max_height;
  tmp_node.x0 = LEFT + tmp_node.w/2.0; tmp_node.y0 = BOTTOM - tmp_node.h/2.0;
  boundary_list.push_back(tmp_node);

  tmp_node.w = multiplier*max_width; tmp_node.h = multiplier*max_height;
  tmp_node.x0 = LEFT - tmp_node.w/2.0; tmp_node.y0 = BOTTOM - tmp_node.h/2.0;
  boundary_list.push_back(tmp_node);

  tmp_node.w = multiplier*max_width; tmp_node.h = TOP - BOTTOM;
  tmp_node.x0 = LEFT - tmp_node.w/2.0; tmp_node.y0 = BOTTOM + tmp_node.h/2.0;
  boundary_list.push_back(tmp_node);
}

void diffusion_limited_aggregation_placer::initialize_bin_list(){
  // initialize the binlist, build up the binlist matrix
  int max_width=0, max_height=0;
  for (auto &&node: node_list) {
    // find maximum width and maximum height
    if (node.is_terminal) {
      continue;
    }
    if (node.w>max_width) {
      max_width = node.w;
    }
    if (node.h>max_height) {
      max_height = node.h;
    }
  }
  size_t x_bin_num, y_bin_num;
  x_bin_num = (size_t)std::ceil((RIGHT - LEFT + 2*max_width)/(double)max_width); // determine the bin numbers in x direction
  y_bin_num = (size_t)std::ceil((TOP - BOTTOM + 2*max_height)/(double)max_height); // determine the bin numbers in y direction
  bin_width = max_width;
  bin_height = max_height;
  bin_dla tmp_bin;
  std::vector<bin_dla> tmp_bin_column(y_bin_num);
  for (size_t x=0; x<x_bin_num; x++) {
    // binlist[x][y] indicates the bin in the  x-th x, and y-th y bin
    bin_list.push_back(tmp_bin_column);
  }
  double Left_new = LEFT - (x_bin_num * bin_width - (RIGHT - LEFT))/2.0;
  double Bottom_new = BOTTOM - (y_bin_num * bin_height - (TOP - BOTTOM))/2.0;
  for (size_t x=0; x<bin_list.size(); x++) {
    for (size_t y=0; y<bin_list[x].size(); y++) {
      bin_list[x][y].left = Left_new + x*bin_width;
      bin_list[x][y].bottom = Bottom_new + y*bin_height;
      bin_list[x][y].width = bin_width;
      bin_list[x][y].height = bin_height;
    }
  }
}

void diffusion_limited_aggregation_placer::add_neb_num(int node_num1, int node_num2, int net_size) {
  size_t neb_list_size; // neighbor number
  cell_neighbor tmp_neb; // for one of its neighbor, we need to record the primary key of this neighbor and the number of wires between them
  tmp_neb.node_num = node_num2;
  tmp_neb.wire_num = 1.0 / (net_size - 1);
  neb_list_size = node_list[node_num1].neblist.size();
  if (neb_list_size == 0) {
    node_list[node_num1].neblist.push_back(tmp_neb);
  } else {
    for (size_t j = 0; j < node_list[node_num1].neblist.size(); j++) {
      if (node_list[node_num1].neblist[j].node_num != node_num2) {
        if (j == neb_list_size - 1) {
          node_list[node_num1].neblist.push_back(tmp_neb);
          break;
        } else {
          continue;
        }
      } else {
        node_list[node_num1].neblist[j].wire_num += tmp_neb.wire_num;
        break;
      }
    }
  }
}

void diffusion_limited_aggregation_placer::sort_neighbor_list() {
  int max_wire_node_index;
  cell_neighbor tmp_neb;
  for (auto &&node: node_list) {
    for (size_t i=0; i<node.neblist.size(); i++) {
      max_wire_node_index = i;
      for (size_t j=i+1; j<node.neblist.size(); j++) {
        if (node.neblist[j].wire_num > node.neblist[max_wire_node_index].wire_num) {
          max_wire_node_index = j;
        }
      }
      tmp_neb = node.neblist[i];
      node.neblist[i] = node.neblist[max_wire_node_index];
      node.neblist[max_wire_node_index] = tmp_neb;
    }
  }
}

void diffusion_limited_aggregation_placer::update_neighbor_list() {
  // update the list of neighbor and the wire numbers connected to its neighbor, and the nets connected to this cell
  int cell1, cell2;
  size_t net_size;
  for (auto &&net: *net_list) {
    for (auto &&pin: net.pinlist) {
      cell1 = pin.pinnum;
      node_list[cell1].net.push_back(net.net_num);
      node_list[cell1].totalwire += 1;
    }
    net_size = net.pinlist.size();
    for (size_t i=0; i<net_size; i++) {
      cell1 = net.pinlist[i].pinnum;
      for (size_t j=i+1; j<net.pinlist.size(); j++) {
        cell2 = net.pinlist[j].pinnum;
        add_neb_num(cell1, cell2, net_size);
        add_neb_num(cell2, cell1, net_size);
      }
    }
  }
  sort_neighbor_list();
}

void diffusion_limited_aggregation_placer::order_node_to_place(std::queue<int> &cell_to_place){
  // creat a new cell list cell_to_place to store the sorted node_list based on the number of wires connecting to the cell
  node_dla tmp_node;
  std::vector<node_dla> tmp_node_list;
  for (auto &&node: node_list) {
    tmp_node_list.push_back(node);
  }
  int max_wire_cell;
  for (size_t i=0; i<tmp_node_list.size(); i++) {
    max_wire_cell = i;
    for (size_t j=i+1; j<tmp_node_list.size(); j++) {
      if (tmp_node_list[j].totalwire > tmp_node_list[max_wire_cell].totalwire) {
        max_wire_cell = j;
      }
    }
    tmp_node = tmp_node_list[i];
    tmp_node_list[i] = tmp_node_list[max_wire_cell];
    tmp_node_list[max_wire_cell] = tmp_node;
  }
  for (auto &&node: tmp_node_list) {
    std::cout << node.node_num << " is connected to " << node.totalwire << "\n";
    cell_to_place.push(node.node_num); // the queue of sorted cell list
  }
}

void diffusion_limited_aggregation_placer::update_bin_list(int first_node_num, std::vector<int> &cell_out_bin) {
  double left_most = bin_list[0][0].left, bottom_most = bin_list[0][0].bottom;
  int X_bin_list_size = bin_list.size(), Y_bin_list_size = bin_list[0].size();
  double right_most = left_most + X_bin_list_size * bin_width, top_most = bottom_most + Y_bin_list_size * bin_height;
  double left = node_list[first_node_num].llx(), right = node_list[first_node_num].urx();
  double bottom = node_list[first_node_num].lly(), top = node_list[first_node_num].ury();
  node_dla temp_large_boundry; // the larger boundries of bins
  temp_large_boundry.x0 = left_most;
  temp_large_boundry.y0 = bottom_most;
  temp_large_boundry.w = (int)(right_most - left_most);
  temp_large_boundry.h = (int)(top_most - bottom_most);

  //cout << "Cell: " << first_node_num << " is in bins: ";
  if (temp_large_boundry.overlap_area(node_list[first_node_num]) < node_list[first_node_num].area()) {
    cell_out_bin.push_back(first_node_num);
    //cout << "-1 -1; ";
  }
  int L,B,R,T; // left, bottom, right, top of the cell in which bin
  L = floor((left - left_most)/bin_width);
  B = floor((bottom - bottom_most)/bin_height);
  R = floor((right - left_most)/bin_width);
  T = floor((top - bottom_most)/bin_height);
  for (int j=B; j<=T; j++) {
    if ((j>=0)&&(j<X_bin_list_size))
    {
      for (int k=L; k<=R; k++)
      {
        if ((k>=0)&&(k<Y_bin_list_size))
        {
          bin_list[j][k].CIB.push_back(first_node_num);
          //cout << j << " " << k << "; ";
          // tempbinloc.iloc = j;
          // tempbinloc.jloc = k;
          // node_list[first_node_num].bin.push_back(tempbinloc);
        }
      }
    }
  }
  //cout << "\n";
}

bool diffusion_limited_aggregation_placer::random_release_from_boundaries(int boundary_num, node_dla &node) {
  switch (boundary_num) {
    case 0:
      // 2*Left boundry
      node.x0 = LEFT - 1*(RIGHT - LEFT);
      node.y0 = BOTTOM + std::rand()%(TOP - BOTTOM);
      break;
    case 1:
      // 2*Top boundry
      node.x0 = LEFT + std::rand()%(RIGHT - LEFT);
      node.y0 = TOP + 1*(TOP - BOTTOM);
      break;
    case 2:
      // 2*Right boundry
      node.x0 = RIGHT + 1*(RIGHT - LEFT);
      node.y0 = BOTTOM + std::rand()%(TOP - BOTTOM);
      break;
    case 3:
      // 2*Bottom boundry
      node.x0 = LEFT + std::rand()%(RIGHT - LEFT);
      node.y0 = BOTTOM - 1*(TOP - BOTTOM);
      break;
    default:
      return false;
  }
  return true;
}

double diffusion_limited_aggregation_placer::net_hwpl_during_dla(net_t *net) {
  double WL=0;
  int num_of_placed_cell_in_net=0, tempnetsize, tempcellNo;
  double tempcellx, tempcelly;
  double maxx = 0, minx = 0, maxy = 0, miny = 0;
  tempnetsize = net->pinlist.size();
  for (int j=0; j<tempnetsize; j++) {
    tempcellNo = net->pinlist[j].pinnum;
    if (node_list[tempcellNo].placed==1) {
      num_of_placed_cell_in_net += 1;
      //cout << Net.PinList[j].cellNo << "\n";
      //cout << Net.PinList[j].x_off << " " << Net.PinList[j].y_off << "\t" << node_list[tempcellNo].left << " " << node_list[tempcellNo].bottom << "\n";
      tempcellx = node_list[tempcellNo].x0 + net->pinlist[j].xoffset;
      tempcelly = node_list[tempcellNo].y0 + net->pinlist[j].yoffset;
      if ( num_of_placed_cell_in_net == 1 ) {
        maxx = tempcellx; minx = tempcellx; maxy = tempcelly; miny = tempcelly;
      }
      else {
        if ( tempcellx > maxx ) maxx = tempcellx;
        if ( tempcellx < minx ) minx = tempcellx;
        if ( tempcelly > maxy ) maxy = tempcelly;
        if ( tempcelly < maxy ) miny = tempcelly;
      }
    }
  }
  WL = ( maxx - minx ) + ( maxy - miny );
  //cout << maxx << " " << minx << " " << maxy << " " << miny << "\n\n";
  return WL;
}

double diffusion_limited_aggregation_placer::wirelength_during_DLA(int first_node_num) {
  // support multi-pin nets
  double WL=0;
  net_t *net;
  for (size_t i=0; i<node_list[first_node_num].net.size(); i++) {
    //cout << "Net " << node_list[first_node_num].net[i] << "\n";
    net = &(net_list->at(node_list[first_node_num].net[i]));
    WL += net_hwpl_during_dla(net);
  }
  return WL;
}

int diffusion_limited_aggregation_placer::is_legal(int first_node_num, std::vector<int> &cell_out_bin) {
  // if legal, return -1, if illegal, return the cell with which this first_node_num has overlap
  double left_most = bin_list[0][0].left, bottom_most = bin_list[0][0].bottom;
  int binwidth = bin_list[0][0].width, binheight = bin_list[0][0].height;
  int Ybinlistsize = bin_list.size(), Xbinlistsize = bin_list[0].size();
  double right_most = left_most + Xbinlistsize*binwidth, top_most = bottom_most + Ybinlistsize*binheight;
  double Left = node_list[first_node_num].llx(), Right = node_list[first_node_num].urx();
  double Bottom = node_list[first_node_num].lly(), Top = node_list[first_node_num].ury();
  node_dla temp_large_boundry; // the larger boundries of bins
  temp_large_boundry.x0 = left_most;
  temp_large_boundry.y0 = bottom_most;
  temp_large_boundry.w = (int)(right_most - left_most);
  temp_large_boundry.h = (int)(top_most - bottom_most);

  int tempcellnum;
  if (temp_large_boundry.overlap_area(node_list[first_node_num]) < node_list[first_node_num].area()) {
    for (size_t i=0; i<cell_out_bin.size(); i++) {
      tempcellnum = cell_out_bin[i];
      if ((tempcellnum!=first_node_num)&&(node_list[first_node_num].overlap_area(node_list[tempcellnum])>=1)) return tempcellnum;
    }
  }
  int L,B,R,T; // left, bottom, right, top of the cell in which bin
  L = floor((Left - left_most)/binwidth);
  B = floor((Bottom - bottom_most)/binheight);
  R = floor((Right - left_most)/binwidth);
  T = floor((Top - bottom_most)/binheight);
  for (int j=B; j<=T; j++) {
    if ((j>=0)&&(j<Ybinlistsize)) {
      for (int k=L; k<=R; k++) {
        if ((k>=0)&&(k<Xbinlistsize)) {
          for (size_t i=0; i<bin_list[j][k].CIB.size(); i++) {
            tempcellnum = bin_list[j][k].CIB[i];
            if ((tempcellnum!=first_node_num)&&(node_list[first_node_num].overlap_area(node_list[tempcellnum])>=1)) return tempcellnum;
          }
        }
      }
    }
  }
  return -1;
}

void diffusion_limited_aggregation_placer::diffuse(int first_node_num, std::vector<int> &cell_out_bin) {
  double WL1, WL2, MW; // MW indicates minimum wirelength
  double modWL1, modWL2;
  int step;
  double center_x=(LEFT + RIGHT)/2.0, center_y=(BOTTOM + TOP)/2.0;
  double cell_center_x, cell_center_y, distance_to_center;
  double p1=0.00005, p2; // accept probability
  double r, T=50.0;
  node_dla TLUM; // Temporary Location Under Motion
  node_dla MWLL; // Minimum Wire-Length Location
  random_release_from_boundaries(0, node_list[first_node_num]);
  MW = wirelength_during_DLA(first_node_num);
  for (int i=0; i<4; i++) {
    // release this cell from all 4 boundries and find the location, which gives the smallest total wirelength
    random_release_from_boundaries(i, node_list[first_node_num]);
    while (true) {
      cell_center_x = node_list[first_node_num].x0;
      cell_center_y = node_list[first_node_num].y0;
      distance_to_center = fabs(cell_center_x - center_x)+fabs(cell_center_y - center_y);
      WL1 = wirelength_during_DLA(first_node_num);
      modWL1 = WL1 + distance_to_center/4.0;
      TLUM = node_list[first_node_num];
      step = (int)(WL1/100.0);
      node_list[first_node_num].random_move(std::max(step, 1));
      cell_center_x = node_list[first_node_num].x0;
      cell_center_y = node_list[first_node_num].y0;
      distance_to_center = fabs(cell_center_x - center_x)+fabs(cell_center_y - center_y);
      WL2 = wirelength_during_DLA(first_node_num);
      modWL2 = WL2 + distance_to_center/4.0;
      if (is_legal(first_node_num, cell_out_bin)==-1) {
        if (modWL2 > modWL1) {
          // if wire-length becomes larger, reject this move by some probability
          r = ((double) std::rand()/RAND_MAX);
          p2 = exp(-(modWL2-modWL1)/T);
          if (r>p2) node_list[first_node_num] = TLUM;
        }
        if (WL2<=MW)
        {
          MW = WL2;
          MWLL = node_list[first_node_num];
        }
      }
      else
      {
        node_list[first_node_num] = TLUM;
        r = ((double)std::rand()/RAND_MAX);
        if (r<=p1) break;
      }
    }
  }
  node_list[first_node_num] = MWLL;
  /*
  cout << first_node_num << " " << MWLL.cellNo << "\n";
  for (int j=0; j<node_list.size(); j++)
  {
    if (node_list[j].cellNo == -1)
    {
      cout << j << " errordiffuse\n";
      exit(1);
    }
  }
  */
  update_bin_list(first_node_num, cell_out_bin);
}

bool diffusion_limited_aggregation_placer::DLA() {
  update_neighbor_list();
  std::queue<int> cell_to_place;
  std::vector<int> cell_out_bin; // cells which are out of bins
  order_node_to_place(cell_to_place);

  std::queue<int> Q_place;
  int first_node_num, tmp_num;
  first_node_num = cell_to_place.front();
  cell_to_place.pop();
  Q_place.push(first_node_num);
  node_list[first_node_num].queued = true;
  int num_of_node_placed = 0;
  while (!Q_place.empty()) {
    std::cout << Q_place.size() << "\n";
    first_node_num = Q_place.front();
    Q_place.pop();
    std::cout << "Placing cell " << first_node_num << "\n";
    for (size_t j=0; j<node_list[first_node_num].neblist.size(); j++) {
      tmp_num = node_list[first_node_num].neblist[j].node_num;
      if (node_list[tmp_num].queued){
        continue;
      } else {
        node_list[tmp_num].queued = true;
        Q_place.push(tmp_num);
      }
    }
    node_list[first_node_num].placed = true;
    if (num_of_node_placed == 0) {
      node_list[first_node_num].x0 = (LEFT + RIGHT)/2.0;
      node_list[first_node_num].y0 = (BOTTOM + TOP)/2.0;
      update_bin_list(first_node_num, cell_out_bin);
    }
    else {
      diffuse(first_node_num, cell_out_bin);
    }
    num_of_node_placed += 1;
    //drawplaced(node_list, NetList, boundries);
    //if (numofcellplaced==2) break;
    if (Q_place.empty()) {
      while (!cell_to_place.empty()) {
        first_node_num = cell_to_place.front();
        if (!node_list[first_node_num].queued) {
          Q_place.push(first_node_num);
          break;
        } else {
          cell_to_place.pop();
        }
      }
    }
  }
  //std::cout << "Total WireLength after placement is " <<  TotalWireLength(node_list, NetList) << "\n";
  return true;
}

bool diffusion_limited_aggregation_placer::place() {
  add_boundary_list();
  std::cout << "bug here\n";
  initialize_bin_list();
  DLA();
  return true;
}