#include <QTest>

#include "StrategyFrameReader.h"

namespace SDK = OpenTraderPlatform::StrategySDK;

class StrategyFrameReaderTests : public QObject
{
    Q_OBJECT

  private slots:
    void fragmentedFramePreservesIncompleteInput()
    {
        const QByteArray payload("frame payload");
        const auto prefix = SDK::encodeFrameSize(static_cast<std::uint32_t>(payload.size()));
        const QByteArray frame = QByteArray(reinterpret_cast<const char*>(prefix.data()), prefix.size()) + payload;
        QByteArray buffer;
        for (qsizetype i = 0; i < frame.size(); ++i)
        {
            buffer.append(frame.at(i));
            const QByteArray before = buffer;
            const auto result = StrategyFrameReader::takeFrame(buffer);
            QVERIFY(result.has_value());
            if (i + 1 < frame.size())
            {
                QVERIFY(!result->has_value());
                QCOMPARE(buffer, before);
            }
            else
            {
                QVERIFY(result->has_value());
                QCOMPARE(QByteArray(reinterpret_cast<const char*>(result->value().data()), result->value().size()),
                         payload);
                QVERIFY(buffer.isEmpty());
            }
        }
    }

    void coalescedFramesRetainPartialTail()
    {
        const auto prefix = SDK::encodeFrameSize(1);
        const QByteArray first = QByteArray(reinterpret_cast<const char*>(prefix.data()), prefix.size()) + 'a';
        const QByteArray second = QByteArray(reinterpret_cast<const char*>(prefix.data()), prefix.size()) + 'b';
        QByteArray buffer = first + second + first.left(2);
        for (const std::uint8_t expected: {std::uint8_t('a'), std::uint8_t('b')})
        {
            auto result = StrategyFrameReader::takeFrame(buffer);
            QVERIFY(result.has_value() && result->has_value());
            QCOMPARE(result->value().size(), std::size_t(1));
            QCOMPARE(result->value().front(), expected);
        }
        QCOMPARE(buffer, first.left(2));
        auto incomplete = StrategyFrameReader::takeFrame(buffer);
        QVERIFY(incomplete.has_value() && !incomplete->has_value());
    }

    void acceptsExactLimitAndRejectsLimitPlusOne()
    {
        const auto maximumPrefix = SDK::encodeFrameSize(SDK::kMaxFramePayloadSize);
        QByteArray buffer(reinterpret_cast<const char*>(maximumPrefix.data()), maximumPrefix.size());
        auto incomplete = StrategyFrameReader::takeFrame(buffer);
        QVERIFY(incomplete.has_value() && !incomplete->has_value());
        buffer.append(QByteArray(SDK::kMaxFramePayloadSize, 'x'));
        auto maximum = StrategyFrameReader::takeFrame(buffer);
        QVERIFY(maximum.has_value() && maximum->has_value());
        QCOMPARE(maximum->value().size(), static_cast<std::size_t>(SDK::kMaxFramePayloadSize));
        QVERIFY(buffer.isEmpty());
        const auto oversizePrefix = SDK::encodeFrameSize(SDK::kMaxFramePayloadSize + 1);
        buffer = QByteArray(reinterpret_cast<const char*>(oversizePrefix.data()), oversizePrefix.size());
        auto rejected = StrategyFrameReader::takeFrame(buffer);
        QVERIFY(!rejected.has_value());
        QVERIFY(!rejected.error().isEmpty());
        QCOMPARE(buffer.size(), qsizetype(4));
    }

    void acceptsEmptyFrame()
    {
        QByteArray buffer(4, '\0');
        auto result = StrategyFrameReader::takeFrame(buffer);
        QVERIFY(result.has_value() && result->has_value());
        QVERIFY(result->value().empty());
        QVERIFY(buffer.isEmpty());
    }
};

QTEST_GUILESS_MAIN(StrategyFrameReaderTests)
#include "StrategyFrameReaderTests.moc"
