// Copyright (c) 2011-2016 The Cryptonote developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <unordered_map>
#include <random>

template <typename T, typename Gen>
class ShuffleGenerator {
public:

  ShuffleGenerator(T n, const Gen& gen = Gen()) :
    N(n), generator(gen), count(n) {}

  T operator()() {

    if (count == 0) {
      throw std::runtime_error("shuffle sequence ended");
    }

    typedef typename std::uniform_int_distribution<T> distr_t;
    typedef typename distr_t::param_type param_t;

    distr_t distr;
    
    T value = distr(generator, param_t(0, --count));

    auto rvalIt = selected.find(count);
    auto rval = rvalIt != selected.end() ? rvalIt->second : count;

    auto lvalIt = selected.find(value);

    if (lvalIt != selected.end()) {
      value = lvalIt->second;
      lvalIt->second = rval;
    } else {
      selected[value] = rval;
    }

    return value;
  }

  void reset() {
    count = N;
    selected.clear();
  }

private:

  std::unordered_map<T, T> selected;
  T count;
  const T N;
  Gen generator;
};
