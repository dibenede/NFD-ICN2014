/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014,  Regents of the University of California,
 *                      Arizona Board of Regents,
 *                      Colorado State University,
 *                      University Pierre & Marie Curie, Sorbonne University,
 *                      Washington University in St. Louis,
 *                      Beijing Institute of Technology,
 *                      The University of Memphis
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
 */

#include "table/measurements.hpp"

#include "tests/test-common.hpp"
#include "tests/limited-io.hpp"

namespace nfd {
namespace tests {

BOOST_FIXTURE_TEST_SUITE(TableMeasurements, BaseFixture)

BOOST_AUTO_TEST_CASE(Get_Parent)
{
  NameTree nameTree;
  Measurements measurements(nameTree);

  Name name0;
  Name nameA ("ndn:/A");
  Name nameAB("ndn:/A/B");

  shared_ptr<measurements::Entry> entryAB = measurements.get(nameAB);
  BOOST_REQUIRE(static_cast<bool>(entryAB));
  BOOST_CHECK_EQUAL(entryAB->getName(), nameAB);

  shared_ptr<measurements::Entry> entry0 = measurements.get(name0);
  BOOST_REQUIRE(static_cast<bool>(entry0));

  shared_ptr<measurements::Entry> entryA = measurements.getParent(entryAB);
  BOOST_REQUIRE(static_cast<bool>(entryA));
  BOOST_CHECK_EQUAL(entryA->getName(), nameA);

  shared_ptr<measurements::Entry> entry0c = measurements.getParent(entryA);
  BOOST_CHECK_EQUAL(entry0, entry0c);
}

BOOST_AUTO_TEST_CASE(Lifetime)
{
  LimitedIo limitedIo;
  NameTree nameTree;
  Measurements measurements(nameTree);
  Name nameA("ndn:/A");
  Name nameB("ndn:/B");
  Name nameC("ndn:/C");

  BOOST_CHECK_EQUAL(measurements.size(), 0);

  shared_ptr<measurements::Entry> entryA = measurements.get(nameA);
  shared_ptr<measurements::Entry> entryB = measurements.get(nameB);
  shared_ptr<measurements::Entry> entryC = measurements.get(nameC);
  BOOST_CHECK_EQUAL(measurements.size(), 3);

  const time::nanoseconds EXTEND_A = time::seconds(2);
  const time::nanoseconds CHECK1 = time::seconds(3);
  const time::nanoseconds CHECK2 = time::seconds(5);
  const time::nanoseconds EXTEND_C = time::seconds(6);
  const time::nanoseconds CHECK3 = time::seconds(7);
  BOOST_ASSERT(EXTEND_A < CHECK1 &&
               CHECK1 < Measurements::getInitialLifetime() &&
               Measurements::getInitialLifetime() < CHECK2 &&
               CHECK2 < EXTEND_C &&
               EXTEND_C < CHECK3);

  measurements.extendLifetime(entryA, EXTEND_A);
  measurements.extendLifetime(entryC, EXTEND_C);
  // remaining lifetime:
  //   A = initial lifetime, because it's extended by less duration
  //   B = initial lifetime
  //   C = EXTEND_C
  entryA.reset();
  entryB.reset();
  entryC.reset();

  BOOST_CHECK_EQUAL(limitedIo.run(LimitedIo::UNLIMITED_OPS, CHECK1), LimitedIo::EXCEED_TIME);
  BOOST_CHECK_EQUAL(static_cast<bool>(measurements.findExactMatch(nameA)), true);
  BOOST_CHECK_EQUAL(static_cast<bool>(measurements.findExactMatch(nameB)), true);
  BOOST_CHECK_EQUAL(static_cast<bool>(measurements.findExactMatch(nameC)), true);
  BOOST_CHECK_EQUAL(measurements.size(), 3);

  BOOST_CHECK_EQUAL(limitedIo.run(LimitedIo::UNLIMITED_OPS, CHECK2 - CHECK1),
                    LimitedIo::EXCEED_TIME);
  BOOST_CHECK_EQUAL(static_cast<bool>(measurements.findExactMatch(nameA)), false);
  BOOST_CHECK_EQUAL(static_cast<bool>(measurements.findExactMatch(nameB)), false);
  BOOST_CHECK_EQUAL(static_cast<bool>(measurements.findExactMatch(nameC)), true);
  BOOST_CHECK_EQUAL(measurements.size(), 1);

  BOOST_CHECK_EQUAL(limitedIo.run(LimitedIo::UNLIMITED_OPS, CHECK3 - CHECK2),
                    LimitedIo::EXCEED_TIME);
  BOOST_CHECK_EQUAL(static_cast<bool>(measurements.findExactMatch(nameA)), false);
  BOOST_CHECK_EQUAL(static_cast<bool>(measurements.findExactMatch(nameB)), false);
  BOOST_CHECK_EQUAL(static_cast<bool>(measurements.findExactMatch(nameC)), false);
  BOOST_CHECK_EQUAL(measurements.size(), 0);
}

BOOST_AUTO_TEST_CASE(EraseNameTreeEntry)
{
  LimitedIo limitedIo;
  NameTree nameTree;
  Measurements measurements(nameTree);
  size_t nNameTreeEntriesBefore = nameTree.size();

  shared_ptr<measurements::Entry> entry = measurements.get("/A");
  BOOST_CHECK_EQUAL(
    limitedIo.run(LimitedIo::UNLIMITED_OPS,
                  Measurements::getInitialLifetime() + time::milliseconds(10)),
    LimitedIo::EXCEED_TIME);
  BOOST_CHECK_EQUAL(measurements.size(), 0);
  BOOST_CHECK_EQUAL(nameTree.size(), nNameTreeEntriesBefore);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace tests
} // namespace nfd
