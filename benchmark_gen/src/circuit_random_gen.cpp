//
// Created by Yihang Yang on 2019-05-17.
//

#include <vector>
#include <random>
#include <cstdlib>

void circuit_t::random_gen_node_list(int w_lower, int w_upper, int h_lower, int h_upper, int cell_num) {
  std::default_random_engine w_generator;
  std::uniform_int_distribution<int> w_distribution(w_lower, w_upper); // width range is 20~80
  std::default_random_engine h_generator;
  std::uniform_int_distribution<int> h_distribution(h_lower, h_upper); // height range is 20~40

  int total_area = 0;
  double ave_height = 0;
  node_t tmp_node;
  for (int i=0; i<cell_num; i++) {
    tmp_node.is_terminal = false;
    tmp_node.w = w_distribution(w_generator);
    tmp_node.h = h_distribution(h_generator);
    tmp_node.node_num = i;
    total_area += tmp_node.area();
    ave_height += tmp_node.h;
    Nodelist.push_back(tmp_node);
  }
  ave_height = ave_height/cell_num;

  int IO_width, Length_of_Space;
  Length_of_Space = (int) ceil(sqrt(total_area*1.5)/10.0)*10;
  IO_width = ((int) (ave_height / 10.0)) * 10;
  LEFT = IO_width;
  RIGHT = Length_of_Space + IO_width;
  BOTTOM = IO_width;
  TOP = Length_of_Space + IO_width;
  //std::srand(time(NULL));
  for (auto node: Nodelist) {
    node.x0 = LEFT + node.w/2.0;
    node.y0 = BOTTOM + node.h/2.0;
  }
}

void circuit_t::random_gen_net_list() {
  net_t tmp_net;
  pininfo tmp_pin;
  int netNum = 0;
  int rand_num;
  for (size_t i = 0; i < Nodelist.size(); i++) {
    if (i % 2) {
      rand_num = std::rand() % 2;
      if (std::rand() % 2 == 1) {
        for (int j = 0; j < 4; j++) {
          tmp_net.pinlist.clear();
          tmp_pin.pinnum = i - 1;
          tmp_pin.xoffset = (j+1) + Nodelist[i-1].w/2.0 - 2;
          if (rand_num % 2 == 1) {
            tmp_pin.yoffset = 0;
          } else {
            tmp_pin.yoffset = Nodelist[i-1].h;
          }
          tmp_net.pinlist.push_back(tmp_pin);
          tmp_pin.pinnum = i;
          tmp_pin.xoffset = (j+1) + Nodelist[i].w/2.0 - 2;
          if (rand_num % 2) {
            tmp_pin.yoffset = Nodelist[i].h;
          } else {
            tmp_pin.yoffset = 0;
          }
          tmp_net.pinlist.push_back(tmp_pin);
          tmp_net.net_num = netNum;
          Netlist.push_back(tmp_net);
          netNum++;
        }
      } else {
        for (int j = 0; j < 4; j++) {
          tmp_net.pinlist.clear();
          tmp_pin.pinnum = i-1;
          if (rand_num % 2) {
            tmp_pin.xoffset = 0;
          } else {
            tmp_pin.xoffset = Nodelist[i-1].w;
          }
          tmp_pin.yoffset = (j+1) + Nodelist[i-1].h/2.0 - 2;
          tmp_net.pinlist.push_back(tmp_pin);
          tmp_pin.pinnum = i;
          if (rand_num % 2) {
            tmp_pin.xoffset = Nodelist[i].w;
          } else {
            tmp_pin.xoffset = 0;
          }
          tmp_pin.yoffset = (j+1) + Nodelist[i].h/2.0 - 2;
          tmp_net.pinlist.push_back(tmp_pin);
          tmp_net.net_num = netNum;
          Netlist.push_back(tmp_net);
          netNum++;
        }
      }
    }
  }

  std::vector<int> capacity(Nodelist.size()); // number of pins for each cell
  std::vector<int> usage(Nodelist.size());
  for (size_t i = 0; i < Nodelist.size(); i++) {
    capacity[i] = (std::rand() % 4 + 1) * 5;
    usage[i] = 4;
  }
  int rand_node_num;
  for (size_t i = 0; i < Nodelist.size() - 1; i++) {
    if (usage[i] >= capacity[i]) continue;
    for (int j = 0; (usage[i] < capacity[i]) && (j < 20); j++) {
      rand_num = std::rand() % 2;
      rand_node_num = std::rand() % (Nodelist.size() - i - 1) + i + 1;
      if (std::rand() % 2) {
        tmp_net.pinlist.clear();
        tmp_pin.pinnum = i;
        tmp_pin.xoffset = std::rand() % Nodelist[i].w;
        if (rand_num % 2) {
          tmp_pin.yoffset = 0;
        } else {
          tmp_pin.yoffset = Nodelist[i].h;
        }
        tmp_net.pinlist.push_back(tmp_pin);
        if (usage[rand_node_num] >= capacity[rand_node_num]){
          continue;
        }
        tmp_pin.pinnum = rand_node_num;
        tmp_pin.xoffset = std::rand() % Nodelist[rand_node_num].w;
        if (rand_num % 2) {
          tmp_pin.yoffset = Nodelist[rand_node_num].h;
        } else {
          tmp_pin.yoffset = 0;
        }
        tmp_net.pinlist.push_back(tmp_pin);
        tmp_net.net_num = netNum;
        Netlist.push_back(tmp_net);
        netNum++;
        usage[i]++;
        usage[rand_node_num]++;
      } else {
        tmp_net.pinlist.clear();
        tmp_pin.pinnum = i;
        rand_num = std::rand() % 2;
        if (rand_num % 2) {
          tmp_pin.xoffset = 0;
        } else {
          tmp_pin.xoffset = Nodelist[i].w;
        }
        tmp_pin.yoffset = std::rand() % Nodelist[i].h;
        tmp_net.pinlist.push_back(tmp_pin);
        if (usage[rand_node_num] >= capacity[rand_node_num]) {
          continue;
        }
        tmp_pin.pinnum = rand_node_num;
        if (rand_num % 2) {
          tmp_pin.xoffset = Nodelist[rand_node_num].w;
        } else {
          tmp_pin.xoffset = 0;
        }
        tmp_pin.yoffset = std::rand() % Nodelist[rand_node_num].h;
        tmp_net.pinlist.push_back(tmp_pin);
        tmp_net.net_num = netNum;
        Netlist.push_back(tmp_net);
        netNum++;
        usage[i]++;
        usage[rand_node_num]++;
      }
    }
  }

  size_t max_pin_num_index;
  size_t max_pin_num;
  for (auto &&net: Netlist) {
    for (size_t i=0; i<net.pinlist.size(); i++) {
      max_pin_num_index = i;
      max_pin_num = net.pinlist[i].pinnum;
      for (size_t j=i+1; j<net.pinlist.size(); j++) {
        if (net.pinlist[j].pinnum > max_pin_num) {
          max_pin_num = net.pinlist[j].pinnum;
          max_pin_num_index = j;
        }
      }

      tmp_pin = net.pinlist[i];
      net.pinlist[i] = net.pinlist[max_pin_num_index];
      net.pinlist[max_pin_num_index] = tmp_pin;
    }
  }
}