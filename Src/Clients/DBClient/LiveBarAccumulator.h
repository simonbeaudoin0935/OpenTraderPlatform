#pragma once

#include <QDateTime>
#include <QLoggingCategory>
#include <QMap>
#include <QObject>
#include <QString>

#include "Core/Models/Bar.h"
#include "Core/Models/Trade.h"

Q_DECLARE_LOGGING_CATEGORY(LiveBarAccumulatorLog)

/**
 * @brief Accumulates individual Trade records into forming 1-minute OHLCV bars.
 *
 * Databento streams raw trade prints (TradeMsg → Trade), not pre-built bars.
 * This class maintains one forming bar per symbol and emits signals on bar
 * updates (every trade) and bar closures (minute boundary rollover).
 *
 * ## Bar timestamp convention
 * Bars are timestamped with the **close time** of the interval, matching BarCache convention:
 *   - A trade at 09:31:04 contributes to the bar timestamped 09:32:00
 *   - The bar covering 09:31:00–09:31:59 is timestamped 09:32
 *
 * ## Threading
 * This object should live on the MainAlgo thread. Connect DBClient::newTrade → onNewTrade.
 * Qt AutoConnection handles the cross-thread dispatch from Databento's callback thread.
 *
 * @see DBClient::newTrade, BarReceiver::receivedNewBar
 */
class LiveBarAccumulator : public QObject
{
    Q_OBJECT

  public:
    explicit LiveBarAccumulator(QObject* p_parent = nullptr);

  public slots:
    /**
     * @brief Process an incoming trade and update the forming bar.
     *
     * If the trade falls in a new minute, the previous forming bar is closed
     * and emitted via barClosed(), then a new bar is started.
     *
     * @param p_symbol Ticker symbol
     * @param p_trade  The trade data
     */
    void onNewTrade(const QString& p_symbol, const Trade& p_trade);

  signals:
    /**
     * @brief Emitted when a 1-minute bar is completed (minute boundary crossed).
     * Thread context: Emitted from MainAlgo thread (same thread as onNewTrade slot)
     * @param p_symbol Ticker symbol
     * @param p_bar    The completed bar (BarStatus::Closed)
     */
    void barClosed(const QString& p_symbol, const Bar& p_bar);

    /**
     * @brief Emitted on every trade with the updated forming bar.
     * Thread context: Emitted from MainAlgo thread
     * @param p_symbol Ticker symbol
     * @param p_bar    The in-progress bar (BarStatus::Open)
     */
    void barUpdated(const QString& p_symbol, const Bar& p_bar);

  private:
    /**
     * @brief Internal state for a forming bar (mutable, unlike Bar which is constructed immutably).
     */
    struct FormingBar
    {
        QDateTime barCloseTime; ///< The close-time timestamp for this bar's minute
        float open = 0.0f;
        float high = 0.0f;
        float low = 0.0f;
        float close = 0.0f;
        quint64 volume = 0;
    };

    /**
     * @brief Compute the bar close-time for a given trade timestamp.
     * A trade at 09:31:04 → bar close time 09:32:00 (ceiling to next minute).
     */
    [[nodiscard]] static QDateTime barCloseTimeForTrade(const QDateTime& p_tradeTime);

    /**
     * @brief Convert internal FormingBar to an immutable Bar object.
     */
    [[nodiscard]] static Bar toBar(const FormingBar& p_forming, Bar::BarStatus p_status);

    QMap<QString, FormingBar> m_formingBars;
};
