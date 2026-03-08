#pragma once

#include <QObject>
#include <QMap>
#include <QLoggingCategory>

#include "Bar.h"
#include "TimeFrame.h"

Q_DECLARE_LOGGING_CATEGORY(BarAggregatorLog)

/**
 * @brief Aggregates live 1-minute bars into higher timescale bars.
 *
 * Listens for closed 1m bars and accumulates them into in-progress bars for
 * 5m, 15m, 30m, 1h, 4h, 1d, 1w, and 1M timescales.
 *
 * Each time a 1m bar arrives:
 * - Every higher-TF bar is updated (barUpdated emitted for live candle display).
 * - When an accumulation period ends, the completed bar is emitted via barClosed.
 *
 * ## Threading
 * Runs on the MainAlgo worker thread. All signals are emitted on that thread.
 *
 * ## Usage
 * Connect BarReceiver::receivedNewBar (closed bars only) → BarAggregator::onNewBar.
 * Connect BarAggregator::barClosed → BarCache::storeBar (for each higher TF).
 */
class BarAggregator : public QObject
{
    Q_OBJECT

  public:
    explicit BarAggregator(QObject* parent = nullptr);
    ~BarAggregator() override = default;

  public slots:
    /**
     * @brief Process an incoming live bar.
     *
     * Should be connected to BarReceiver::receivedNewBar.
     * Only closed (status == Closed) 1m bars drive higher-TF accumulation;
     * in-progress bars update the current live candle for display.
     *
     * @param symbol Symbol (passed through from BarReceiver)
     * @param bar    The new bar (may be in-progress or closed)
     */
    void onNewBar(const QString& symbol, const Bar& bar);

  signals:
    /**
     * @brief Emitted when a higher-TF bar is updated (every 1m bar while period is open).
     * Thread context: Emitted from MainAlgo worker thread.
     * @param tf  The timescale of the updated bar
     * @param bar The current (in-progress) state of the higher-TF bar
     */
    void barUpdated(TimeFrame tf, const Bar& bar);

    /**
     * @brief Emitted when a higher-TF bar's accumulation period closes.
     * Thread context: Emitted from MainAlgo worker thread.
     * @param tf  The timescale of the completed bar
     * @param bar The completed bar (status = Closed)
     */
    void barClosed(TimeFrame tf, const Bar& bar);

  private:
    struct AccumulatingBar
    {
        bool active = false;
        QDateTime openTime;
        double open = 0;
        double high = 0;
        double low = 0;
        double close = 0;
        qint64 volume = 0;

        void reset()
        {
            active = false;
        }

        void start(const Bar& bar)
        {
            active = true;
            openTime = bar.getTimeStamp();
            open = bar.getOpen();
            high = bar.getHigh();
            low = bar.getLow();
            close = bar.getClose();
            volume = bar.getTotalVolume();
        }

        void update(const Bar& bar)
        {
            high = std::max(high, static_cast<double>(bar.getHigh()));
            low = std::min(low, static_cast<double>(bar.getLow()));
            close = bar.getClose();
            volume += bar.getTotalVolume();
        }

        [[nodiscard]] Bar toBar() const
        {
            return Bar(openTime, open, high, low, close, volume);
        }
    };

    /**
     * @brief Returns true when the given 1m bar's close marks the end of a target-TF period.
     */
    [[nodiscard]] bool isBarClosing(TimeFrame tf, const QDateTime& barTs) const;

    // Timescales this aggregator accumulates into (all derived from 1m live stream)
    static const QList<TimeFrame> AGGREGATED_TIMEFRAMES;

    QMap<TimeFrame, AccumulatingBar> m_accum;
};
