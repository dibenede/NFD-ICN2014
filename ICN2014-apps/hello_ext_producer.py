# -*- Mode:python; c-file-style:"gnu"; indent-tabs-mode:nil -*- */
#
# Copyright (C) 2014 Regents of the University of California.
# Copyright (c) 2014 Susmit Shannigrahi, Steve DiBenedetto
#
# Author: Jeff Thompson <jefft0@remap.ucla.edu>
# Author Steve DiBenedetto <http://www.cs.colostate.edu/~dibenede>
# Author Susmit Shannigrahi <http://www.cs.colostate.edu/~susmit>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
# A copy of the GNU General Public License is in the file COPYING.

import sys
import time
import argparse
import traceback
import random

from pyndn import Name
from pyndn import Data
from pyndn import Face
from pyndn.security import KeyChain




class Producer(object):

    def __init__(self, prefix, maxCount=1):
        self.keyChain = KeyChain()
        self.prefix = Name(prefix)
        self.isDone = False

        # Initialize list for Data packet storage.
        # We'll treat the indices as equivalent to the sequence
        # number requested by Interests.
        self.data = []

        finalBlock = Name.Component.fromNumberWithMarker(maxCount - 1, 0x00)
        hourMilliseconds = 3600 * 1000

        # Pre-generate and sign all of Data we can serve.
        # We can also set the FinalBlockID in each packet
        # ahead of time because we know the entire sequence.

        for i in range(maxCount):
            dataName = Name(prefix).appendSegment(i)

            data = Data(dataName)
            data.setContent("Hello, " + dataName.toUri())
            data.getMetaInfo().setFinalBlockID(finalBlock)
            data.getMetaInfo().setFreshnessPeriod(hourMilliseconds)

            self.keyChain.sign(data, self.keyChain.getDefaultCertificateName())

            self.data.append(data)



    def run(self):
        face = Face()

        # Use the system default key chain and certificate name to sign commands.
        face.setCommandSigningInfo(self.keyChain, self.keyChain.getDefaultCertificateName())

        # Also use the default certificate name to sign data packets.
        face.registerPrefix(self.prefix, self.onInterest, self.onRegisterFailed)

        print "Registering prefix %s" % self.prefix.toUri()

        while not self.isDone:
            face.processEvents()
            time.sleep(0.01)



    def onInterest(self, prefix, interest, transport, registeredPrefixId):
        interestName = interest.getName()
        sequence = interestName[-1].toNumber()

        if 0 <= sequence and sequence < len(self.data):
            transport.send(self.data[sequence].wireEncode().toBuffer())

        print "Replied to: %s" % interestName.toUri()


    def onRegisterFailed(self, prefix):
        print "Register failed for prefix", prefix.toUri()
        self.isDone = True






if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Parse command line args for ndn producer')
    parser.add_argument("-n", "--namespace", required=True, help='namespace to listen under')
    parser.add_argument("-c", "--count", required=False, help='number of Data packets to generate, default = 1', nargs='?', const=1,  type=int, default=1)

    args = parser.parse_args()

    try:
        namespace = args.namespace
        maxCount = args.count

        Producer(namespace, maxCount).run()

    except:
        traceback.print_exc(file=sys.stdout)
        sys.exit(1)
