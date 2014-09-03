# -*- Mode:python; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
#
# Copyright (c) 2013-2014 Regents of the University of California.
# Copyright (c) 2014 Susmit Shannigrahi, Steve DiBenedetto
#
# This file is part of ndn-cxx library (NDN C++ library with eXperimental eXtensions).
#
# ndn-cxx library is free software: you can redistribute it and/or modify it under the
# terms of the GNU Lesser General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later version.
#
# ndn-cxx library is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
#
# You should have received copies of the GNU General Public License and GNU Lesser
# General Public License along with ndn-cxx, e.g., in COPYING.md file.  If not, see
# <http://www.gnu.org/licenses/>.
#
# See AUTHORS.md for complete list of ndn-cxx authors and contributors.
#
# @author Wentao Shang <http://irl.cs.ucla.edu/~wentao/>
# @author Steve DiBenedetto <http://www.cs.colostate.edu/~dibenede>
# @author Susmit Shannigrahi <http://www.cs.colostate.edu/~susmit>
# pylint: disable=line-too-long

import sys
import time
import argparse
import traceback

from pyndn import Interest
from pyndn import Name
from pyndn import Face


class Consumer(object):
    '''Sends Interest, listens for data'''

    def __init__(self, prefix, pipeline, count):
        self.prefix = prefix
        self.pipeline = pipeline
        self.count = count
        self.nextSegment = 0
        self.outstanding = dict()
        self.isDone = False

        self.face = Face("127.0.0.1")

    def run(self):
        try:
            while self.nextSegment < self.pipeline:
                self._sendNextInterest(self.prefix)
                self.nextSegment += 1

            while not self.isDone:
                self.face.processEvents()
                time.sleep(0.01)

        except RuntimeError as e:
            print "ERROR: %s" % e



    def _onData(self, interest, data):
        payload = data.getContent()
        name = data.getName()

        print "Received data: %s\n" % payload.toRawStr()
        del self.outstanding[name.toUri()]

        if self.count == self.nextSegment or data.getMetaInfo().getFinalBlockID() == data.getName()[-1]:
            self.isDone = True
        else:
            self._sendNextInterest(self.prefix)
            self.nextSegment += 1


    def _sendNextInterest(self, name):
        self._sendNextInterestWithSegment(Name(name).appendSegment(self.nextSegment))


    def _sendNextInterestWithSegment(self, name):
        interest = Interest(name)
        uri = name.toUri()

        interest.setInterestLifetimeMilliseconds(4000)
        interest.setMustBeFresh(True)

        if name.toUri() not in self.outstanding:
            self.outstanding[name.toUri()] = 1

        self.face.expressInterest(interest, self._onData, self._onTimeout)
        print "Sent Interest for %s" % uri


    def _onTimeout(self, interest):
        name = interest.getName()
        uri = name.toUri()

        print "TIMEOUT #%d: segment #%s" % (self.outstanding[uri], name[-1].toNumber())
        self.outstanding[uri] += 1

        if self.outstanding[uri] <= 3:
            self._sendNextInterestWithSegment(name)
        else:
            self.isDone = True



if __name__ == "__main__":
    parser = argparse.ArgumentParser(description='Parse command line args for ndn consumer')

    parser.add_argument("-u", "--uri", required=True, help='ndn URI to retrieve')
    parser.add_argument("-p", "--pipe",required=False, help='number of Interests to pipeline, default = 1', nargs= '?', const=1, type=int, default=1)
    parser.add_argument("-c", "--count", required=False, help='number of (unique) Interests to send before exiting, default = repeat until final block', nargs='?', const=1,  type=int, default=None)

    arguments = parser.parse_args()

    try:
        uri = arguments.uri
        pipeline = arguments.pipe
        count = arguments.count

        if count is not None and count < pipeline:
            print "Number of Interests to send must be >= pipeline size"
            sys.exit(1)

        Consumer(Name(uri), pipeline, count).run()

    except:
        traceback.print_exc(file=sys.stdout)
        print "Error parsing command line arguments"
        sys.exit(1)
