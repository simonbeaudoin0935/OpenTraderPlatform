#pragma once

#include <QObject>

#include "StreamBars.h"
#include "StreamReceiver.h"

Q_DECLARE_LOGGING_CATEGORY(BarReceiverLog)

class BarReceiver : public StreamReceiver
{
    Q_OBJECT
  public:
    explicit BarReceiver(const QString& symbol, QObject* parent = nullptr);
    ~BarReceiver();

    [[nodiscard]] const QString& getSymbol() const
    {
        return m_symbol;
    }

    [[nodiscard]] QPointer<StreamBars> getStream() const
    {
        return m_stream;
    }

  signals:
    /**
     * @brief Signal emitted when a new bar is received from the stream
     * 
     * Thread Safety (Architecture_Improvements.md point 1.4):
     * - Emitted from: MainAlgo worker thread (this receiver runs on MainAlgo thread)
     * - Received on: MainAlgo thread (same thread, typically uses Qt::DirectConnection)
     * - Thread-safe: Yes (receivers run on MainAlgo thread, receive from TSClient via queued connection)
     * 
     * Data Flow:
     * 1. StreamBars (TSClient thread) emits newBar signal
     * 2. BarReceiver slot onReceivedNewBar (MainAlgo thread) receives via Qt::QueuedConnection
     * 3. BarReceiver emits receivedNewBar to BarCache (same MainAlgo thread)
     * 
     * @param symbol Stock ticker symbol
     * @param newBar The new bar data
     */
    void receivedNewBar(QString symbol, Bar newBar);

  private slots:
    void onReceivedNewBar(Bar newBar);

  protected:
    [[nodiscard]] QPointer<Stream> getStreamBase() const override
    {
        return QPointer<Stream>(m_stream.data());
    }

  private:
    void createBarStream();

    const QString m_symbol;
    QPointer<StreamBars> m_stream;
};
