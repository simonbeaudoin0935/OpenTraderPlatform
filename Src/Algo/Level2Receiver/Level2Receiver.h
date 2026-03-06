#pragma once

#include <QObject>

#include "Assume.h"
#include "Level2.h"
#include "StreamReceiver.h"

Q_DECLARE_LOGGING_CATEGORY(Level2ReceiverLog)

/**
 * @brief Receives and processes Level 2 (full 10-level book) updates for a single symbol.
 *
 * Subscribes to Databento Schema::Mbp10 via DBClient (Phase 6).
 * Routes the raw Level2 snapshot to connected consumers (GUI, strategies).
 * Any metric computation (BAI, DWP, etc.) is the responsibility of strategies.
 *
 * Thread context: lives on MainAlgo worker thread.
 */
class Level2Receiver : public StreamReceiver
{
    Q_OBJECT
  public:
    explicit Level2Receiver(const QString& symbol, QObject* parent = nullptr);
    ~Level2Receiver() override = default;

  signals:
    /**
     * @brief Emitted when a new Level 2 snapshot is received.
     * Thread context: Emitted from MainAlgo worker thread.
     */
    void receivedNewLevel2(QString symbol, Level2 level2);

  public slots:
    void onReceivedNewLevel2(Level2 level2);

  protected:
    [[nodiscard]] QPointer<Stream> getStreamBase() const override
    {
        return nullptr;
    }

  private:
    void openStream();

    QString m_symbol;
};
