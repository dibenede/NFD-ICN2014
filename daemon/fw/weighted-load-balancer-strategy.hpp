/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014  Regents of the University of California,
 *                     Arizona Board of Regents,
 *                     Colorado State University,
 *                     University Pierre & Marie Curie, Sorbonne University,
 *                     Washington University in St. Louis,
 *                     Beijing Institute of Technology
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#ifndef NFD_DAEMON_FW_WEIGHTED_LOAD_BALANCER_STRATEGY_HPP
#define NFD_DAEMON_FW_WEIGHTED_LOAD_BALANCER_STRATEGY_HPP

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/identity.hpp>
#include <boost/multi_index/mem_fun.hpp>

#include <boost/random/mersenne_twister.hpp>

#include <ndn-cxx/util/time.hpp>

#include "strategy.hpp"

namespace nfd {
namespace fw {

struct MyMeasurementInfo;

class WeightedLoadBalancerStrategy : public Strategy
{
public:
  WeightedLoadBalancerStrategy(Forwarder& forwarder,
                               const Name& name = STRATEGY_NAME);

  virtual
  ~WeightedLoadBalancerStrategy();

  virtual void
  afterReceiveInterest(const Face& inFace,
                       const Interest& interest,
                       shared_ptr<fib::Entry> fibEntry,
                       shared_ptr<pit::Entry> pitEntry);

  virtual void
  beforeSatisfyPendingInterest(shared_ptr<pit::Entry> pitEntry,
                               const Face& inFace,
                               const Data& data);


protected:

  shared_ptr<MyMeasurementInfo>
  getOrCreateMyMeasurementInfo(const shared_ptr<fib::Entry>& entry);


public:
  static const Name STRATEGY_NAME;

protected:
  boost::random::mt19937 m_randomGenerator;
};

} // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_WEIGHTED_LOAD_BALANCER_HPP
