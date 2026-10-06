# SDK framing unit tests

[MessageFramingTests.cpp](MessageFramingTests.cpp) covers big-endian frame-size
encoding at boundaries, protobuf serialization/payload round trips, null output
pointers, truncated/malformed payload rejection, and empty protobuf messages.

It links the public StrategySDK rather than the complete platform runtime.
Do not allocate multi-gigabyte buffers just to exercise length guards.
MessageFraming only encodes prefixes and parses a supplied payload; it does not
buffer socket input. Split packets, coalesced frames, advertised-length limits,
and disconnect mid-frame need tests of the socket/runtime consumer, not a fake
parser invented in this suite.
