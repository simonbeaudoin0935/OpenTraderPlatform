#pragma once

#include <QJsonObject>
#include <QString>

#include <expected>

class PlatformControlClient
{
  public:
    struct Options
    {
        QString socketPath;
        int connectTimeoutMs = 5000;
        int responseTimeoutMs = 5000;
    };

    PlatformControlClient();
    explicit PlatformControlClient(Options p_options);

    [[nodiscard]] std::expected<QJsonObject, QString> sendCommand(const QJsonObject& p_request) const;

  private:
    Options m_options;
};
