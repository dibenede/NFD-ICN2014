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

#include "simple-load-balancer-strategy.hpp"

#include <ndn-cxx/util/random.hpp>

#include <core/logger.hpp>

NFD_LOG_INIT("SimpleLoadBalancerStrategy");

namespace nfd {
namespace fw {


const Name SimpleLoadBalancerStrategy::STRATEGY_NAME("ndn:/localhost/nfd/strategy/simple-load-balancer");

SimpleLoadBalancerStrategy::SimpleLoadBalancerStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder, name)
{
}

SimpleLoadBalancerStrategy::~SimpleLoadBalancerStrategy()
{
}

static inline bool
canForwardToNextHop(shared_ptr<pit::Entry> pitEntry,
                    const fib::NextHop& nexthop)
{
  return pitEntry->canForwardTo(*nexthop.getFace());
}

void
SimpleLoadBalancerStrategy::afterReceiveInterest(const Face& inFace,
                                                 const Interest& interest,
                                                 shared_ptr<fib::Entry> fibEntry,
                                                 shared_ptr<pit::Entry> pitEntry)
{
  if (pitEntry->hasUnexpiredOutRecords())
    {
      // not a new Interest, don't forward
      return;
    }

  const fib::NextHopList& nexthops = fibEntry->getNextHops();

  fib::NextHopList::const_iterator it = std::find_if(nexthops.begin(), nexthops.end(),
    bind(&canForwardToNextHop, pitEntry, _1));

  if (it == nexthops.end())
    {
      this->rejectPendingInterest(pitEntry);
      return;
    }

  // There is at least 1 face we can use to forward this Interest

  do
    {
      boost::random::uniform_int_distribution<> dist(0, nexthops.size() - 1);
      const size_t randomIndex = dist(m_randomGenerator);

      it = nexthops.begin();
      uint64_t currentIndex = 0;

      for (; it != nexthops.end() && currentIndex != randomIndex;
           ++it, ++currentIndex)
        {
        }
    } while (!canForwardToNextHop(pitEntry, *it));

  this->sendInterest(pitEntry, it->getFace());
}

} // namespace fw
} // namespace nfd
