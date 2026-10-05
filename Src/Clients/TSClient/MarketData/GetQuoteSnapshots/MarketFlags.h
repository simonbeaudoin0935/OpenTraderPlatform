#pragma once

#include <QJsonObject>

/**
 * @brief Market flags carried by TradeStation Quote payloads.
 */
class MarketFlags
{
  public:
    MarketFlags() = default;
    explicit MarketFlags(const QJsonObject& p_json);

    [[nodiscard]] bool isValid() const;

    [[nodiscard]] bool isHalted() const
    {
        return m_halted;
    }
    [[nodiscard]] bool isDelayed() const
    {
        return m_delayed;
    }
    [[nodiscard]] bool isHardToBorrow() const
    {
        return m_hardToBorrow;
    }
    [[nodiscard]] bool isBats() const
    {
        return m_bats;
    }

    [[nodiscard]] QJsonObject toJson() const;

  private:
    bool m_halted = false;
    bool m_delayed = false;
    bool m_hardToBorrow = false;
    bool m_bats = false;
};
