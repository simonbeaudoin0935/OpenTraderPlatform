#pragma once

#include <QDateTime>
#include <QString>

#include <optional>

#include "Bar.h"
#include "Trade.h"

/**
 * @brief Rebuilds an approximate Time & Sales tape from live bar-stream updates.
 *
 * TradeStation does not expose a real trade tape. Its 1-minute bar stream, however,
 * pushes the cumulative TotalVolume of the forming bar on every update. The volume
 * delta between two consecutive updates is the number of shares that traded in
 * between, and the bar close is the price of the last of those trades. That pair
 * becomes one synthetic print, classified against the current best bid/offer:
 *
 * - close >= ask  -> buy aggressor  (TradeSide::Bid, Databento convention)
 * - close <= bid  -> sell aggressor (TradeSide::Ask, Databento convention)
 * - otherwise / no BBO -> TradeSide::None
 *
 * Limitations: one print aggregates every trade between two updates, intermediate
 * prices are lost, and the BBO can lag or lead the bar update slightly.
 *
 * The first update only establishes a baseline. Updates for an older minute and
 * updates whose volume does not grow (TradeStation re-sends stale bars around
 * minute rollovers and reconnects) never produce a print.
 *
 * ## Threading
 * Not thread-safe; owned and driven by a single SymbolContext drain.
 */
class BarTapeReconstructor
{
  public:
    struct Bbo
    {
        double m_bid = 0.0;
        double m_ask = 0.0;
    };

    /**
     * @brief Feed one bar-stream update.
     * @param p_symbol      Symbol stamped on the produced print
     * @param p_bar         Latest 1-minute bar update (Open or Closed)
     * @param p_bbo         Best bid/offer at the time the update is processed, if known
     * @param p_receivedAt  Timestamp to stamp on the print (bars carry no trade time)
     * @return The synthetic print, or std::nullopt when nothing traded
     */
    [[nodiscard]] std::optional<Trade>
    onBar(const QString& p_symbol, const Bar& p_bar, const std::optional<Bbo>& p_bbo, const QDateTime& p_receivedAt);

    /** @brief Forget the baseline; the next update will not produce a print. */
    void reset();

    [[nodiscard]] static TradeSide classify(double p_price, const std::optional<Bbo>& p_bbo);

  private:
    QDateTime m_minute;
    quint64 m_volume = 0;
    bool m_hasBaseline = false;
};
