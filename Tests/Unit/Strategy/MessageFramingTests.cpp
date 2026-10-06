#include <QTest>
#include <algorithm>
#include <limits>

#include "OpenTraderPlatform/StrategySDK/MessageFraming.h"
#include "opentraderplatform_strategy_v1.pb.h"

namespace SDK = OpenTraderPlatform::StrategySDK;
namespace Protocol = opentraderplatform::strategy::v1;

class MessageFramingTests : public QObject
{
    Q_OBJECT

  private slots:
    void prefixEncoding()
    {
        const auto prefix = SDK::encodeFrameSize(0x12345678U);
        QCOMPARE(prefix[0], std::uint8_t(0x12));
        QCOMPARE(prefix[1], std::uint8_t(0x34));
        QCOMPARE(prefix[2], std::uint8_t(0x56));
        QCOMPARE(prefix[3], std::uint8_t(0x78));
        for (auto size: {0U, 1U, 255U, 256U, 65535U, std::numeric_limits<std::uint32_t>::max()})
            QCOMPARE(SDK::decodeFrameSize(SDK::encodeFrameSize(size)), size);
    }

    void protobufRoundTrip()
    {
        Protocol::HostToStrategyEnvelope original;
        original.mutable_shutdown()->set_reason("Test shutdown");
        const auto frame = SDK::serializeFramedMessage(original);
        QVERIFY(frame.size() >= SDK::kFramePrefixSize);
        std::array<std::uint8_t, SDK::kFramePrefixSize> prefix;
        std::copy_n(frame.begin(), prefix.size(), prefix.begin());
        QCOMPARE(static_cast<std::size_t>(SDK::decodeFrameSize(prefix)), original.ByteSizeLong());
        const auto payload = std::span<const std::uint8_t>(frame).subspan(SDK::kFramePrefixSize);
        Protocol::HostToStrategyEnvelope parsed;
        QVERIFY(SDK::parseMessagePayload(payload, &parsed));
        QCOMPARE(parsed.SerializeAsString(), original.SerializeAsString());
        QVERIFY(!SDK::parseMessagePayload(payload, nullptr));
        QVERIFY(!SDK::parseMessagePayload(payload.first(payload.size() - 1), &parsed));
    }

    void malformedPayloadIsRejected()
    {
        Protocol::HostToStrategyEnvelope parsed;
        const std::array<std::uint8_t, 1> invalid{0xff};
        QVERIFY(!SDK::parseMessagePayload(invalid, &parsed));
    }

    void emptyMessageRoundTrip()
    {
        Protocol::HostToStrategyEnvelope original;
        const auto frame = SDK::serializeFramedMessage(original);
        QCOMPARE(frame.size(), SDK::kFramePrefixSize);
        Protocol::HostToStrategyEnvelope parsed;
        QVERIFY(SDK::parseMessagePayload(std::span<const std::uint8_t>(frame).subspan(SDK::kFramePrefixSize), &parsed));
        QVERIFY(!parsed.has_shutdown());
    }
};

QTEST_GUILESS_MAIN(MessageFramingTests)
#include "MessageFramingTests.moc"
