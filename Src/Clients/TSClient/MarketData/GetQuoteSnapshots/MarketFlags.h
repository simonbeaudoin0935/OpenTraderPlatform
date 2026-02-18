#pragma once

#include <QJsonObject>
#include <QMetaType>
#include <QString>

class MarketFlags
{
  public:
    MarketFlags() = default;
    explicit MarketFlags(const QJsonObject& p_json);

    bool isBats() const
    {
        return m_isBats;
    }
    bool isDelayed() const
    {
        return m_isDelayed;
    }
    bool isHalted() const
    {
        return m_isHalted;
    }
    bool isHardToBorrow() const
    {
        return m_isHardToBorrow;
    }

    bool isValid() const;

    QJsonObject toJson() const;
    QString toJsonString() const;
    void fromJson(const QJsonObject& p_json);

  private:
    bool m_isBats = false;
    bool m_isDelayed = false;
    bool m_isHalted = false;
    bool m_isHardToBorrow = false;
};

Q_DECLARE_METATYPE(MarketFlags)
