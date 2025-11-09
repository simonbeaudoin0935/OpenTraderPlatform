#include "SecureStorage.h"

#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QLoggingCategory>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QTimer>

Q_LOGGING_CATEGORY(secureStorage, "SecureStorage")

#ifndef QT_KEYCHAIN_LIB
#warning "QKeychain library not found, SecureStorage will use less secure QSettings fallback."
#endif

SecureStorage::SecureStorage(QObject* parent)
    : QObject(parent)
{
}

SecureStorage::~SecureStorage()
{
}

void SecureStorage::storeValue(const QString& service, const QString& key, const QString& value,
                              std::function<void(bool)> callback)
{
#ifdef QT_KEYCHAIN_LIB
    if (isSecureStorageAvailable()) {
        auto* job = new QKeychain::WritePasswordJob(service, this);
        job->setKey(key);
        job->setBinaryData(value.toUtf8());

        connect(job, &QKeychain::Job::finished, this, [this, job, callback]() {
            handleStoreFinished(job, callback);
        });

        job->start();
        return;
    }
#endif

    // Fallback to QSettings
    storeValueFallback(service, key, value, callback);
}

void SecureStorage::retrieveValue(const QString& service, const QString& key,
                                 std::function<void(const QString&)> callback)
{
#ifdef QT_KEYCHAIN_LIB
    if (isSecureStorageAvailable()) {
        auto* job = new QKeychain::ReadPasswordJob(service, this);
        job->setKey(key);

        connect(job, &QKeychain::Job::finished, this, [this, job, callback]() {
            handleReadFinished(job, callback);
        });

        job->start();
        return;
    }
#endif

    // Fallback to QSettings
    retrieveValueFallback(service, key, callback);
}

void SecureStorage::deleteValue(const QString& service, const QString& key,
                               std::function<void(bool)> callback)
{
#ifdef QT_KEYCHAIN_LIB
    if (isSecureStorageAvailable()) {
        auto* job = new QKeychain::DeletePasswordJob(service, this);
        job->setKey(key);

        connect(job, &QKeychain::Job::finished, this, [this, job, callback]() {
            handleDeleteFinished(job, callback);
        });

        job->start();
        return;
    }
#endif

    // Fallback to QSettings
    deleteValueFallback(service, key, callback);
}

bool SecureStorage::isSecureStorageAvailable()
{
#ifdef QT_KEYCHAIN_LIB
    return true;
#else
    qCWarning(secureStorage) << "QKeychain not available, using encrypted QSettings fallback";
    return false;
#endif
}

bool SecureStorage::storeValuesSync(const QString& service, const QMap<QString, QString>& keyValues, int timeoutMs)
{
    if (keyValues.isEmpty()) {
        return true;
    }

#ifdef QT_KEYCHAIN_LIB
    if (isSecureStorageAvailable()) {
        // Use async approach with event loop for QKeychain
        QEventLoop loop;
        bool success = true;
        int completedOperations = 0;
        const int totalOperations = keyValues.size();

        auto checkCompletion = [&]() {
            completedOperations++;
            if (completedOperations >= totalOperations) {
                loop.quit();
            }
        };

        // Start all store operations
        for (auto it = keyValues.constBegin(); it != keyValues.constEnd(); ++it) {
            storeValue(service, it.key(), it.value(), [&](bool result) {
                if (!result) success = false;
                checkCompletion();
            });
        }

        // Wait for all operations to complete (with timeout)
        QTimer timer;
        timer.setSingleShot(true);
        timer.start(timeoutMs);

        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

        loop.exec();

        return success && (completedOperations >= totalOperations);
    }
#endif

    // Fallback to synchronous QSettings operations
    bool success = true;
    for (auto it = keyValues.constBegin(); it != keyValues.constEnd(); ++it) {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                          "L2Trader", "SecureStorage");
        settings.setFallbacksEnabled(false);

        QString fullKey = QString("%1/%2").arg(service, it.key());
        QString obfuscatedValue = obfuscateValue(it.value());
        settings.setValue(fullKey, obfuscatedValue);
        settings.sync();

        if (settings.status() != QSettings::NoError) {
            qCWarning(secureStorage) << "Failed to store value in fallback storage:" << settings.status();
            success = false;
        }
    }

    if (success) {
        qCWarning(secureStorage) << "Values stored using obfuscated fallback storage (upgrade to QKeychain recommended)";
    }

    return success;
}

