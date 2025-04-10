#pragma once

#include <QObject>
#include <QLoggingCategory>
#include <QMap>

#include "StreamBars.h"

Q_DECLARE_LOGGING_CATEGORY(StockBarsReceiverLog)

class StockBarsReceiver : public QObject
{
    Q_OBJECT
public:
    explicit StockBarsReceiver(QObject *parent = nullptr);

    void startStream(QString &symbol);
    void startStream(const char* symbol);

    void stopStream(QString &symbol);
    void stopStream(const char* symbol);

signals:
    void currentHighlightedReceivedNewBar(QString symbol, Bar bar);

private slots:
    void onReceivedNewBar(QString symbol, Bar bar);
    void onStreamError(Stream::StreamError error, QString errorMessage);

private:
    QMap<QString, StreamBars*> streams;
};
