#include "SecureStorage.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QLoggingCategory>
#include <QPointer>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>

#include "Assume.h"
#include "CONSTANTS.h"
#include "YubiKeyStorage.h"

Q_LOGGING_CATEGORY(secureStorage, "SecureStorage")

SecureStorage::SecureStorage(QObject* parent) : QObject(parent)
{
    // Freeze the running session's backend before the GUI can change the next-launch preference.
    static_cast<void>(activeBackend());
    QSettings legacy(QSettings::IniFormat, QSettings::UserScope, "OpenTraderPlatform", "SecureStorage");
    legacy.setFallbacksEnabled(false);
    if (!legacy.allKeys().isEmpty())
    {
        legacy.clear();
        legacy.sync();
        if (legacy.status() != QSettings::NoError)
        {
            qCCritical(secureStorage) << "Could not remove obsolete insecure credential entries";
        }
        else
        {
            qCWarning(secureStorage) << "Removed obsolete insecure credential entries; re-enter credentials";
        }
    }
}

SecureStorage::~SecureStorage() = default;

SecureStorage::Backend SecureStorage::configuredBackend()
{
    QSettings settings;
    return settings.value(CredentialStorageConstants::SETTINGS_KEY, "OSKeyring").toString() == "YubiKey"
               ? Backend::YubiKey
               : Backend::OSKeyring;
}

SecureStorage::Backend SecureStorage::activeBackend()
{
    static const Backend backend = configuredBackend();
    return backend;
}

bool SecureStorage::configureBackend(Backend p_backend)
{
    QSettings settings;
    settings.setValue(CredentialStorageConstants::SETTINGS_KEY,
                      p_backend == Backend::YubiKey ? "YubiKey" : "OSKeyring");
    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        qCWarning(secureStorage) << "Could not save the credential storage preference";
        return false;
    }
    return true;
}

QString SecureStorage::backendName()
{
    return activeBackend() == Backend::YubiKey ? "YubiKey vault" : "OS keyring";
}

bool SecureStorage::unlockYubiKey(QString& p_error)
{
    ASSUME_EQUAL(QThread::currentThread(), QCoreApplication::instance()->thread());
    return YubiKeyStorage::unlock(p_error);
}

bool SecureStorage::resetYubiKey(QString& p_error)
{
    ASSUME_EQUAL(QThread::currentThread(), QCoreApplication::instance()->thread());
    const bool success = YubiKeyStorage::reset(p_error);
    if (!success)
    {
        qCWarning(secureStorage) << "YubiKey vault reset failed:" << p_error;
    }
    else
    {
        qCInfo(secureStorage) << "YubiKey vault reset; credential storage disabled until restart";
    }
    return success;
}

SecureStorage::Result SecureStorage::runYubiKeyJob(const QString& p_service,
                                                   const QString& p_key,
                                                   const QString& p_value,
                                                   Operation p_operation)
{
    YubiKeyStorage::Operation operation = YubiKeyStorage::Operation::Read;
    switch (p_operation)
    {
    case Operation::Read:
        break;
    case Operation::Write:
        operation = YubiKeyStorage::Operation::Write;
        break;
    case Operation::Delete:
        operation = YubiKeyStorage::Operation::Delete;
        break;
    }
    const auto vaultResult = YubiKeyStorage::execute(p_service, p_key, p_value, operation);
    Result result;
    result.finished = true;
    result.value = vaultResult.value;
    result.error =
        vaultResult.success
            ? (p_operation == Operation::Read && !vaultResult.found ? QKeychain::EntryNotFound : QKeychain::NoError)
            : QKeychain::OtherError;
    if (!vaultResult.success)
    {
        qCWarning(secureStorage) << vaultResult.error;
    }
    return result;
}