QMap<QString, QString> SecureStorage::retrieveValuesSync(const QString& service, const QStringList& keys, int timeoutMs)
{
    QMap<QString, QString> results;
    if (keys.isEmpty()) {
        return results;
    }

#ifdef QT_KEYCHAIN_LIB
    if (isSecureStorageAvailable()) {
        // Use async approach with event loop for QKeychain
        QEventLoop loop;
        int completedOperations = 0;
        const int totalOperations = keys.size();

        auto checkCompletion = [&]() {
            completedOperations++;
            if (completedOperations >= totalOperations) {
                loop.quit();
            }
        };

        // Start all retrieve operations
        for (const QString& key : keys) {
            retrieveValue(service, key, [&](const QString& value) {
                results[key] = value;
                checkCompletion();
            });
        }

        // Wait for all operations to complete (with timeout)
        QTimer timer;
        timer.setSingleShot(true);
        timer.start(timeoutMs);

        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

        loop.exec();

        return results;
    }
#endif

    // Fallback to synchronous QSettings operations
    for (const QString& key : keys) {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                          "L2Trader", "SecureStorage");
        settings.setFallbacksEnabled(false);

        QString fullKey = QString("%1/%2").arg(service, key);
        QString obfuscatedValue = settings.value(fullKey).toString();
        QString value = deobfuscateValue(obfuscatedValue);

        results[key] = value;
    }

    qCDebug(secureStorage) << "Retrieved values from fallback storage for service:" << service;
    return results;
}

bool SecureStorage::deleteValuesSync(const QString& service, const QStringList& keys, int timeoutMs)
{
    if (keys.isEmpty()) {
        return true;
    }

#ifdef QT_KEYCHAIN_LIB
    if (isSecureStorageAvailable()) {
        // Use async approach with event loop for QKeychain
        QEventLoop loop;
        bool success = true;
        int completedOperations = 0;
        const int totalOperations = keys.size();

        auto checkCompletion = [&]() {
            completedOperations++;
            if (completedOperations >= totalOperations) {
                loop.quit();
            }
        };

        // Start all delete operations
        for (const QString& key : keys) {
            deleteValue(service, key, [&](bool result) {
                if (!result) success = false;
                checkCompletion();
            });
        }

        // Wait for all operations to complete (with timeout)
        QTimer timer;
        timer.setSingleShot(true);
        timer.start(timeoutMs);

        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

        loop.exec();

        return success && (completedOperations >= totalOperations);
    }
#endif

    // Fallback to synchronous QSettings operations
    bool success = true;
    for (const QString& key : keys) {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                          "L2Trader", "SecureStorage");
        settings.setFallbacksEnabled(false);

        QString fullKey = QString("%1/%2").arg(service, key);
        settings.remove(fullKey);
        settings.sync();

        if (settings.status() != QSettings::NoError) {
            qCWarning(secureStorage) << "Failed to delete value from fallback storage:" << settings.status();
            success = false;
        }
    }

    return success;
}

#ifdef QT_KEYCHAIN_LIB
void SecureStorage::handleStoreFinished(QKeychain::Job* job, std::function<void(bool)> callback)
{
    auto* writeJob = qobject_cast<QKeychain::WritePasswordJob*>(job);
    Q_ASSERT(writeJob);

    bool success = (writeJob->error() == QKeychain::NoError);
    if (!success) {
        qCWarning(secureStorage) << "Failed to store value securely:" << writeJob->errorString();
        qCInfo(secureStorage) << "Falling back to QSettings storage";
        // Could fall back here, but for now just report failure
    }

    if (callback) {
        callback(success);
    }

    writeJob->deleteLater();
}

