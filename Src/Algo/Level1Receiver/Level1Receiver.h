#pragma once

#include <QObject>

#include "Level2.h"
#include "StreamReceiver.h"

Q_DECLARE_LOGGING_CATEGORY(Level1ReceiverLog)

/**
 * @brief Receives Level 1 BBO updates for a single symbol.
 *
 * Subscribes to Databento Schema::Mbp1 via DBClient (Phase 6).
 * Lightweight alternative to Level2Receiver for secondary monitored stocks.
 *
 * Thread context: lives on MainAlgo worker thread.
 */
class Level1Receiver : public StreamReceiver
{
    Q_OBJECT
  public:
    explicit Level1Receiver(const QString& symbol, QObject* parent = nullptr);
    ~Level1Receiver() override = default;

  signals:
    /**
     * @brief Emitted when a new Level 1 BBO snapshot is received.
     * Thread context: Emitted from MainAlgo worker thread.
     */
    void receivedNewLevel1(QString symbol, Level1 level1);

  public slots:
    void onReceivedNewLevel1(Level1 level1);

  protected:
    [[nodiscard]] QPointer<Stream> getStreamBase() const override
    {
        return nullptr;
    }

  private:
    void openStream();

    QString m_symbol;
};
