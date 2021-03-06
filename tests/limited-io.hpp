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

#ifndef NFD_TESTS_LIMITED_IO_HPP
#define NFD_TESTS_LIMITED_IO_HPP

#include "core/global-io.hpp"
#include "core/scheduler.hpp"

namespace nfd {
namespace tests {

/** \brief provides IO operations limit and/or time limit for unit testing
 */
class LimitedIo
{
public:
  LimitedIo();

  /// indicates why .run returns
  enum StopReason
  {
    /// g_io.run() runs normally because there's no work to do
    NO_WORK,
    /// .afterOp() has been invoked nOpsLimit times
    EXCEED_OPS,
    /// nTimeLimit has elapsed
    EXCEED_TIME,
    /// an exception is thrown
    EXCEPTION
  };

  /** \brief g_io.run() with operation count and/or time limit
   *
   *  \param nOpsLimit operation count limit, pass UNLIMITED_OPS for no limit
   *  \param nTimeLimit time limit, pass UNLIMITED_TIME for no limit
   */
  StopReason
  run(int nOpsLimit, const time::nanoseconds& nTimeLimit);

  /// count an operation
  void
  afterOp();

  const std::exception&
  getLastException() const;

private:
  void
  afterTimeout();

public:
  static const int UNLIMITED_OPS;
  static const time::nanoseconds UNLIMITED_TIME;

private:
  bool m_isRunning;
  int m_nOpsRemaining;
  EventId m_timeout;
  StopReason m_reason;
  std::exception m_lastException;
};

} // namespace tests
} // namespace nfd

#endif // NFD_TESTS_LIMITED_IO_HPP
