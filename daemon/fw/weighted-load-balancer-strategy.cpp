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

#include <boost/random/uniform_int_distribution.hpp>
#include <boost/chrono/system_clocks.hpp>

#include "weighted-load-balancer-strategy.hpp"
#include "core/logger.hpp"
#include "table/measurements-entry.hpp"

using namespace ndn::time;
using namespace boost::multi_index;

namespace nfd {
namespace fw {

struct MyPitInfo : public StrategyInfo
{
  MyPitInfo()
    : creationTime(system_clock::now())
  {}

  system_clock::TimePoint creationTime;
};

struct WeightedFace
{
WeightedFace(const Face& face_,
               const milliseconds& delay = milliseconds(0))
  : face(face_)
  , lastDelay(delay)
  {}

  bool
  operator<(const WeightedFace& other) const
  {
    if (lastDelay == other.lastDelay)
      return face.getId() < other.face.getId();

    return lastDelay < other.lastDelay;
  }

  FaceId
  getId() const
  {
    return face.getId();
  }

  static void
  modifyWeightedFaceDelay(WeightedFace& face,
                          const ndn::time::milliseconds& delay)
  {
    face.lastDelay = delay;
  }

  const Face& face;
  ndn::time::milliseconds lastDelay;
};



struct MyMeasurementInfo : public StrategyInfo
{
struct ByDelay {};
struct ByFaceId {};

typedef multi_index_container<
  WeightedFace,
  indexed_by<
    ordered_unique<
      tag<ByDelay>,
      identity<WeightedFace>
      >,
    hashed_unique<
      tag<ByFaceId>,
      const_mem_fun<WeightedFace, FaceId, &WeightedFace::getId>
      >
    >
  > WeightedFaceSet;

  typedef WeightedFaceSet::index<ByDelay>::type WeightedFaceSetByDelay;
  typedef WeightedFaceSet::index<ByFaceId>::type WeightedFaceSetByFaceId;

