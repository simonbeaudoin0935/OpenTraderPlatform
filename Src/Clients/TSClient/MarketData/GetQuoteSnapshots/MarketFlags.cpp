#include "MarketFlags.h"

namespace
{
    bool parseBool(const QJsonObject& p_json, const char* p_key)
    {
        const QJsonValue value = p_json.value(p_key);
        if (value.isBool())
        {
            return value.toBool();
        }

        if (value.isString())
        {
            const QString text = value.toString().trimmed().toLower();
            return text == "true" || text == "1" || text == "yes";
        }

        return false;
    }
} // namespace

MarketFlags::MarketFlags(const QJsonObject& p_json)
    : m_halted(parseBool(p_json, "IsHalted"))
    , m_delayed(parseBool(p_json, "IsDelayed"))
    , m_hardToBorrow(parseBool(p_json, "IsHardToBorrow"))
    , m_bats(parseBool(p_json, "IsBats"))
{
}

bool MarketFlags::isValid() const
{
    return true;
}

QJsonObject MarketFlags::toJson() const
{
    QJsonObject obj;
    obj["IsHalted"] = m_halted;
    obj["IsDelayed"] = m_delayed;
    obj["IsHardToBorrow"] = m_hardToBorrow;
    obj["IsBats"] = m_bats;
    return obj;
}
