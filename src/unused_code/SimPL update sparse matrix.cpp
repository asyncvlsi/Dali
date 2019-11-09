void GPSimPL::BuildProblemB2B(bool is_x_direction, Eigen::VectorXd &b) {
  std::vector<Block> &block_list = *BlockList();
  size_t coefficients_capacity = coefficients.capacity();
  coefficients.resize(0);
  for (long int i = 0; i < b.size(); i++) {
    b[i] = 0;
  }
  double weight, inv_p, pin_loc0, pin_loc1, offset_diff;
  int blk_num0, blk_num1, max_pin_index, min_pin_index;
  bool is_movable0, is_movable1;
  if (is_x_direction) {
    for (auto &&net: circuit_->net_list) {
      if (net.P() <= 1) continue;
      inv_p = net.InvP();
      net.UpdateMaxMinX();
      max_pin_index = net.MaxBlkPinNumX();
      min_pin_index = net.MinBlkPinNumX();
      AddMatrixElement(net, max_pin_index, min_pin_index);
      for (int i = 0; i < int(net.blk_pin_list.size()); ++i) {
        if (i==max_pin_index || i==min_pin_index) continue;
        AddMatrixElement(net, i, min_pin_index);
        AddMatrixElement(net, i, max_pin_index);
      }
    }
    std::sort(coefficients.begin(), coefficients.end(), [](T& t1, T& t2) {
      return (t1.row() < t2.row() || (t1.row() == t2.row() && t1.col() < t2.col()) || (t1.row() == t2.row() && t1.col() == t2.col() && t1.value() < t2.value()));
    });