#include <QJsonDocument>

#include "MarketFlags.h"

MarketFlags::MarketFlags(const QJsonObject& p_json)
{
    fromJson(p_json);
}

bool MarketFlags::isValid() const
{
    // All fields are booleans with defaults; parsing cannot produce invalid states.
    return true;
}

QJsonObject MarketFlags::toJson() const
{
    QJsonObject json;
    json["IsBats"] = m_isBats;
    json["IsDelayed"] = m_isDelayed;
    json["IsHalted"] = m_isHalted;
    json["IsHardToBorrow"] = m_isHardToBorrow;
    return json;
}

QString MarketFlags::toJsonString() const
{
    const QJsonDocument doc(toJson());
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

void MarketFlags::fromJson(const QJsonObject& p_json)
{
    m_isBats = p_json.value("IsBats").toBool(false);
    m_isDelayed = p_json.value("IsDelayed").toBool(false);
    m_isHalted = p_json.value("IsHalted").toBool(false);
    m_isHardToBorrow = p_json.value("IsHardToBorrow").toBool(false);
}
