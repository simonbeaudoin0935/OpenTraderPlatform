#include "OrderRoute.h"

#include <QJsonArray>

OrderRoute::OrderRoute(const QJsonObject& p_jsonObj)
{
    m_id = p_jsonObj.value("Id").toString().trimmed();
    m_name = p_jsonObj.value("Name").toString().trimmed();

    const QJsonArray assetTypesArray = p_jsonObj.value("AssetTypes").toArray();
    for (const QJsonValue& value: assetTypesArray)
    {
        const QString assetType = value.toString().trimmed();
        if (!assetType.isEmpty())
        {
            m_assetTypes.push_back(assetType);
        }
    }
}

bool OrderRoute::supportsAssetType(const QString& p_assetType) const
{
    for (const QString& assetType: m_assetTypes)
    {
        if (assetType.compare(p_assetType, Qt::CaseInsensitive) == 0)
        {
            return true;
        }
    }
    return false;
}

bool OrderRoute::isValid() const
{
    return !m_id.isEmpty();
}
