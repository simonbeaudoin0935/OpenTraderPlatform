#pragma once

#include <QDateTime>
#include <QFutureWatcher>
#include <QMap>
#include <QObject>

#include "OrdersDatabase.h"
#include "StreamOrders.h"
#include "StreamReceiver.h"

Q_DECLARE_LOGGING_CATEGORY(OrdersReceiverLog)

class OrdersReceiver : public StreamReceiver
{
    Q_OBJECT
  public:
    explicit OrdersReceiver(const QString& p_account, QObject* p_parent = nullptr);
    ~OrdersReceiver() override;

    void stopStream(const QString& p_account);
    void stopStream(const char* p_account);

    QPointer<StreamOrders> getStream() const
    {
        return m_stream;
    }

  signals:
    /**
     * @brief Signal emitted when a new order is received from the stream
     *
     * Thread context: Emitted from MainAlgo worker thread
     * Data flow: StreamOrders (TSClient thread) → OrdersReceiver slot (MainAlgo thread, queued) → this signal
     */
    void receivedNewOrder(QString account, Order order);

  private slots:
    void onReceivedNewOrder(Order order);
    void onEndSnapshotReceived();

  protected:
    [[nodiscard]] QPointer<Stream> getStreamBase() const override
    {
        return QPointer<Stream>(m_stream.data());
    }

  private:
    QPointer<StreamOrders> m_stream = nullptr;
    QString m_account;
    OrdersDatabase* m_database = nullptr;
    bool m_receivedEndSnapshot = false;

    // Track orders for the initial snapshot validation
    QMap<QString, QDateTime> m_snapshotOrders; // orderID -> received time

    // Cache of loaded order latencies from database
    QMap<QString, std::optional<qint64>> m_loadedLatencies; // orderID -> latencyMs

    void createOrdersStream();
    void validateSnapshotOrders();
};
