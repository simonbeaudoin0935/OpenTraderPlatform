#pragma once

#include <QObject>

#include "StreamBars.h"

Q_DECLARE_LOGGING_CATEGORY(BarReceiverLog)

class BarReceiver : public QObject
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

  private:
    void createBarStream();

    const QString m_symbol;
    QPointer<StreamBars> m_stream;
};
