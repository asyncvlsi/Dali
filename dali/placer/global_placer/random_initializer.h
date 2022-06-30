/*******************************************************************************
 *
 * Copyright (c) 2022 Yihang Yang
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 ******************************************************************************/
#ifndef DALI_PLACER_GLOBAL_PLACER_RANDOM_INITIALIZER_H_
#define DALI_PLACER_GLOBAL_PLACER_RANDOM_INITIALIZER_H_

#include <unordered_map>
#include <string>

#include "dali/circuit/circuit.h"
#include "dali/common/elapsed_time.h"

namespace dali {

class RandomInitializer {
 public:
  RandomInitializer(
      Circuit *ckt_ptr,
      int num_threads,
      unsigned int random_seed
  );
  virtual ~RandomInitializer() = default;
  virtual void SetParameters(
      std::unordered_map<std::string, std::string> &params_dict
  );
  virtual void RandomPlace() = 0;
  virtual void PrintStartStatement();
  virtual void PrintEndStatement();
 protected:
  Circuit *ckt_ptr_ = nullptr;
  int num_threads_ = 1;
  unsigned int random_seed_ = 1;

  // cut the block list in to a fixed number of chunks, and perform location
  // initialization in parallel
  int num_chunks_ = 1;

  // save intermediate result for debugging and/or visualization
  bool should_save_intermediate_result_ = false;

  ElapsedTime elapsed_time_;
};

class UniformInitializer : public RandomInitializer {
 public:
  UniformInitializer(
      Circuit *ckt_ptr,
      int num_threads,
      unsigned int random_seed
  ) : RandomInitializer(ckt_ptr, num_threads, random_seed) {}
  ~UniformInitializer() override = default;
  void RandomPlace() override;
  void PrintEndStatement() override;
 private:

};

class NormalInitializer : public RandomInitializer {
 public:
  NormalInitializer(
      Circuit *ckt_ptr,
      int num_threads,
      unsigned int random_seed
  ) : RandomInitializer(ckt_ptr, num_threads, random_seed) {}
  ~NormalInitializer() override = default;
  void SetParameters(
      std::unordered_map<std::string, std::string> &params_dict
  ) override;
  void RandomPlace() override;
  void PrintEndStatement() override;
 protected:
  double std_dev_ = 1.0/3.0;
};

class MonteCarloInitializer : public RandomInitializer {
 public:
  MonteCarloInitializer(
      Circuit *ckt_ptr,
      int num_threads,
      unsigned int random_seed
  ) : RandomInitializer(ckt_ptr, num_threads, random_seed) {}
  ~MonteCarloInitializer() override = default;
  void RandomPlace() override;
};

} // dali

#endif //DALI_PLACER_GLOBAL_PLACER_RANDOM_INITIALIZER_H_
