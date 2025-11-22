#pragma once

#include <QTimer>
#include <QObject>
#include <QLoggingCategory>

#include "StockNewsFilter.h"
#include "Ticker.h"


Q_DECLARE_LOGGING_CATEGORY(BreakingNewsFetcherLog)

class BreakingNewsFetcher : public QObject
{
    Q_OBJECT
public:
    explicit BreakingNewsFetcher(QObject *parent = nullptr);

    void start(QVector<Ticker> &symbolsScreenedByFloat,
               int newsFetchingInterval,
               int newsFetchDepthLimit);

    void stop();

signals:
    void foundNewNews(StockNewsResult newNews);

private slots:
    void onStockNewsReceived(QVector<StockNewsResult> results);

private:
    bool stopped = true;

    void fetchSyncNewsStockScreenedByFloat();
    void fetchAsyncNewsStockScreenedByFloat();

    void processReceivedNews();
    void processReceivedNewsPingPongBuffers();

    void alternateNewsPerSymbolPingPong();

    int fetchingInterval = -1;
    int fetchDepthLimit = -1;

    QVector<Ticker> symbolsToWatch;

    QMap<Ticker, QVector<StockNewsResult>> latestNewsPerSymbolPingPong1;
    QMap<Ticker, QVector<StockNewsResult>> latestNewsPerSymbolPingPong2;

    QMap<Ticker, QVector<StockNewsResult>> *latestNewsPerSymbolPingPongPtr;
};