QKeychain::Job*
SecureStorage::createJob(const QString& p_service, const QString& p_key, const QString& p_value, Operation p_operation)
{
    auto* app = QCoreApplication::instance();
    ASSUME_DIFF(app, nullptr);
    ASSUME_EQUAL(QThread::currentThread(), app->thread());
    QKeychain::Job* job = nullptr;
    switch (p_operation)
    {
    case Operation::Read:
        job = new QKeychain::ReadPasswordJob(p_service, app);
        break;
    case Operation::Write:
    {
        auto* writeJob = new QKeychain::WritePasswordJob(p_service, app);
        Q_CHECK_PTR(writeJob);
        writeJob->setBinaryData(p_value.toUtf8());
        job = writeJob;
        break;
    }
    case Operation::Delete:
        job = new QKeychain::DeletePasswordJob(p_service, app);
        break;
    }
    Q_CHECK_PTR(job);
    job->setInsecureFallback(false);
    job->setKey(p_key);
    return job;
}

SecureStorage::Result SecureStorage::readResult(QKeychain::Job* p_job, Operation p_operation)
{
    Result result;
    result.finished = true;
    result.error = p_job->error();
    if (result.error != QKeychain::NoError && result.error != QKeychain::EntryNotFound)
    {
        qCWarning(secureStorage) << "Native keyring operation failed:" << p_job->errorString();
    }
    if (p_operation == Operation::Read && result.error == QKeychain::NoError)
    {
        auto* readJob = qobject_cast<QKeychain::ReadPasswordJob*>(p_job);
        ASSUME_DIFF(readJob, nullptr);
        result.value = QString::fromUtf8(readJob->binaryData());
    }
    return result;
}

SecureStorage::Result SecureStorage::runJob(const QString& p_service,
                                            const QString& p_key,
                                            const QString& p_value,
                                            Operation p_operation,
                                            int p_timeoutMs)
{
    ASSUME_GT(p_timeoutMs, 0);
    auto* app = QCoreApplication::instance();
    ASSUME_DIFF(app, nullptr);
    Result result;
    const auto execute = [&]()
    {
        if (activeBackend() == Backend::YubiKey)
        {
            result = runYubiKeyJob(p_service, p_key, p_value, p_operation);
            return;
        }
        // QtKeychain 0.14 has a process-wide executor; all jobs must share its GUI-thread affinity.
        auto* job = createJob(p_service, p_key, p_value, p_operation);
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(job,
                         &QKeychain::Job::finished,
                         &loop,
                         [&](QKeychain::Job* p_finishedJob)
                         {
                             result = readResult(p_finishedJob, p_operation);
                             loop.quit();
                         });
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        timer.start(p_timeoutMs);
        job->start();
        loop.exec();
        if (!result.finished)
        {
            // The loop's destruction disconnects its callback. The job stays alive until completion.
            qCWarning(secureStorage) << "Native keyring operation timed out";
        }
    };
    if (QThread::currentThread() == app->thread())
    {
        execute();
    }
    else
    {
        const bool invoked = QMetaObject::invokeMethod(app, execute, Qt::BlockingQueuedConnection);
        ASSUME_TRUE(invoked);
    }
    return result;
}

void SecureStorage::startJob(const QString& p_service,
                             const QString& p_key,
                             const QString& p_value,
                             Operation p_operation,
                             std::function<void(Result)> p_callback)
{
    auto* app = QCoreApplication::instance();
    OBJ_ASSUME_DIFF(app, nullptr);
    const QPointer<SecureStorage> receiver(this);
    const bool invoked = QMetaObject::invokeMethod(
        app,
        [receiver, p_service, p_key, p_value, p_operation, p_callback]()
        {
            if (!receiver)
            {
                return;
            }
            if (activeBackend() == Backend::YubiKey)
            {
                const Result result = runYubiKeyJob(p_service, p_key, p_value, p_operation);
                if (receiver && p_callback)
                {
                    QMetaObject::invokeMethod(
                        receiver,
                        [p_callback, result]() { p_callback(result); },
                        Qt::QueuedConnection);
                }
                return;
            }
            auto* job = createJob(p_service, p_key, p_value, p_operation);
            QObject::connect(job,
                             &QKeychain::Job::finished,
                             job,
                             [receiver, p_operation, p_callback](QKeychain::Job* p_finishedJob)
                             {
                                 const Result result = readResult(p_finishedJob, p_operation);
                                 if (receiver && p_callback)
                                 {
                                     QMetaObject::invokeMethod(
                                         receiver,
                                         [p_callback, result]() { p_callback(result); },
                                         Qt::QueuedConnection);
                                 }
                             });
            job->start();
        },
        Qt::QueuedConnection);
    OBJ_ASSUME_TRUE(invoked);
}

