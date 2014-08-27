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

#include <ndn-cxx/util/random.hpp>

#include "weighted-load-balancer-strategy.hpp"
#include "core/logger.hpp"
#include "table/measurements-entry.hpp"



namespace nfd {
namespace fw {

NFD_LOG_INIT("WeightedLoadBalancerStrategy");

int WeightedLoadBalancerStrategy::MeasurementsEntryInfo::ID = 0;

const Name WeightedLoadBalancerStrategy::STRATEGY_NAME("ndn:/localhost/nfd/strategy/weighted-load-balancer");

WeightedLoadBalancerStrategy::WeightedLoadBalancerStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder, name)
{

}

WeightedLoadBalancerStrategy::~WeightedLoadBalancerStrategy()
{
}



void
WeightedLoadBalancerStrategy::afterReceiveInterest(const Face& inFace,
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

  shared_ptr<MeasurementsEntryInfo> measurementsEntryInfo =
    getOrCreateMeasurementsEntryInfo<fib::Entry>(fibEntry);

  NFD_LOG_TRACE("afterReceive: Accessing Measurements Info ID: " << measurementsEntryInfo->id);

  // begin set reconciliation
  {
    MeasurementsEntryInfo::WeightedFaceSetByFaceId& facesById =
      measurementsEntryInfo->weightedFaces.get<MeasurementsEntryInfo::ByFaceId>();

    std::set<FaceId> nexthopFaceIds;

    for (fib::NextHopList::const_iterator i = nexthops.begin();
         i != nexthops.end();
         ++i)
      {
        const FaceId id = i->getFace()->getId();
        if (facesById.find(id) == facesById.end())
          {
            // new nexthop, add to set
            NFD_LOG_TRACE("adding FaceId: " << i->getFace()->getId() << " with delay 0");
            facesById.insert(WeightedFace(*i->getFace()));
          }
        nexthopFaceIds.insert(id);
      }

    for (MeasurementsEntryInfo::WeightedFaceSetByFaceId::const_iterator i = facesById.begin();
         i != facesById.end();
         ++i)
      {
        if (nexthopFaceIds.find(i->getId()) == nexthopFaceIds.end())
          {
            NFD_LOG_TRACE("pruning FaceId: " << i->getId());
            facesById.erase(i);
          }
      }

  } //end set reconciliation

  const MeasurementsEntryInfo::WeightedFaceSetByDelay& facesByDelay =
    measurementsEntryInfo->weightedFaces.get<MeasurementsEntryInfo::ByDelay>();

  NFD_LOG_TRACE(facesByDelay.size() << " Faces in weighted face set");

  const ndn::time::nanoseconds totalDelay(measurementsEntryInfo->totalDelay);
  ndn::time::nanoseconds inverseTotalDelay(0);

  for (MeasurementsEntryInfo::WeightedFaceSetByDelay::const_iterator i = facesByDelay.begin();
       i != facesByDelay.end();
       ++i)
    {
      inverseTotalDelay += (totalDelay - i->lastDelay);
    }

  boost::random::uniform_int_distribution<> dist(0, inverseTotalDelay.count());
  const uint64_t rnd = dist(m_randomGenerator);

  uint64_t cumulativeWeight = 0;

  NFD_LOG_TRACE("Generated rnd = " << rnd);

  for (MeasurementsEntryInfo::WeightedFaceSetByDelay::const_iterator i = facesByDelay.begin();
       i != facesByDelay.end();
       ++i)
    {
      const uint64_t weight = totalDelay.count() - i->lastDelay.count();
      cumulativeWeight += weight;

      NFD_LOG_TRACE("FaceId: " << i->face.getId() << " cumulativeWeight: " << cumulativeWeight);

      if(rnd <= cumulativeWeight && pitEntry->canForwardTo(i->face))
        {
          NFD_LOG_TRACE("Forwarding " << interest.getName()
                        << " out FaceId " << i->face.getId());
          this->sendInterest(pitEntry, this->getFace(i->face.getId()));
          return;
        }
    }

  NFD_LOG_WARN("rejecting Interest " << interest.getName());
  this->rejectPendingInterest(pitEntry);
}



void
WeightedLoadBalancerStrategy::beforeSatisfyPendingInterest(shared_ptr<pit::Entry> pitEntry,
                                                           const Face& inFace,
                                                           const Data& data)
{
  using namespace ndn::time;

  static const nanoseconds MEASUREMENTS_LIFETIME = time::seconds(16);

  shared_ptr<PitEntryInfo> pitInfo =
    pitEntry->getOrCreateStrategyInfo<PitEntryInfo>();

  const nanoseconds delay = SystemClock::now() - pitInfo->creationTime;

  // NFD_LOG_TRACE("PIT Info created at " << pitInfo->creationTime
  //               << " Now: " << SystemClock::now()
  //               << " delay: " << delay.count() << "ns");

  for (shared_ptr<measurements::Entry> measurementsEntry =
         this->getMeasurements().get(*pitEntry);
       static_cast<bool>(measurementsEntry);
       measurementsEntry = this->getMeasurements().getParent(measurementsEntry))
    {
      NFD_LOG_TRACE("beforeSatisfy: Accessing Measurement Entry for Name: "
                    << measurementsEntry->getName());

      this->getMeasurements().extendLifetime(measurementsEntry, MEASUREMENTS_LIFETIME);

      shared_ptr<MeasurementsEntryInfo> measurementsEntryInfo =
        measurementsEntry->getStrategyInfo<MeasurementsEntryInfo>();

      if (!static_cast<bool>(measurementsEntryInfo))
          continue;

      NFD_LOG_TRACE("beforeStatisfy: Accessing Measurements Info ID: "
                    << measurementsEntryInfo->id);

      MeasurementsEntryInfo::WeightedFaceSetByFaceId& facesById =
        measurementsEntryInfo->weightedFaces.get<MeasurementsEntryInfo::ByFaceId>();

      MeasurementsEntryInfo::WeightedFaceSetByFaceId::iterator faceEntry =
        facesById.find(inFace.getId());

      NFD_LOG_TRACE("Received " << data.getName() << " from FaceId: " << inFace.getId());

      if (faceEntry != facesById.end())
        {
          measurementsEntryInfo->totalDelay += (delay - faceEntry->lastDelay);

          NFD_LOG_TRACE("Recording delay of " << delay.count()
                        << "ns (diff: " << (delay-faceEntry->lastDelay).count()
                        << "ns) for FaceId: " << inFace.getId());


          facesById.modify(faceEntry,
                           bind(&WeightedFace::modifyWeightedFaceDelay,
                                _1, boost::cref(delay)));
        }
      else
        {
          NFD_LOG_WARN("FaceId: " << inFace.getId() << " not in weighted face set");
        }
    }

}

} // namespace fw
} // namespace nfd
