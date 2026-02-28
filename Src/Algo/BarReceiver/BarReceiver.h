#pragma once

#include <QObject>

#include "Bar.h"
#include "StreamReceiver.h"

Q_DECLARE_LOGGING_CATEGORY(BarReceiverLog)

class BarReceiver : public StreamReceiver
{
    Q_OBJECT
  public:
    explicit BarReceiver(const QString& symbol, QObject* parent = nullptr);
    ~BarReceiver() override = default;

    [[nodiscard]] const QString& getSymbol() const
    {
        return m_symbol;
    }

  signals:
    /**
     * @brief Signal emitted when a new bar is received
     * Thread context: Emitted from MainAlgo worker thread
     */
    void receivedNewBar(QString symbol, Bar newBar);

  protected:
    [[nodiscard]] QPointer<Stream> getStreamBase() const override
    {
        return nullptr; // Not used — bars arrive via LiveBarAccumulator signal chain
    }

  private:
    const QString m_symbol;
};
