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
     * Thread context:
     * - Emitted from: MainAlgo worker thread
     * - Data flow: StreamBars (TSClient thread) → BarReceiver slot (MainAlgo thread, queued) → this signal
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
