#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

class OrderRoute
{
  public:
    OrderRoute() = default;
    explicit OrderRoute(const QJsonObject& p_jsonObj);

    [[nodiscard]] QString getId() const
    {
        return m_id;
    }

    [[nodiscard]] QString getName() const
    {
        return m_name;
    }

    [[nodiscard]] QStringList getAssetTypes() const
    {
        return m_assetTypes;
    }

    [[nodiscard]] bool supportsAssetType(const QString& p_assetType) const;
    [[nodiscard]] bool isValid() const;

  private:
    QString m_id;
    QString m_name;
    QStringList m_assetTypes;
};

Q_DECLARE_METATYPE(OrderRoute)
Q_DECLARE_METATYPE(QVector<OrderRoute>)
