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
 * Two signal paths feed this aggregator:
 * 1. LiveBarAccumulator::barClosed → onNewBar: drives full accumulation (OHLCV + period-close detection)
 * 2. LiveBarAccumulator::barUpdated → onBarUpdated: real-time price updates (H/L/C only, no volume)
 *
 * This means the live higher-TF candle updates with every trade tick (smooth animation),
 * while volume and period-close logic is correct (based on closed 1m bars only).
 *
 * ## Threading
 * Runs on the MainAlgo worker thread. All signals are emitted on that thread.
 *
 * ## Usage
 * Connect LiveBarAccumulator::barClosed → BarAggregator::onNewBar.
 * Connect LiveBarAccumulator::barUpdated → BarAggregator::onBarUpdated.
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
     * @brief Process a CLOSED 1m bar — drives full OHLCV accumulation and period-close detection.
     * Should be connected to LiveBarAccumulator::barClosed.
     */
    void onNewBar(const QString& symbol, const Bar& bar);

    /**
     * @brief Process an IN-PROGRESS 1m bar — updates the live candle's H/L/C for real-time display.
     * Should be connected to LiveBarAccumulator::barUpdated.
     * Volume is intentionally NOT updated here — only committed when the 1m bar closes via onNewBar.
     */
    void onBarUpdated(const QString& symbol, const Bar& bar);

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

        [[nodiscard]] Bar toBar(Bar::BarStatus status = Bar::BarStatus::Open) const
        {
            return Bar(openTime, open, high, low, close, volume, status);
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
