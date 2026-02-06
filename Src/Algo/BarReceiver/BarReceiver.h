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
