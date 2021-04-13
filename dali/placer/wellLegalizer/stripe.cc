//
// Created by Yihang Yang on 4/12/21.
//

#include "stripe.h"

#include <algorithm>
#include <climits>

namespace dali {

Stripe *ClusterStripe::GetStripeMatchSeg(SegI seg, int y_loc) {
  Stripe *res = nullptr;
  for (auto &&Stripe: stripe_list_) {
    if ((Stripe.URY() == y_loc) && (Stripe.LLX() == seg.lo) && (Stripe.URX() == seg.hi)) {
      res = &Stripe;
      break;
    }
  }
  return res;
}

Stripe *ClusterStripe::GetStripeMatchBlk(Block *blk_ptr) {
  Stripe *res = nullptr;
  double center_x = blk_ptr->X();
  double center_y = blk_ptr->Y();
  for (auto &&Stripe: stripe_list_) {
    if ((Stripe.LLY() <= center_y) &&
        (Stripe.URY() > center_y) &&
        (Stripe.LLX() <= center_x) &&
        (Stripe.URX() > center_x)) {
      res = &Stripe;
      break;
    }
  }
  return res;
}

Stripe *ClusterStripe::GetStripeClosestToBlk(Block *blk_ptr, double &distance) {
  Stripe *res = nullptr;
  double center_x = blk_ptr->X();
  double center_y = blk_ptr->Y();
  double min_distance = DBL_MAX;
  for (auto &&Stripe: stripe_list_) {
    double tmp_distance;
    if ((Stripe.LLY() <= center_y) &&
        (Stripe.URY() > center_y) &&
        (Stripe.LLX() <= center_x) &&
        (Stripe.URX() > center_x)) {
      res = &Stripe;
      tmp_distance = 0;
    } else if ((Stripe.LLX() <= center_x) && (Stripe.URX() > center_x)) {
      tmp_distance = std::min(std::abs(center_y - Stripe.LLY()), std::abs(center_y - Stripe.URY()));
    } else if ((Stripe.LLY() <= center_y) && (Stripe.URY() > center_y)) {
      tmp_distance = std::min(std::abs(center_x - Stripe.LLX()), std::abs(center_x - Stripe.URX()));
    } else {
      tmp_distance = std::min(std::abs(center_x - Stripe.LLX()), std::abs(center_x - Stripe.URX()))
          + std::min(std::abs(center_y - Stripe.LLY()), std::abs(center_y - Stripe.URY()));
    }
    if (tmp_distance < min_distance) {
      min_distance = tmp_distance;
      res = &Stripe;
    }
  }

  distance = min_distance;
  return res;
}

void ClusterStripe::AssignBlockToSimpleStripe() {
  for (auto &Stripe: stripe_list_) {
    Stripe.block_count_ = 0;
    Stripe.block_list_.clear();
  }

  for (auto &blk_ptr: block_list_) {
    double tmp_dist;
    auto Stripe = GetStripeClosestToBlk(blk_ptr, tmp_dist);
    Stripe->block_count_++;
  }

  for (auto &Stripe: stripe_list_) {
    Stripe.block_list_.reserve(Stripe.block_count_);
  }

  for (auto &blk_ptr: block_list_) {
    double tmp_dist;
    auto Stripe = GetStripeClosestToBlk(blk_ptr, tmp_dist);
    Stripe->block_list_.push_back(blk_ptr);
  }
}

}