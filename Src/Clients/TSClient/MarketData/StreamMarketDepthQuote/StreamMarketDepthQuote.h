#pragma once

#include <QObject>

#include "Stream.h"
#include "MarketDepthQuote.h"

class StreamMarketDepthQuote final : public Stream {
    Q_OBJECT

public:
    explicit StreamMarketDepthQuote(const QString &symbol, QObject *parent = nullptr);
    ~StreamMarketDepthQuote();
    StreamMarketDepthQuote(const StreamMarketDepthQuote&) = delete;
    StreamMarketDepthQuote& operator=(const StreamMarketDepthQuote&) = delete;

signals:
    void receivedNewMarketDepthQuote(QString symbol, MarketDepthQuote quote);

private:
    bool processJsonObject(const QJsonObject& jsonObj) override;
    QString m_symbol;
};