void SecureStorage::storeValue(const QString& service,
                               const QString& key,
                               const QString& value,
                               std::function<void(bool)> callback)
{
    startJob(service,
             key,
             value,
             Operation::Write,
             [callback](Result p_result)
             {
                 if (callback)
                 {
                     callback(p_result.error == QKeychain::NoError);
                 }
             });
}

void SecureStorage::retrieveValue(const QString& service,
                                  const QString& key,
                                  std::function<void(const QString&)> callback)
{
    OBJ_ASSUME_TRUE(callback);
    startJob(service, key, {}, Operation::Read, [callback](Result p_result) { callback(p_result.value); });
}

void SecureStorage::deleteValue(const QString& service, const QString& key, std::function<void(bool)> callback)
{
    startJob(service,
             key,
             {},
             Operation::Delete,
             [callback](Result p_result)
             {
                 if (callback)
                 {
                     callback(p_result.error == QKeychain::NoError || p_result.error == QKeychain::EntryNotFound);
                 }
             });
}

bool SecureStorage::isSecureStorageAvailable()
{
    return activeBackend() == Backend::YubiKey ? !QStandardPaths::findExecutable("ykman").isEmpty()
                                               : QKeychain::isAvailable();
}

bool SecureStorage::storeValuesSync(const QString& service, const QMap<QString, QString>& keyValues, int timeoutMs)
{
    OBJ_ASSUME_FALSE(service.isEmpty());
    OBJ_ASSUME_FALSE(keyValues.isEmpty());
    for (auto it = keyValues.cbegin(); it != keyValues.cend(); ++it)
    {
        OBJ_ASSUME_FALSE(it.key().isEmpty());
        OBJ_ASSUME_FALSE(it.value().isEmpty());
        const Result result = runJob(service, it.key(), it.value(), Operation::Write, timeoutMs);
        if (!result.finished || result.error != QKeychain::NoError)
        {
            return false;
        }
    }
    return true;
}

QMap<QString, QString> SecureStorage::retrieveValuesSync(const QString& service, const QStringList& keys, int timeoutMs)
{
    OBJ_ASSUME_FALSE(service.isEmpty());
    OBJ_ASSUME_FALSE(keys.isEmpty());
    QMap<QString, QString> results;
    for (const QString& key: keys)
    {
        OBJ_ASSUME_FALSE(key.isEmpty());
        const Result result = runJob(service, key, {}, Operation::Read, timeoutMs);
        if (!result.finished || (result.error != QKeychain::NoError && result.error != QKeychain::EntryNotFound))
        {
            return {};
        }
        if (result.error == QKeychain::NoError)
        {
            results.insert(key, result.value);
        }
    }
    return results;
}

bool SecureStorage::deleteValuesSync(const QString& service, const QStringList& keys, int timeoutMs)
{
    OBJ_ASSUME_FALSE(service.isEmpty());
    OBJ_ASSUME_FALSE(keys.isEmpty());
    bool success = true;
    for (const QString& key: keys)
    {
        OBJ_ASSUME_FALSE(key.isEmpty());
        const Result result = runJob(service, key, {}, Operation::Delete, timeoutMs);
        if (!result.finished || (result.error != QKeychain::NoError && result.error != QKeychain::EntryNotFound))
        {
            success = false;
        }
    }
    return success;
}
