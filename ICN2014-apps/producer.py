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

    def __init__(self, delay=None):
        self.keyChain = KeyChain()
        self.delay = delay
        self.nDataServed = 0


    def run(self, namespace):

        # The default Face will connect using a Unix socket
        face = Face()
        prefix = Name(namespace)

        # Use the system default key chain and certificate name to sign commands.
        face.setCommandSigningInfo(self.keyChain, self.keyChain.getDefaultCertificateName())

        # Also use the default certificate name to sign data packets.
        face.registerPrefix(prefix, self.onInterest, self.onRegisterFailed)

        print "Registering prefix", prefix.toUri()

        while True:
            face.processEvents()
            time.sleep(0.01)



    def onInterest(self, prefix, interest, transport, registeredPrefixId):

        if self.delay is not None:
            time.sleep(self.delay)

        interestName = interest.getName()

        data = Data(interestName)
        data.setContent("Hello " + interestName.toUri())
        data.getMetaInfo().setFreshnessPeriod(3600 * 1000)

        self.keyChain.sign(data, self.keyChain.getDefaultCertificateName())

        transport.send(data.wireEncode().toBuffer())

        self.nDataServed += 1
        print "Replied to: %s (#%d)" % (interestName.toUri(), self.nDataServed)


    def onRegisterFailed(self, prefix):
        print "Register failed for prefix", prefix.toUri()






if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Parse command line args for ndn consumer')
    parser.add_argument("-n", "--namespace", required=True, help='namespace to listen under')
    parser.add_argument("-d", "--delay", required=False, help='namespace to listen under', nargs= '?', const=1, type=float, default=None)

    args = parser.parse_args()

    try:
        namespace = args.namespace
        delay = args.delay

        Producer(delay).run(namespace)

    except:
        traceback.print_exc(file=sys.stdout)
        sys.exit(1)
