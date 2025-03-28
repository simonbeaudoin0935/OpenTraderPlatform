#ifndef TRADESTATIONCLIENT_H
#define TRADESTATIONCLIENT_H

#include <QObject>

#include <QLoggingCategory>
#include <QVector>

#include "../restclient.h"

Q_DECLARE_LOGGING_CATEGORY(TradeStationClientLog)

// This is a singleton


class TradeStationClient : public RESTClient {
    Q_OBJECT
public:

    // Singleton : Instance getter
    static TradeStationClient& getInstance();
    static TradeStationClient* getInstancePtr();
    // Singleton : Delete copy constructor and assignment operator
    TradeStationClient(const TradeStationClient&) = delete;
    TradeStationClient& operator=(const TradeStationClient&) = delete;



signals:

private slots:

private:
    // Singleton : private constructor
    explicit TradeStationClient();
    ~TradeStationClient();

    enum class RequestType {
        None
    };

    void emitSignalDemuxer(RequestTypeInt type, const QJsonArray &doc);

    // Singleton
    static TradeStationClient* instance;

    friend class TestTradeStationClient;
};

#endif // TRADESTATIONCLIENT_H 