  WeightedFaceSet weightedFaces;
  ndn::time::milliseconds totalDelay;
};

NFD_LOG_INIT("WeightedLoadBalancerStrategy");

const Name WeightedLoadBalancerStrategy::STRATEGY_NAME("ndn:/localhost/nfd/strategy/weighted-load-balancer");

WeightedLoadBalancerStrategy::WeightedLoadBalancerStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder, name)
{
}

WeightedLoadBalancerStrategy::~WeightedLoadBalancerStrategy()
{
}

static void
updateStoredNextHops(shared_ptr<MyMeasurementInfo>& measurementsEntryInfo, const fib::NextHopList& nexthops)
{
  MyMeasurementInfo::WeightedFaceSetByFaceId& facesById =
    measurementsEntryInfo->weightedFaces.get<MyMeasurementInfo::ByFaceId>();

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

  for (MyMeasurementInfo::WeightedFaceSetByFaceId::const_iterator i = facesById.begin();
       i != facesById.end();
       ++i)
    {
      if (nexthopFaceIds.find(i->getId()) == nexthopFaceIds.end())
        {
          NFD_LOG_TRACE("pruning FaceId: " << i->getId());
          facesById.erase(i);
        }
    }
}

static milliseconds
calculateInverseDelaySum(const shared_ptr<MyMeasurementInfo>& info)
{
  const MyMeasurementInfo::WeightedFaceSetByDelay& facesByDelay =
    info->weightedFaces.get<MyMeasurementInfo::ByDelay>();

  milliseconds inverseTotalDelay(0);

  for (MyMeasurementInfo::WeightedFaceSetByDelay::const_iterator i = facesByDelay.begin();
       i != facesByDelay.end();
       ++i)
    {
      inverseTotalDelay += (info->totalDelay - i->lastDelay);
    }

  return inverseTotalDelay;
}

void
WeightedLoadBalancerStrategy::afterReceiveInterest(const Face& inFace,
                                                   const Interest& interest,
                                                   shared_ptr<fib::Entry> fibEntry,
                                                   shared_ptr<pit::Entry> pitEntry)
{
  // not a new Interest, don't forward
  if (pitEntry->hasUnexpiredOutRecords())
      return;

  // create timer information and attach to PIT entry
  pitEntry->setStrategyInfo<MyPitInfo>(make_shared<MyPitInfo>());

  shared_ptr<MyMeasurementInfo> measurementsEntryInfo =
    getOrCreateMyMeasurementInfo(fibEntry);


  // reconcile differences between incoming nexthops and those stored
  // on our custom measurement entry info
  updateStoredNextHops(measurementsEntryInfo, fibEntry->getNextHops());

  const MyMeasurementInfo::WeightedFaceSetByDelay& facesByDelay =
    measurementsEntryInfo->weightedFaces.get<MyMeasurementInfo::ByDelay>();

  NFD_LOG_TRACE(facesByDelay.size() << " Faces in weighted face set");

  const milliseconds totalDelay = measurementsEntryInfo->totalDelay;
  const milliseconds inverseTotalDelay = calculateInverseDelaySum(measurementsEntryInfo);


  boost::random::uniform_int_distribution<> dist(0, inverseTotalDelay.count());
  const uint64_t rnd = dist(m_randomGenerator);

  uint64_t cumulativeWeight = 0;

  NFD_LOG_TRACE("Generated rnd = " << rnd);

  for (MyMeasurementInfo::WeightedFaceSetByDelay::const_iterator i = facesByDelay.begin();
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

  this->rejectPendingInterest(pitEntry);
  BOOST_ASSERT(false);
}



void
WeightedLoadBalancerStrategy::beforeSatisfyPendingInterest(shared_ptr<pit::Entry> pitEntry,
                                                           const Face& inFace,
                                                           const Data& data)
{
  shared_ptr<MyPitInfo> pitInfo =
    pitEntry->getStrategyInfo<MyPitInfo>();

  // No start time available, cannot compute delay for this retrieval
  if (!static_cast<bool>(pitInfo))
    return;


  const milliseconds delay =
    duration_cast<milliseconds>(system_clock::now() - pitInfo->creationTime);

  NFD_LOG_TRACE("PIT Info created at " << pitInfo->creationTime
                << " Now: " << system_clock::now()
                << " delay: " << delay.count() << "ms");

  for (shared_ptr<measurements::Entry> measurementsEntry =
         this->getMeasurements().get(*pitEntry);
       static_cast<bool>(measurementsEntry);
       measurementsEntry = this->getMeasurements().getParent(measurementsEntry))
    {
      NFD_LOG_TRACE("beforeSatisfy: Accessing Measurement Entry for Name: "
                    << measurementsEntry->getName());

      this->getMeasurements().extendLifetime(measurementsEntry, seconds(16));

      shared_ptr<MyMeasurementInfo> measurementsEntryInfo =
        measurementsEntry->getStrategyInfo<MyMeasurementInfo>();

      if (!static_cast<bool>(measurementsEntryInfo))
          continue;

      MyMeasurementInfo::WeightedFaceSetByFaceId& facesById =
        measurementsEntryInfo->weightedFaces.get<MyMeasurementInfo::ByFaceId>();

      MyMeasurementInfo::WeightedFaceSetByFaceId::iterator faceEntry =
        facesById.find(inFace.getId());

      NFD_LOG_TRACE("Received " << data.getName() << " from FaceId: " << inFace.getId());

      if (faceEntry != facesById.end())
        {
          measurementsEntryInfo->totalDelay += (delay - faceEntry->lastDelay);

          NFD_LOG_TRACE("Recording delay of " << delay.count()
                        << "ms (diff: " << (delay-faceEntry->lastDelay).count()
                        << "ms) for FaceId: " << inFace.getId());


          facesById.modify(faceEntry,
                           bind(&WeightedFace::modifyWeightedFaceDelay,
                                _1, boost::cref(delay)));
        }
      else
        {
          NFD_LOG_TRACE("FaceId: " << inFace.getId() << " no longer in weighted face set");
        }
    }

}

shared_ptr<MyMeasurementInfo>
WeightedLoadBalancerStrategy::getOrCreateMyMeasurementInfo(const shared_ptr<fib::Entry>& entry)
{
  BOOST_ASSERT(static_cast<bool>(entry));

  shared_ptr<measurements::Entry> measurementsEntry =
    this->getMeasurements().get(*entry);

  shared_ptr<MyMeasurementInfo> measurementsEntryInfo =
    measurementsEntry->getStrategyInfo<MyMeasurementInfo>();

  if (!static_cast<bool>(measurementsEntryInfo))
    {
      measurementsEntryInfo = make_shared<MyMeasurementInfo>();
      measurementsEntry->setStrategyInfo(measurementsEntryInfo);
    }

  return measurementsEntryInfo;
}

} // namespace fw
} // namespace nfd
