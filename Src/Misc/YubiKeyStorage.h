#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

class YubiKeyStorage
{
  public:
    enum class Operation
    {
        Read,
        Write,
        Delete
    };
    struct Result
    {
        bool success = false;
        bool found = false;
        QString value;
        QString error;
    };

    static Result
    execute(const QString& p_service, const QString& p_key, const QString& p_value, Operation p_operation);
    static bool unlock(QString& p_error);
    // Terminal for this session: blocks all further credential operations until restart.
    [[nodiscard]] static bool reset(QString& p_error);
    static QString filePath();

    // Shared authenticated file format, also exercised by hardware-independent tests.
    static QByteArray
    encrypt(const QByteArray& p_plaintext, const QByteArray& p_key, const QByteArray& p_challenge, QString& p_error);
    static QByteArray decrypt(const QByteArray& p_file, const QByteArray& p_key, QString& p_error);

  private:
    static bool ensureUnlocked(QString& p_error);
};
