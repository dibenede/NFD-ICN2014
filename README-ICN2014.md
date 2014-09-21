Code Walkthrough Source for ICN 2014
==============================================

## Included ICN2014-apps

This directory includes the Python application examples. **Note**:
[PyNDN2](https://github.com/named-data/PyNDN2) is a pre-requesite for
runnings these applications.

### Hello World Applications

- hello_consumer.py
- hello_producer.py

### Extended Hello World Applications

- hello_ext_consumer.py
- hello_ext_producer.py

### Forwarding Strategy Demo Helper Applications

- consumer.py
- producer.py

## Not Included in ICN2014-apps

### Forwarding Strategy Files

- random-load-balancer-strategy.{hpp, cpp}
- weighted-load-balancer-strategy.{hpp, cpp}
- (modified) available-strategies.cpp

The example forwarding strategy source code is colocated with
the stock NFD strategies. From the project root, you can
find all forwarding strategies under `daemon/fw/`.

available-strategies.cpp is a standard part NFD, but has
been modified to "install" the example forwarding strategies.

Consequently, the example forwarding strategies can be compiled as
part of the normal NFD build process. Please see the
[NFD homepage](http://named-data.net/doc/NFD/) for build and
installation instructions.

# Code Walkthrough

This guide will walk you through writing a simple NDN
application with PyNDN2 as well as how to develop a new
forwarding strategy for NFD. We assume you have previously
installed the following packages:

* NFD 0.2.0 (included, this repository)
* ndn-cxx 0.2.0
* PyNDN2

More information about NFD, ndn-cxx, PyNDN2, and the rest
of the NDN Platform, can be found at the
[NDN Platform homepage](http://named-data.net/codebase/platform/).

## Applications

* NDN-CCL overview
* core OO abstractions
* PyNDN2, installation


### Hello World

This section illustrates how to write simple, but functionally
complete, "Hello World" NDN consumer and producer applications using
the PyNDN2 library. In particular, you will learn how to:

* listen for Interests matching a namespace
* create and send Interests and Data packets
* catch and handle common NDN communication error

These example applications may be a little longer and complex
than the minimal NDN "Hello World" application, but we hope they will
serve as useful starting points for your own applications.


#### Hello World Producer

Our producer application will serve a "Hello" Data packet in response
to each incoming Interest. To achieve this, the producer will register
a prefix to "listen" for Interests matching said prefix.

Let's get started by importing the relevant modules from PyNDN2.

    from pyndn import Name
    from pyndn import Data
    from pyndn import Face
    from pyndn.security import KeyChain


One common application pattern is to create a driving class to
encapsulate our application's logic. For our purposes, let's call this
class Producer:

    class Producer(object):

        def __init__(self):
            self.keyChain = KeyChain()
            self.isDone = False

We've initialized the `Producer` with a `KeyChain`
instance. `KeyChain` is an abstraction around your system's key
management. It associates identities with their respective keys. For
example, a user may have multiple identities such as "Home", "Work",
"Friends" and use a different keypair for each. `KeyChain` also has a
concept of a default identity that can be used without explicitly
specifying the name of an identity.

To see this in action, we'll add a `run` method to `Producer`.

    def run(self, namespace):
        # Create a connection to the local forwarder over a Unix socket
        face = Face()

        prefix = Name(namespace)

        # Use the system default key chain and certificate name to sign commands.
        face.setCommandSigningInfo(self.keyChain, \
                                   self.keyChain.getDefaultCertificateName())


        # Also use the default certificate name to sign Data packets.
        face.registerPrefix(prefix, self.onInterest, self.onRegisterFailed)

        print "Registering prefix", prefix.toUri()

        # Run the event loop forever. Use a short sleep to
        # prevent the Producer from using 100% of the CPU.
        while not self.isDone:
            face.processEvents()
            time.sleep(0.01)


`run` is responsible for establishing a connection to the local
forwarder (NFD), registering a prefix to publish content under, and
running the program's event loop. The commented lines denote the
essential parts of `run`'s operation, but let's focus on the prefix
registration process.

The first step for registering a prefix is to configure the library
for command signing. NFD requires an authorized signed Interests in
order to perform management actions like modifying the FIB. We
configure the library in the above code with with the call to
`setCommandSigningInfo`.

Next, we need to inform NFD of the prefix our producer serves data
for. Prefix registration tells NFD to send matching Interests to our
application's Face. This is accomplished by calling `registerPrefix`
with the prefix we want to register (represented by an `Name` object)
with two callback functions (`self.onInterest` and
`self.onRegisterFailed` here).

The first callback, `onInterest`, is triggered whenever an Interest
matching the registered prefix is delivered to the application's Face.
This callback takes several arguments: a prefix (`Name` instance), the
arriving `Interest`, a `Transport`, and the registered prefix's ID.

The second callback, `onRegisterFailed`, is used if registration
fails. **NOTE** If prefix registration fails, a good first step is to
ensure that both the **nfd** and **nrd** processes are running. The
`onRegisterFailed` callback is given the name of the prefix that
failed to register.

    def onInterest(self, prefix, interest, transport, registeredPrefixId):
        interestName = interest.getName()

        data = Data(interestName)
        data.setContent("Hello, " + interestName.toUri())

        hourMilliseconds = 3600 * 1000
        data.getMetaInfo().setFreshnessPeriod(3600 * 1000)

        self.keyChain.sign(data, self.keyChain.getDefaultCertificateName())

        transport.send(data.wireEncode().toBuffer())

        print "Replied to: %s" % interestName.toUri()

    def onRegisterFailed(self, prefix):
        print "Register failed for prefix", prefix.toUri()
        self.isDone = True

The `onInterest` callback is the more interesting callback (we have
limited options if prefix registration fails). The
`Producer.onInterest` method essentially echoes the incoming
Interest. First, it creates a new `Data` instance using the `Name` it
extracts from the `Interest`. Next, it sets the payload of the created
Data packet using `Data.setContent`. In this example, we're just
setting the payload to a simple hello message.

Once we've setup our response Data packet, we need to sign and publish
it. Our `KeyChain` instance provides a `sign` method that
cryptographically signs the packet with the specified identity. Here,
we just use the default identity as we did during prefix registration.

The signed packet can now be published using the provided `transport`
argument's `send` method. The `Transport` class is an abstraction of
lower level communication methods. PyNDN2 currently supports TCP, UDP,
and Unix socket transport. The `onInterest` callback will be given the
specific transport channel instance that the Interest arrived over so
that you can respond to the correct interface.

`Transport.send` requires an array-type buffer of data to send. We can
obtain this kind of arrangement from our `Data` instance by wire
encoding it, which returns a `SignedBlob` instance, and converting it
to a buffer. The exact nature of the buffer depends on whether you're running
Python 2 or 3. For more details, please see [pyndn.util.Blob](https://github.com/named-data/PyNDN2/blob/master/python/pyndn/util/blob.py)

Together, the previously described methods round out everything
necessary to create a publisher/producer class. To actually run your
producer, you'll first need start NFD using the `nfd-start` script and
wait for your shell's prompt to return. Then, create an instance an
invoke its run method with an NDN namespace URI:

    Producer().run("/hello")

The producer will not do much because it is waiting for
Interests. However, you should see it print a "Registering prefix
&lt;namespace&gt;" message with the namespace you provided to `run`.
When you're done the producer, you can stop the Python application
with ctrl-c and NFD with the `nfd-stop` script.

#### Hello World Consumer

Next, we'll make a consumer application to send Interests to our
producer and print the response messages. As with the producer,
we'll first begin by creating an encapsulating `Consumer` class.

    from pyndn import Name
    from pyndn import Data
    from pyndn import Face
    from pyndn.security import KeyChain

    class Consumer(object):
        def __init__(self, prefix):
            self.prefix = Name(prefix)
            self.outstanding = dict()
            self.isDone = False
            self.face = Face("127.0.0.1")

The `Consumer` constructor initializes more state than
`Producer`.  As we will see in a moment, this is because we want to
build a reasonably robust client that can perform retransmission.
Also note that the consumer's Face is initialized with an IP address.
This results in `Consumer` using TCP to connect to the local NFD
instance (recall that `Producer` uses the `Face()` constructor to
create a Unix socket Face). We could have just as easily used a Unix
socket Face, but chose to use TCP to demonstrate the alternative
creation process.

    def run(self):
        try:
            self._sendNextInterest(self.prefix)

            while not self.isDone:
                self.face.processEvents()
                time.sleep(0.01)

        except RuntimeError as e:
            print "ERROR: %s" %  e

Like `Producer`, `Consumer` also has a `run` containing the event
loop. The main difference is the `self._sendNextInterest` call before
the event loop begins. `Consumer` abstracts the Interest creation and
sending process into its own method to simplify retransmission (i.e
avoid code duplication).

    def _sendNextInterest(self, name):
        interest = Interest(name)
        uri = name.toUri()

        interest.setInterestLifetimeMilliseconds(4000)
        interest.setMustBeFresh(True)

        if uri not in self.outstanding:
            self.outstanding[uri] = 1

        self.face.expressInterest(interest, self._onData, self._onTimeout)
        print "Sent Interest for %s" % uri


The `_sendNextInterest` method creates an Interest from a `Name` and
configures its lifetime and the freshness selector. The lifetime
attribute tells each NDN node to expire the Interest **N**
milliseconds after it arrives (here we use 4000 ms). The freshness
selector specifies whether the NDN node's Content Store may satisfy
the Interest with stale content (i.e Data that has been held past its
FreshnessPeriod). We specify that the content must be fresh in this
example to demonstrate its usage. By default, Interests permit stale
content matches. `self.outstanding` is a dictionary (initialized in
`__init__`) that counts the number of transmission attempts for each
Interest by the URI of its name.

The key part of `_sendNextInterest` is the
`self.face.expressInterest` call. Here, we tell our Face to actually
send the Interest we constructed and provided two callback
methods. The first callback is invoked when a matching Data packet is
delivered and the second signals that the sent Interet timed out/expired.

    def _onData(self, interest, data):
        payload = data.getContent()
        name = data.getName()

        print "Received data: ", payload.toRawStr()
        del self.outstanding[name.toUri()]

        self.isDone = True

`Consumer` uses `_onData` as its on Data arrival callback. This
callback will be invoked with the Interest and Data packets that
matched. In this example, our callback just prints the payload of the
Data, which is accessed by the `Data.getContent` method (recall that
the payload was set with a similarly named setter method).  `_onData`
also removes the transmission count entry from `self.outstanding` and
signals the main event loop in `Consumer.run` to exit by setting
`self.isDone`.

    def _onTimeout(self, interest):
        name = interest.getName()
        uri = name.toUri()

        print "TIMEOUT #%d: %s" % (self.outstanding[uri], uri)
        self.outstanding[uri] += 1

        if self.outstanding[uri] <= 3:
            self._sendNextInterest(name)
        else:
            self.isDone = True

In comparison, the timeout callback, `_onTimeout`, is called with just
the Interest that expired before retrieving Data. `_onTimeout`
increments the transmission count for the specific Interest and starts
the program termination process if the application has tried too many
times (arbitrarily set to 3 times).


With the timeout callback implemented, we now have a functional
consumer application that will request a single Data packet and exit
after success or 3 timeouts. To run your `Consumer`, first start NFD
and your `Producer`. Then, create a `Consumer` instance and call
the run method:

    Consumer("/hello").run()

Note that we use "/hello" here for the name of the Data to request and
similarly told the producer application to to serve/generate content
under the "/hello" namespace.


### Extended Hello World

With the basics of NDN application writing in PyNDN2 covered, we can
now extend the producer and consumer into more useful NDN
applications. Specifically, you'll learn how to:

* Serve pre-generated content
* Communicate the end of a sequence or stream of content to consumers
* Retrieve content that spans multiple Data packets
* Pipeline multiple Interests

This section of the guide assumes you have a copy of the Hello World
`Producer` and `Consumer` class and will focus on the functionality
improvements. If you are starting the guide from here, you may find copies
under `ICN2014-apps` as `hello_producer.py` and `hello_consumer.py`.

#### Extended Producer

The extended `Producer` class pre-packetizes its content and stores it
to make it easy to serve subsequent requests. Typically, an
application will publish content that is larger than the maximum
packet size (currently about 8 KB). This makes it necessary for the
publisher to *sequence* the Data packets so that consumers can
recognize when there is more content to be retrieved.

In NDN, packet sequence numbers can be presented in name components by
the `0x00` marker followed by the number (see the [NDN Naming Conventions](http://named-data.net/wp-content/uploads/2014/08/ndn-tr-22-ndn-memo-naming-conventions.pdf) memo for more details). We can see this in action in the extended `Producer`'s constructor.

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


We initialize a list to store the Data packets we create. For the sake
of this example, we'll generate a pre-determined number of packets
specified by `maxCount`. In a real application, such as serving a
large file, `maxCount` could instead be calculated based on the
desired Data packet size.

Knowing the number of packets to generate ahead of time or that are
remaining allows us to inform the consumer of the end of the content
sequence. Producers can inform their consumers of the end by setting the
FinalBlockId meta-information in one or more of the Data packets. The
optional FinalBlockId field is an NDN name component found in the
Data's MetaInfo TLV block (i.e. not in the Data's Name TLV). Consumers
determine whether a given Data packet is the last one in a sequence by
comparing the FinalBlockId to the last component (before the implicit
digest) of the Data's name. If the fields match, then the Data packet
is the last item in the sequence. One common use is for the
FinalBlockId to refer to a sequence number name component, but any
valid name component can signal the end of a collection.

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


    def onRegisterFailed(self, prefix):
        print "Register failed for prefix", prefix.toUri()
        self.isDone = True

There is little modification to `Producer.run`; the method no longer
needs a namespace argument because we already know the prefix and have
constructed all of the Data packets in the constructor. Similarly, the
`onRegisterFailed` callback is unmodified (we we provide it for
completeness).

    def onInterest(self, prefix, interest, transport, registeredPrefixId):
        interestName = interest.getName()
        sequence = interestName[-1].toNumber()

        if 0 <= sequence and sequence < len(self.data):
            transport.send(self.data[sequence].wireEncode().toBuffer())

        print "Replied to: %s" % interestName.toUri()

Finally, `Producer.onInterest` is now serves the constructed Data
packets we placed in the `Producer.data` list. `onInterest` determines
the correct Data packet to publish by converting the incoming
Interest's sequence number to an integer and returning the packet
stored at the referenced list index (assuming it is within the list's
bounds).

You can now run your extender producer with the following:

    Producer("/hello", 10).run()

where "/hello" is the prefix to publish content under and **10**
represents the number of Data packets to publish.


#### Extended Consumer

We can now extend our initial `Consumer` class to fully use the
enhanced `Producer`. First, `Consumer` must be able to request the
entire range of published content. Second, `Consumer` should be able
to request *multiple* Data packets at once by pipelining the
Interests. While `Consumer` could wait for each requested Data to
return before requesting another item, the process is much slower than
requesting multiple items simultaneously.

    class Consumer(object):
        def __init__(self, prefix, pipeline):
            self.prefix = Name(prefix)
            self.pipeline = pipeline
            self.nextSegment = 0
            self.outstanding = dict()
            self.isDone = False
            self.face = Face("127.0.0.1")


To begin, `Consumer`'s init method has been augmented in a few ways;
`Consumer` now accepts the number of Interests to pipeline as an
argument and initializes `Consumer.nextSegment` to keep track of the
next Interest segment number to request.

    def run(self):
        try:
            while self.nextSegment < self.pipeline:
                self._sendNextInterest(self.prefix)
                self.nextSegment += 1

            while not self.isDone:
                self.face.processEvents()
                time.sleep(0.01)

        except RuntimeError as e:
            print "ERROR: %s" %  e


The `Consumer.run` method is extended to initiate sending pipelined
Interest. `run` immediately will send `self.pipeline` count
Interests. As each Interest returns Data (or expires), a
new Interest will be sent in its place for the next segment (or
retransmission). This process repeats until the entire collection
has been retrieved.

    def _sendNextInterest(self, name):
        nameWithSegment = Name(name).appendSegment(self.nextSegment)
        self._sendNextInterestWithSegment(nameWithSegment)


    def _sendNextInterestWithSegment(self, name):
        interest = Interest(name)
        uri = name.toUri()

        interest.setInterestLifetimeMilliseconds(4000)
        interest.setMustBeFresh(True)

        if uri not in self.outstanding:
            self.outstanding[uri] = 1

        self.face.expressInterest(interest, self._onData, self._onTimeout)
        print "Sent Interest for %s" % uri

Previously, `Consumer._sendNextInterest` was responsible for the heavy
lifting of sending the Interest and updating the retransmission
count. However, it is now possible that we either need to request the
latest segment or retransmit any one of the previously pipelined
Interests. To simplify the program flow, `_sendNextInterest` has been
refactored into a frontend for sending an Interest with the latest
sequence number. The actual Interest sending moves into
`_sendNextInterestWithSegment`, that expects a `Name` instance with the
appropriate sequence number appended. Now, we can easily
retransmit lost Interests from `Consumer._onTimeout` as before by
directly calling `_sendNextInterestWithSegment` because `_onTimeout`
knows the exact Interest (and its name with a segment number) that
timed out.

We can see the actual segment number being appended to the outgoing
Interest in `_sendNextInterest`. The `Name` class allows us to append
integers as sequence numbers via `Name.appendSegment`.

    def _onData(self, interest, data):
        payload = data.getContent()
        dataName = data.getName()

        print "Received data: ", payload.toRawStr()
        del self.outstanding[interest.getName().toUri()]

        finalBlockId = data.getMetaInfo().getFinalBlockID()

        if finalBlockId.getValue().size() > 0 and \
           finalBlockId == dataName[-1]:
            self.isDone = True
        else:
            self._sendNextInterest(self.prefix)
            self.nextSegment += 1

Moving on to `Consumer._onData`, relatively little code has
changed. The key difference is that `Consumer` now checks if the
arrived Data packet is the final block of the collection and starts
the program termination process if it is. A Data packet's FinalBlockId
is accessed, much like it is set, via a meta-information
object. Before checking the value of the FinalBlockId, it is first
necessary to ensure that the field is actually present;
`data.getMetaInfo().getFinalBlockID()` will not produce an error if
the field is absent. Instead, the application must check to see if the
FinalBlockId field has a non-zero length. If a value is present, it
can be interpreted as a `Name.Component` instance. We can then compare
the FinalBlockId directly against the Data's name component preceding
the implicit digest (i.e. the -1 with PyNDN2's negative index support).

    def _onTimeout(self, interest):
        name = interest.getName()
        uri = name.toUri()

        print "TIMEOUT #%d: segment #%s" % (self.outstanding[uri], name[-1].toNumber())
        self.outstanding[uri] += 1

        if self.outstanding[uri] <= 3:
            self._sendNextInterestWithSegment(name)
        else:
            self.isDone = True

`Consumer._onTimeout` is slightly modified, as previously mentioned, to
use `Consumer._sendNextInterestWithSegment` instead of
`Consumer._sendNextInterest` because we want to retransmit an Interest
with a specific sequence number.


We can now run the extended `Consumer` with the following:

        Consumer("/hello", 2).run()

where "/hello" is the prefix to retrieve content from and **2** represents
the Interest pipeline size. Feel free to experiment with different
pipeline sizes.



## Forwarding Strategy

Forwarding Strategies are a key component of NFD's *forwarding
pipelines*; they are responsible for choosing the set of Faces through
which an outgoing Interest should be forwarded. Strategies are
associated with individual FIB entries, thus allowing fine grain
control over how an NDN node forwards.

In NFD, the forwarding strategy can be thought of as the "last stop"
for an outgoing Interest. By the time a strategy is invoked the CS and
PIT have already been checked for cache hits and duplicates,
respectively. Similarly, NFD forwarding strategies are also informed
when Data packets arrive so that the strategies can attempt to keep
measurement and other performance information. Specifically,
strategies are notified of Data when it is about to satisfy a PIT
entry.

NFD allows developers to implement and enable custom forwarding
strategies. Developers can inherit from an abstract ``Strategy`` base
class and overload its methods, called *triggers*, that allow the
custom strategy to perform a range of actions before Interests are
sent out and PIT entries are satisfied by returning Data. For more
information on the details of NFD forwarding strategies and pipelines,
interested readers should refer to the
[NFD Developer's Guide](http://named-data.net/wp-content/uploads/2014/07/NFD-developer-guide.pdf)

Unlike NDN applications, which can be implemented in a wide range of
languages, forwarding strategies must be written in C++. Consequently,
there are often large amounts of setup and accounting code that are
unsuitable for quickly understanding what is going on in a new
forwarding strategy. As a result, this section of the guide will omit
much of the surrounding code such as header files and helper methods
specific to the strategy's implementation. Instead, the reader should
refer to the provided forwarding strategy
(`daemon/fw/{random-load-balancer-strategy,
weighted-load-balancer-strategy}.{hpp, cpp}`) files for the full
implementations.

## Random Load Balancer

We will first create a stateless, random, load balancer strategy called
`RandomLoadBalancerStrategy`. Given a set of possible outgoing Faces,
this strategy will simply select one at random for forwarding.

All NFD forwarding strategies must be named so that they can be set
programatically as well as at runtime by tools such as nfdc. For this
example, we will name our random load balancer
`ndn:/localhost/nfd/strategy/random-load-balancer`:

    const Name RandomLoadBalancerStrategy::STRATEGY_NAME("ndn:/localhost/nfd/strategy/random-load-balancer");

The `/localhost/nfd/strategy` portion of the name is a common prefix
for all NFD strategies. You may also use this prefix to name your own
strategies so long as the full name is unique. The strategy's name is
a required argument for the `Strategy` class' constructor.

As previously mentioned, forwarding strategies can perform actions on
outbound Interests that determine where they are forwarded. This is
achieved by overriding the `Strategy::afterReceiveInterest` method.
`afterReceiveInterest` is invoked with the Face the Interest arrived
from, the Interest itself, the longest matching FIB entry, and the
Interest's PIT entry.


    void
    RandomLoadBalancerStrategy::afterReceiveInterest(const Face& inFace,
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

      // Ensure there is at least 1 Face is available for forwarding
      if (!hasFaceForForwarding(nexthops, pitEntry))
        {
          this->rejectPendingInterest(pitEntry);
          return;
        }

      fib::NextHopList::const_iterator selected;
      do
        {
          boost::random::uniform_int_distribution<> dist(0, nexthops.size() - 1);
          const size_t randomIndex = dist(m_randomGenerator);

          uint64_t currentIndex = 0;

          for (selected = nexthops.begin();
               selected != nexthops.end() && currentIndex != randomIndex;
               ++selected, ++currentIndex)
            { }
        } while (!canForwardToNextHop(pitEntry, *selected));

      this->sendInterest(pitEntry, selected->getFace());
    }

For the random load balancer strategy, we simply need to randomly
select a Face to send the Interest out. Conveniently, the FIB entry
argument provides access to a list-type (via iterators) of
nexthops. With the nexthops in hand, the remainder of the code
generates a random number, *i*, and selects the *i-th* nexthop to
forward the Interest out of. Reinforcing code is also included to
ensure the selected Face is working. If it is, the
`Strategy::sendInterest` method is used to actually forward the
Interest.

Note that forwarding strategies are also permitted to *drop* Interests
that they deem should not be forwarded. `RandomLoadBalancerStrategy`
drops Interests when it does not have any Faces available for
forwarding, as seen in the call to `Strategy::rejectPendingInterest`.

Aside from some setup C++ code and lightweight helper methods, the
above code is all it takes to create a new forwarding strategy that
can send or reject Interests. However, we still need to make the
strategy available for use. Your new strategy must first be
*installed*, in NFD parlance, so that NFD knows that it exists and
can activate it when requested. To install
your strategy, modify `daemon/fw/available-strategies.cpp`'s
`installStrategies` method:

        void
        installStrategies(Forwarder& forwarder)
        {
            ...

          installStrategy<RandomLoadBalancerStrategy>(forwarder);
        }


Now, after starting NFD, you can set the strategy at runtime with nfdc's set-strategy command:

        nfdc set-strategy <namespace> ndn:/localhost/nfd/strategy/random-load-balancer

Replace `<namespace>` with any prefix(es) you wish to activate your strategy for.


## Weighted Load Balancer

While a good demonstration of the basic principles of creating a
forwarding strategy, our stateless `RandomLoadBalancerStrategy` is
probably too simple for many real world use cases. Instead, you may
want to create a strategy that stores and acts on some kind of
measurement information. NFD provides an interface for
attaching information to existing constructs such as PIT entries and a
Measurements table for this purpose.

For our next custom strategy, we will take advantage of these storage
options to keep track of performance information. The
`WeightedLoadBalancerStrategy` will time how long it takes to receive
a Data response for each Face it sends an Interest out and will bias
future forwarding towards Faces with faster responses. To do this,
`WeightedLoadBalancerStrategy` will record the time it sends an Interest
on the PIT entry.

However, the strategy will also need to remember the last retrieval
delay for each Face. Therefore, it needs a place to store the delay
collection where it can be easily retrieved and *persist* across
Interests (after all, the PIT entry will be consumed). NFD's
Measurement table is such a storage option.

To begin, we'll name the new strategy
`ndn:/localhost/nfd/strategy/weighted-load-balancer`:

    const Name WeightedLoadBalancerStrategy::STRATEGY_NAME("ndn:/localhost/nfd/strategy/weighted-load-balancer");

Next, before digging into the implementation of the strategy itself,
we'll define two custom storage classes to hold our clock and delay
measurements. The only requirement for custom storage classes is that
they extend the `StrategyInfo` class; NFD will not attempt to modify
the data itself in anyway, but will garbage collect old Measurement
table data.

    class MyPitInfo : public StrategyInfo
    {
    public:
        MyPitInfo()
            : creationTime(system_clock::now())
        {}

        system_clock::TimePoint creationTime;
    };


    class MyMeasurementInfo : public StrategyInfo
    {
    public:
        void
        updateFaceDelay(const Face& face, const milliseconds& delay);

        void
        updateStoredNextHops(const fib::NextHopList& nexthops);

        static milliseconds
        calculateInverseDelaySum(const shared_ptr<MyMeasurementInfo>& info);
            ...

        //Collection of Faces sorted by delay
        WeightedFaceSet weightedFaces;
    };


`MyPitInfo` records the time the instance was created at
(i.e. starting a timer). `MyMeasurementInfo` holds the delay
collection (sorted by increasing delay) and provides helpers to
manipulate the set.

With the storage classes defined, we can now override the `Strategy`
class' methods. First, we will override `afterReceiveInterest` to
record Interest send times by creating and storing `MyPitInfo`
instances. The delay biasing calculations and housekeeping are
implemented in other methods that are omitted for brevity.

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
               myGetOrCreateMyMeasurementInfo(fibEntry);


      // reconcile differences between incoming nexthops and those stored
      // on our custom measurement entry info
      measurementsEntryInfo->updateStoredNextHops(fibEntry->getNextHops());

      if (!this->mySendInterest(interest, measurementsEntryInfo, pitEntry))
        {
          this->rejectPendingInterest(pitEntry);
          BOOST_ASSERT(false);
        }
    }

The first important line in the above code is

    pitEntry->setStrategyInfo<MyPitInfo>(make_shared<MyPitInfo>());

which creates a new `MyPitInfo` instance (our send time storage) and
attaches it to the PIT entry.

Next, our strategy needs to access its previously stored delay
measurements to determine where it should send the current
Interest. For this, the strategy needs to access the Measurements
table. Measurements table entries are associated with name prefixes
just like any other (FIB, PIT, etc.)  NFD table. Here, we have a
helper method that will lookup and retrieve the Measurements table
entry using the FIB entry's prefix (i.e. stored at the same level of
the tree).

    shared_ptr<MyMeasurementInfo>
    WeightedLoadBalancerStrategy::myGetOrCreateMyMeasurementInfo(const shared_ptr<fib::Entry>& entry)
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

Access to Measurement table entries is restricted to the forwarding
strategy that is assigned for that prefix. NFD provides access control
via the `MeasurementsAccessor` class. All attempts to retrieve
Measurement entries must go through this access (acquired in the above
code via `Strategy::getMeasurements()`). This caveat aside, accessing
and storing information on Measurement entries is identical to PIT
entries with the use of the `get/setStrategyInfo` methods. The rest of
the surround code creates a new instance of our custom delay storage
class if one was not already present.


With the Interest clock started and access to our delay measurements
for sending, we now look to actually recording the delay
measurements. To do this, we need to stop the clock when the Data
packet arrives. NFD will notify our strategy when Data arrives before
satisfying the corresponding PIT entry. The strategy's
`beforeSatisfyPendingInterest` method will be invoked once for each PIT entry
that is consumed with the PIT entry, the Face the Data is arrived
over, and the Data itself. We can access the information
stored on the PIT entry by calling `pit::Entry::getStrategyInfo<T>()`
(where `T` is the custom information type, `MyPitInfo` here).

    void
    WeightedLoadBalancerStrategy::beforeSatisfyPendingInterest(shared_ptr<pit::Entry> pitEntry,
                                                               const Face& inFace,
                                                               const Data& data)
    {
      shared_ptr<MyPitInfo> pitInfo = pitEntry->getStrategyInfo<MyPitInfo>();

      // No start time available, cannot compute delay for this retrieval
      if (!static_cast<bool>(pitInfo))
        return;

      const milliseconds delay =
        duration_cast<milliseconds>(system_clock::now() - pitInfo->creationTime);

      MeasurementsAccessor& accessor = this->getMeasurements();

      // Update Face delay measurements and entry lifetimes owned
      // by this strategy while walking up the NameTree
      shared_ptr<measurements::Entry> measurementsEntry = accessor.get(*pitEntry);
      while (static_cast<bool>(measurementsEntry))
        {
          shared_ptr<MyMeasurementInfo> measurementsEntryInfo =
            measurementsEntry->getStrategyInfo<MyMeasurementInfo>();

          if (static_cast<bool>(measurementsEntryInfo))
            {
              accessor.extendLifetime(measurementsEntry, seconds(16));
              measurementsEntryInfo->updateFaceDelay(inFace, delay);
            }

          measurementsEntry = accessor.getParent(measurementsEntry);
        }
    }

The most complicated part of this `beforeSatisfyPendingInterest`
implementation is the recording of delay information. Previously, we
saw how to use a `MeasurementsAccessor` to fetch/create/set
information on a Measurement entry. The above code will also set the
entry's information, but there's one problem: which entry?

Before, we accessed the Measurement table through the use of the FIB
entry's prefix. In `beforeSatisfyPendingInterest` we only have the PIT
entry in hand and that may be *more specific* (i.e. longer) than the
FIB entry we will use when it comes time to send an
Interest. Therefore, it is necessary to update the chain of
Measurement entries with the new information. The entry tree is walked
here in the trailing while loop by using
`MeasurementsAccessor::getParent` method.

We also take this opportunity to refresh the lifetime of our custom
information. Measurement entry information periodically expires and is
garbage collected. We can prevent this from happening by asking
NFD to extend the information's lifetime through the accessor:

    accessor.extendLifetime(measurementsEntry, seconds(16));

Together, `beforeSatisfyPendingInterest` and `afterReceiveInterest`
make up the core of forwarding strategies; everything else is
implementation specific. We have also seen how to store and retrieve
custom information on NFD constructs. You are encouraged to refer to
the
[NFD Developer's Guide](http://named-data.net/wp-content/uploads/2014/07/NFD-developer-guide.pdf)
for more information of storing useful items and additional attachment
points.

To use our new forwarding strategy, we must once again install it by
modifying `daemon/fw/available-strategies.cpp`'s `installStrategies`
method:

        void
        installStrategies(Forwarder& forwarder)
        {
            ...

          installStrategy<WeightedLoadBalancerStrategy>(forwarder);
        }


You will now be able to set `WeightedLoadBalancerStrategy` as the
active strategy at runtime with nfdc's set-strategy command:

        nfdc set-strategy <namespace> ndn:/localhost/nfd/strategy/weighted-load-balancer
