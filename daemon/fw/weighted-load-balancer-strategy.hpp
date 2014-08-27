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
#include <boost/random/uniform_int_distribution.hpp>

#include <ndn-cxx/util/time.hpp>

#include "strategy.hpp"

namespace nfd {
namespace fw {

/** \class WeightedLoadBalancerStrategy
 *  \brief a forwarding strategy that forwards Interest
 *         to the first nexthop
 */
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


public:
  static const Name STRATEGY_NAME;



protected:

  typedef ndn::time::system_clock SystemClock;
  typedef SystemClock::TimePoint TimePoint;

  boost::random::mt19937 m_randomGenerator;

protected:

  struct PitEntryInfo : public StrategyInfo
  {
    PitEntryInfo()
      : creationTime(SystemClock::now())
    {}

    TimePoint creationTime;
  };

protected:

  struct WeightedFace
  {
    WeightedFace(const Face& face_,
                 const ndn::time::nanoseconds& delay = ndn::time::nanoseconds(0))
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
                            const ndn::time::nanoseconds& delay)
    {
      face.lastDelay = delay;
    }

    const Face& face;
    ndn::time::nanoseconds lastDelay;
  };

protected:

  struct MeasurementsEntryInfo : public StrategyInfo
  {
    MeasurementsEntryInfo()
      : id(ID++)
    {}

    struct ByDelay {};
    struct ByFaceId {};

    // using boost::multi_index::multi_index_container;
    // using boost::multi_index::indexed_by;
    // using boost::multi_index::ordered_unique;
    // using boost::multi_index::hashed_unique;
    // using boost::multi_index::tag;
    // using boost::multi_index::const_mem_fun;

    typedef boost::multi_index::multi_index_container<
      WeightedFace,
      boost::multi_index::indexed_by<
        boost::multi_index::ordered_unique<
          boost::multi_index::tag<ByDelay>,
          boost::multi_index::identity<WeightedFace>
          >,
        boost::multi_index::hashed_unique<
          boost::multi_index::tag<ByFaceId>,
          boost::multi_index::const_mem_fun<WeightedFace, FaceId, &WeightedFace::getId>
          >
        >
      > WeightedFaceSet;

    typedef WeightedFaceSet::index<ByDelay>::type WeightedFaceSetByDelay;
    typedef WeightedFaceSet::index<ByFaceId>::type WeightedFaceSetByFaceId;

    WeightedFaceSet weightedFaces;
    ndn::time::nanoseconds totalDelay;
    int id;

    static int ID;
  };

protected:

  template<typename E>
  shared_ptr<MeasurementsEntryInfo>
  getOrCreateMeasurementsEntryInfo(const shared_ptr<E>& entry)
  {
    BOOST_ASSERT(static_cast<bool>(entry));

    // std::cerr << "Accessing Measurements for " << entry->getPrefix() << "\n";

    shared_ptr<measurements::Entry> measurementsEntry =
      this->getMeasurements().get(*entry);

    shared_ptr<MeasurementsEntryInfo> measurementsEntryInfo =
      measurementsEntry->getStrategyInfo<MeasurementsEntryInfo>();

    if (!static_cast<bool>(measurementsEntryInfo))
      {
        measurementsEntryInfo = make_shared<MeasurementsEntryInfo>();
        measurementsEntry->setStrategyInfo(measurementsEntryInfo);
      }

    return measurementsEntryInfo;
  }

};

} // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_WEIGHTED_LOAD_BALANCER_HPP
