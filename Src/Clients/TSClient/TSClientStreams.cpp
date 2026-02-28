// Market data streaming removed — now handled by DBClient

#include "TSClient.h"
#include "Logging.h"
#include "Assume.h"
#include "Stream/MockNetworkReply.h"
#include "Stream/MockNetworkAccessManager.h"
#include "OrderEmulator.h"
#include "CONSTANTS.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

#define LOGGING_CATEGORY TSClientLog

QPointer<StreamPositions> TSClient::openStreamPositions(const QString& accountID, bool changes)
{
    // In replay mode, accept the simulated account ID
    if (m_mode == Mode::Replay)
    {
        OBJ_ASSUME_EQUAL(accountID, QString("SIM123456"));
    }
    else
    {
        // normal account numbers have 8 digits, sim have additional letters
        OBJ_ASSUME_GTE(accountID.length(), 8);
    }

    DEBUG << "Opening StreamPositions for account " << accountID << " with changes=" << changes;

    QPointer<StreamPositions> stream;

    if (m_mode == Mode::Replay)
    {
        DEBUG << "Opening replay StreamPositions for account" << accountID;

        QMetaObject::invokeMethod(
            this,
            [this, &stream, &accountID]()
            {
                auto* mockReply = new MockNetworkReply(this);
                Q_CHECK_PTR(mockReply);

                // Track the mock reply for position updates from OrderEmulator
                m_replayPositionsReply = mockReply;

                // Start heartbeat for stream keep-alive
                mockReply->startHeartbeat(StreamConstants::MOCK_HEARTBEAT_INTERVAL_MS);

                stream = new StreamPositions(accountID, mockReply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                // Connect OrderEmulator position updates to the mock reply
                if (m_orderEmulator)
                {
                    connect(m_orderEmulator, &OrderEmulator::positionUpdate, mockReply, &MockNetworkReply::injectData);
                }

                // Send initial empty snapshot with EndSnapshot event
                QJsonObject endSnapshot;
                endSnapshot["StreamStatus"] = "EndSnapshot";
                mockReply->injectData(QJsonDocument(endSnapshot).toJson(QJsonDocument::Compact) + "\n");

                INFO << "Opened replay StreamPositions for account" << accountID;
            },
            Qt::BlockingQueuedConnection);
    }
    else
    {
        // Live mode - make real network request
        QUrlQuery query;
        query.addQueryItem("changes", changes ? "true" : "false");

        QNetworkRequest request =
            buildNetworkRequest(QString(TSClientEndpoints::STREAM_POSITIONS).arg(accountID), query);

        QMetaObject::invokeMethod(
            this,
            [this, &request, &stream, &accountID]()
            {
                QNetworkReply* reply = m_networkManager->get(request);
                Q_CHECK_PTR(reply);

                stream = new StreamPositions(accountID, reply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                // Don't emit signal for Positions stream (singleton, always 0 or 1)
            },
            Qt::BlockingQueuedConnection);
    }

    return stream;
}

QPointer<StreamOrders> TSClient::openStreamOrders(const QString& accountID)
{
    // In replay mode, accept the simulated account ID
    if (m_mode == Mode::Replay)
    {
        OBJ_ASSUME_EQUAL(accountID, QString("SIM123456"));
    }
    else
    {
        // normal account numbers have 8 digits, sim have additional letters
        OBJ_ASSUME_GTE(accountID.length(), 8);
    }

    DEBUG << "Opening StreamOrders for account" << accountID;

    QPointer<StreamOrders> stream;

    if (m_mode == Mode::Replay)
    {
        DEBUG << "Opening replay StreamOrders for account" << accountID;

        QMetaObject::invokeMethod(
            this,
            [this, &stream, &accountID]()
            {
                auto* mockReply = new MockNetworkReply(this);
                Q_CHECK_PTR(mockReply);

                // Track the mock reply for order updates from OrderEmulator
                m_replayOrdersReply = mockReply;

                // Start heartbeat for stream keep-alive
                mockReply->startHeartbeat(StreamConstants::MOCK_HEARTBEAT_INTERVAL_MS);

                stream = new StreamOrders(accountID, mockReply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                // Connect OrderEmulator order updates to the mock reply
                if (m_orderEmulator)
                {
                    connect(m_orderEmulator,
                            &OrderEmulator::orderStatusUpdate,
                            mockReply,
                            &MockNetworkReply::injectData);
                }

                // Send initial empty snapshot with EndSnapshot event
                QJsonObject endSnapshot;
                endSnapshot["StreamStatus"] = "EndSnapshot";
                mockReply->injectData(QJsonDocument(endSnapshot).toJson(QJsonDocument::Compact) + "\n");

                INFO << "Opened replay StreamOrders for account" << accountID;
            },
            Qt::BlockingQueuedConnection);
    }
    else
    {
        // Live mode - make real network request
        const QString endpoint = QString(TSClientEndpoints::STREAM_ORDERS).arg(accountID);
        QNetworkRequest request = buildNetworkRequest(endpoint);

        QMetaObject::invokeMethod(
            this,
            [this, &request, &stream, &accountID]()
            {
                QNetworkReply* reply = m_networkManager->get(request);
                Q_CHECK_PTR(reply);

                stream = new StreamOrders(accountID, reply, this);
                Q_CHECK_PTR(stream);

                auto c =
                    connect(stream, &Stream::newAmountOfDataReceived, this, &TSClient::processNewAmountOfDataReceived);
                OBJ_ASSUME_TRUE(c);

                // Don't emit signal for Orders stream (singleton, always 0 or 1)
            },
            Qt::BlockingQueuedConnection);
    }

    return stream;
}

void TSClient::closeStream(Stream* const stream)
{
    OBJ_ASSUME_DIFF(stream, nullptr);

    QMetaObject::invokeMethod(this, [stream]() { stream->deleteLater(); }, Qt::QueuedConnection);
}

// ============================================================================
// Replay Mode Support
// ============================================================================

void TSClient::setMode(Mode p_mode)
{
    if (m_mode == p_mode)
    {
        return;
    }

    INFO << "TSClient mode changing from" << (m_mode == Mode::Live ? "Live" : "Replay") << "to"
         << (p_mode == Mode::Live ? "Live" : "Replay");

    m_mode = p_mode;

    if (p_mode == Mode::Replay)
    {
        // Generate replay session timestamp
        m_replaySessionTimestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss");
        INFO << "Replay session timestamp:" << m_replaySessionTimestamp;

        // Create OrderEmulator
        m_orderEmulator = new OrderEmulator(this);

        // Create MockNetworkAccessManager (routes order requests to emulator)
        m_mockNetworkManager = new MockNetworkAccessManager(m_orderEmulator, this);
    }
    else
    {
        // Returning to Live mode - cleanup replay resources

        // Delete replay streams BEFORE cleaning up emulator
        // This ensures StreamOrders/StreamPositions are destroyed before MockNetworkReply
        if (!m_replayOrdersReply.isNull())
        {
            // Find and delete the StreamOrders that owns this MockNetworkReply
            for (QObject* child: children())
            {
                if (auto* stream = qobject_cast<StreamOrders*>(child))
                {
                    delete stream;
                    break;
                }
            }
            m_replayOrdersReply.clear();
        }

        if (!m_replayPositionsReply.isNull())
        {
            // Find and delete the StreamPositions that owns this MockNetworkReply
            for (QObject* child: children())
            {
                if (auto* stream = qobject_cast<StreamPositions*>(child))
                {
                    delete stream;
                    break;
                }
            }
            m_replayPositionsReply.clear();
        }

        // Cancel any pending orders in emulator
        if (m_orderEmulator)
        {
            m_orderEmulator->clear();
        }

        // Delete mock objects
        delete m_orderEmulator;
        m_orderEmulator = nullptr;

        delete m_mockNetworkManager;
        m_mockNetworkManager = nullptr;

        m_replaySessionTimestamp.clear();

        // Clear replay stream tracking
        m_replayOrdersReply.clear();
        m_replayPositionsReply.clear();
    }
}
