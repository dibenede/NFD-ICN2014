Code Walkthrough Source for NDNcomm 2014
==============================================

## Included in this directory

This directory includes the Python application
examples. **Note**: [PyNDN2](https://github.com/named-data/PyNDN2)
is a pre-requesite for runnings these applications.

### Hello World Applications

- hello_consumer.py
- hello_producer.py

### Extended Hello World Applications

- hello_ext_consumer.py
- hello_ext_producer.py

### Forwarding Strategy Demo Helper Applications

- consumer.py
- producer.py

## Not Included in this directory

### Forwarding Strategy Files

- random-load-balancer-strategy.{hpp, cpp}
- weighted-load-balancer-strategy.{hpp, cpp}
- (modified) available-strategies.cpp

The example forwarding strategy source code is colocated with
the stock NFD strategies. From the project root, you can
find all forwarding strategies under `daemon/fw/`
(or `../daemon/fw/` relative to this README).

available-strategies.cpp is a standard part NFD, but has
been modified to "install" the example forwarding strategies.

Consequently, the example forwarding strategies can be compiled
as part of the normal NFD build process. Please see the
[NFD homepage](http://named-data.net/doc/NFD/) for
step-by-step build and installation instructions.

# Code Walkthrough

**TODO**
