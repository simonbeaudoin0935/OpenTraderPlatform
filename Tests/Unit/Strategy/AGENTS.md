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

[StrategyFrameReaderTests.cpp](StrategyFrameReaderTests.cpp) tests the parser
actually used by the host receive loop: bytewise prefix/payload buffering,
multiple frames with an incomplete tail, empty frames, exact 16 MiB acceptance,
and rejection at limit plus one without allocating the advertised payload.
The host consumes frames after every socket read rather than buffering an
unbounded burst before inspecting lengths. This is parser coverage, not an
end-to-end host child-process launch fixture.
