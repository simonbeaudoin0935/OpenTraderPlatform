#pragma once

#include <QDateTime>
#include <QLoggingCategory>
#include <QMap>
#include <QObject>
#include <QString>

#include <optional>

#include "Core/Models/Bar.h"
#include "Core/Models/Trade.h"

Q_DECLARE_LOGGING_CATEGORY(LiveBarAccumulatorLog)

/**
 * @brief Accumulates individual Trade records into forming OHLCV bars of configurable interval.
 *
 * Databento streams raw trade prints (TradeMsg → Trade), not pre-built bars.
 * This class maintains one forming bar per symbol and emits signals on bar
 * updates (every trade) and bar closures (interval boundary rollover).
 *
 * ## Bar timestamp convention
 * Bars are timestamped with the **open time** of the interval (Databento convention):
 *   - For 60s interval: a trade at 09:31:04 contributes to bar timestamped 09:31:00
 *   - For 10s interval: a trade at 09:31:04 contributes to bar timestamped 09:31:00
 *   - A trade at 09:31:10 contributes to bar timestamped 09:31:10
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
    /**
     * @param p_parent         Qt parent
     * @param p_intervalSeconds Bar interval in seconds (default 60 = 1 minute, 10 = 10 seconds)
     */
    explicit LiveBarAccumulator(QObject* p_parent = nullptr, int p_intervalSeconds = 60);

    /**
     * @brief Return the current forming bar for a symbol, if one exists.
     *
     * Useful for seeding the DisplaySnapshot immediately after a symbol switch
     * so the chart does not have to wait for the next trade to show the live bar.
     *
     * @param p_symbol Ticker symbol
     * @return The forming bar as an immutable Bar (BarStatus::Open), or std::nullopt if none.
     */
    [[nodiscard]] std::optional<Bar> getFormingBar(const QString& p_symbol) const;

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
     * @brief Emitted when a bar is completed (interval boundary crossed).
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
        QDateTime barOpenTime; ///< The open-time timestamp for this bar's interval
        float open = 0.0f;
        float high = 0.0f;
        float low = 0.0f;
        float close = 0.0f;
        quint64 volume = 0;
    };

    /**
     * @brief Compute the bar open-time for a given trade timestamp.
     * Floors to the current interval boundary (e.g. 10-second or 1-minute).
     */
    [[nodiscard]] QDateTime barOpenTimeForTrade(const QDateTime& p_tradeTime) const;

    /**
     * @brief Convert internal FormingBar to an immutable Bar object.
     */
    [[nodiscard]] static Bar toBar(const FormingBar& p_forming, Bar::BarStatus p_status);

    QMap<QString, FormingBar> m_formingBars;
    int m_intervalSeconds; ///< Bar interval in seconds (e.g. 10 or 60)
};