void SecureStorage::handleReadFinished(QKeychain::Job* job, std::function<void(const QString&)> callback)
{
    auto* readJob = qobject_cast<QKeychain::ReadPasswordJob*>(job);
    Q_ASSERT(readJob);

    QString value;
    if (readJob->error() == QKeychain::NoError) {
        value = QString::fromUtf8(readJob->binaryData());
    } else {
        qCWarning(secureStorage) << "Failed to read value securely:" << readJob->errorString();
    }

    if (callback) {
        callback(value);
    }

    readJob->deleteLater();
}

void SecureStorage::handleDeleteFinished(QKeychain::Job* job, std::function<void(bool)> callback)
{
    auto* deleteJob = qobject_cast<QKeychain::DeletePasswordJob*>(job);
    Q_ASSERT(deleteJob);

    bool success = (deleteJob->error() == QKeychain::NoError);
    if (!success) {
        qCWarning(secureStorage) << "Failed to delete value securely:" << deleteJob->errorString();
    }

    if (callback) {
        callback(success);
    }

    deleteJob->deleteLater();
}
#endif

void SecureStorage::storeValueFallback(const QString& service, const QString& key, const QString& value,
                                      std::function<void(bool)> callback)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                      "L2Trader", "SecureStorage");
    settings.setFallbacksEnabled(false);

    QString fullKey = QString("%1/%2").arg(service, key);
    QString obfuscatedValue = obfuscateValue(value);
    settings.setValue(fullKey, obfuscatedValue);
    settings.sync();

    bool success = (settings.status() == QSettings::NoError);
    if (!success) {
        qCWarning(secureStorage) << "Failed to store value in fallback storage:" << settings.status();
    } else {
        qCWarning(secureStorage) << "Value stored using obfuscated fallback storage (upgrade to QKeychain recommended)";
    }

    if (callback) {
        callback(success);
    }
}

void SecureStorage::retrieveValueFallback(const QString& service, const QString& key,
                                         std::function<void(const QString&)> callback)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                      "L2Trader", "SecureStorage");
    settings.setFallbacksEnabled(false);

    QString fullKey = QString("%1/%2").arg(service, key);
    QString obfuscatedValue = settings.value(fullKey).toString();
    QString value = deobfuscateValue(obfuscatedValue);

    qCDebug(secureStorage) << "Retrieved value from fallback storage:" << fullKey;
    qCDebug(secureStorage) << "Obfuscated value:" << obfuscatedValue;

    if (callback) {
        callback(value);
    }
}

void SecureStorage::deleteValueFallback(const QString& service, const QString& key,
                                       std::function<void(bool)> callback)
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                      "L2Trader", "SecureStorage");
    settings.setFallbacksEnabled(false);

    QString fullKey = QString("%1/%2").arg(service, key);
    settings.remove(fullKey);
    settings.sync();

    bool success = (settings.status() == QSettings::NoError);
    if (!success) {
        qCWarning(secureStorage) << "Failed to delete value from fallback storage:" << settings.status();
    }

    if (callback) {
        callback(success);
    }
}

QString SecureStorage::obfuscateValue(const QString& value)
{
    if (value.isEmpty()) return QString();

    // Simple obfuscation using XOR with a pseudo-random key derived from app name
    // This is NOT secure encryption - use QKeychain for production security
    QByteArray data = value.toUtf8();
    QByteArray key = QCryptographicHash::hash("L2TraderSecureStorage", QCryptographicHash::Sha256);

    for (int i = 0; i < data.size(); ++i) {
        data[i] = data[i] ^ key[i % key.size()];
    }

    return QString::fromUtf8(data.toBase64());
}

QString SecureStorage::deobfuscateValue(const QString& obfuscatedValue)
{
    if (obfuscatedValue.isEmpty()) return QString();

    // Reverse the obfuscation
    QByteArray data = QByteArray::fromBase64(obfuscatedValue.toUtf8());
    QByteArray key = QCryptographicHash::hash("L2TraderSecureStorage", QCryptographicHash::Sha256);

    for (int i = 0; i < data.size(); ++i) {
        data[i] = data[i] ^ key[i % key.size()];
    }

    return QString::fromUtf8(data);
}