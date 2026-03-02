#include "PositionsReceiver.h"
#include "TSClient.h"
#include "Logging.h"
#include "Assume.h"

#include <QFutureWatcher>
#include <QTimer>

#define LOGGING_CATEGORY PositionsReceiverLog
Q_LOGGING_CATEGORY(PositionsReceiverLog, "PositionsReceiver");

PositionsReceiver::PositionsReceiver(const QString& account, QObject* parent)
    : StreamReceiver(parent), m_account(account)
{
    this->setObjectName("PositionReceiver");

    DEBUG << "Starting Positions stream for account " << account;

    // Get the singleton database instance
    m_database = PositionsDatabase::getInstance();
    Q_CHECK_PTR(m_database);

    if (!m_database->isOpen())
    {
        CRITICAL << "Failed to open positions database";
        // Failed to open positions database
        Q_UNREACHABLE();
    }

    // Load existing positions from database
    auto existingPositions = m_database->loadAllPositions();
    INFO << "Loaded" << existingPositions.size() << "positions from database";

    // Emit the loaded positions
    if (!existingPositions.isEmpty())
    {
        emit loadedPositionsFromDatabase(m_account, existingPositions);
    }

    createPositionsStream();
}

PositionsReceiver::~PositionsReceiver()
{
    // Stream may be null if stopStream() was called before destruction
    if (m_stream != nullptr)
    {
        TSClient::getInstance()->closeStream(m_stream);
    }
}

void PositionsReceiver::stopStream(const QString& account)
{
    Q_UNUSED(account);
    if (m_stream != nullptr)
    {
        TSClient::getInstance()->closeStream(m_stream);
        m_stream = nullptr;
        DEBUG << "Positions stream stopped for account" << m_account;
    }
}

void PositionsReceiver::stopStream(const char* account)
{
    stopStream(QString(account));
}

void PositionsReceiver::createPositionsStream()
{
    DEBUG << "Starting Positions stream for account " << m_account;

    m_stream = TSClient::getInstance()->openStreamPositions(m_account);
    Q_CHECK_PTR(m_stream);

    connect(m_stream, &StreamPositions::newPositionReceived, this, &PositionsReceiver::onReceivedNewPosition);
    connect(m_stream, &StreamPositions::positionDeleted, this, &PositionsReceiver::onPositionDeleted);
    connect(m_stream,
            &StreamPositions::endSnapshotReceived,
            this,
            [this]()
            {
                INFO << "Received EndSnapshot for Positions stream";
                m_receivedEndSnapshot = true;
            });

    connect(m_stream,
            &Stream::streamClosed,
            this,
            [this](Stream::StreamError reason, QString message)
            {
                if (reason != Stream::StreamError::Closed)
                {
                    CRITICAL << "Positions stream for account" << m_account << "finished with error:" << message;
                    m_stream = nullptr;
                    m_receivedEndSnapshot = false;
                    QTimer::singleShot(300, this, &PositionsReceiver::createPositionsStream);
                }
                else
                {
                    DEBUG << "Positions stream for account" << m_account << "intentionally closed";
                }
            });
}


void PositionsReceiver::onReceivedNewPosition(Position position)
{
    qCDebug(PositionsReceiverLog).noquote()
        << "New position for account (" << m_account << ") : " << position.toJsonString();

    QString positionID = position.getPositionID();
    QDateTime currentTime = QDateTime::currentDateTime();

    double quantity = position.getQuantity().toDouble();
    double avgPrice = position.getAveragePrice().toDouble();
    constexpr double EPSILON = 1e-9;
    bool quantityIsZero = (std::abs(quantity) < EPSILON);

    // Only write to the database on structural changes (open / add / reduce / close).
    // Pure mark-to-market updates (same qty + same avg price) are skipped.
    bool positionExistsInDB = m_database->positionExists(positionID);

    auto structureChanged = [&]() -> bool
    {
        auto it = m_lastKnownStructure.find(positionID);
        if (it == m_lastKnownStructure.end())
            return true; // Never seen before
        return std::abs(it->quantity - quantity) > EPSILON || std::abs(it->averagePrice - avgPrice) > EPSILON;
    };

    if (!positionExistsInDB)
    {
        // New position — always insert
        QDateTime openedTime = currentTime;
        m_positionOpenedTimes[positionID] = openedTime;
        m_database->insertPosition(position, openedTime);
        m_lastKnownStructure[positionID] = {quantity, avgPrice};
        DEBUG << "Stored new position" << positionID << "in database, opened at" << openedTime.toString(Qt::ISODate);
    }
    else if (structureChanged())
    {
        // Structural change (qty or avg price changed) — update database
        std::optional<QDateTime> closedTime;
        if (quantityIsZero)
        {
            closedTime = currentTime;
            DEBUG << "Position" << positionID << "closed at" << currentTime.toString(Qt::ISODate);
            m_positionOpenedTimes.remove(positionID);
            m_lastKnownStructure.remove(positionID);
        }
        else
        {
            m_lastKnownStructure[positionID] = {quantity, avgPrice};
        }

        m_database->updatePosition(position, closedTime);
        DEBUG << "Updated position" << positionID << "in database";
    }
    // else: pure mark-to-market update — skip DB write entirely

    emit receivedNewPosition(m_account, position);
}

void PositionsReceiver::onPositionDeleted(QString positionID)
{
    qCDebug(PositionsReceiverLog) << "Position deleted for account (" << m_account << ") : " << positionID;
    emit positionDeleted(m_account, positionID);
}
