#include "YubiKeyStorage.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMessageBox>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <memory>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/rand.h>

#include "Assume.h"
#include "CONSTANTS.h"

namespace
{
    using namespace CredentialStorageConstants;
    struct Session
    {
        QByteArray key;
        QByteArray challenge;
        QJsonObject credentials;
        QString error;
        bool attempted = false;
        ~Session()
        {
            OPENSSL_cleanse(key.data(), key.size());
        }
    };
    Session g_session;

    QByteArray runYkman(const QStringList& p_arguments, QString& p_error)
    {
        QProcess process;
        QEventLoop loop;
        QTimer timer;
        timer.setSingleShot(true);
        QObject::connect(&process, &QProcess::finished, &loop, &QEventLoop::quit);
        QObject::connect(&process, &QProcess::errorOccurred, &loop, &QEventLoop::quit);
        QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
        std::unique_ptr<QMessageBox> prompt;
        if (qobject_cast<QApplication*>(QCoreApplication::instance()))
        {
            prompt = std::make_unique<QMessageBox>(
                QMessageBox::Information,
                "Unlock YubiKey credentials",
                "Insert your YubiKey and touch it when it blinks.\n"
                "Slot 2 must already be configured for HMAC challenge-response with touch required.",
                QMessageBox::Cancel,
                QApplication::activeWindow());
            QObject::connect(prompt.get(), &QMessageBox::rejected, &loop, &QEventLoop::quit);
            prompt->setWindowModality(Qt::ApplicationModal);
            prompt->show();
        }
        timer.start(PROCESS_TIMEOUT_MS);
        process.start("ykman", p_arguments);
        loop.exec();
        if (process.state() != QProcess::NotRunning)
        {
            process.kill();
            process.waitForFinished();
            p_error = "YubiKey unlock was cancelled or timed out.";
            return {};
        }
        if (process.error() == QProcess::FailedToStart)
        {
            p_error = "Could not start ykman. Install yubikey-manager to use YubiKey storage.";
            return {};
        }
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        {
            p_error = "YubiKey challenge-response failed. Check the connected key and its touch-enabled Slot 2.";
            return {};
        }
        // ykman reports the device's touch-required keepalive on stderr, not elapsed-time heuristics.
        if (!process.readAllStandardError().contains("Touch your YubiKey"))
        {
            QByteArray discarded = process.readAllStandardOutput();
            OPENSSL_cleanse(discarded.data(), discarded.size());
            p_error = "Slot 2 did not report a required touch. Use a touch-enabled HMAC slot and a supported "
                      "ykman version. The app will not program your key.";
            return {};
        }
        return process.readAllStandardOutput().trimmed();
    }
} // namespace

QString YubiKeyStorage::filePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/" +
           CredentialStorageConstants::VAULT_FILE;
}

QByteArray YubiKeyStorage::encrypt(const QByteArray& p_plaintext,
                                   const QByteArray& p_key,
                                   const QByteArray& p_challenge,
                                   QString& p_error)
{
    using namespace CredentialStorageConstants;
    p_error.clear();
    if (p_key.size() != KEY_SIZE || p_challenge.size() != CHALLENGE_SIZE || p_plaintext.size() > MAX_VAULT_BYTES)
    {
        p_error = "Invalid YubiKey vault encryption input.";
        return {};
    }
    QByteArray header(VAULT_MAGIC);
    header += p_challenge;
    QByteArray iv(IV_SIZE, Qt::Uninitialized);
    if (RAND_bytes(reinterpret_cast<unsigned char*>(iv.data()), IV_SIZE) != 1)
    {
        p_error = "Could not generate a vault nonce.";
        return {};
    }
    header += iv;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    QByteArray cipher(p_plaintext.size() + EVP_MAX_BLOCK_LENGTH, Qt::Uninitialized);
    QByteArray tag(TAG_SIZE, Qt::Uninitialized);
    int length = 0;
    int finalLength = 0;
    int ignored = 0;
    const bool ok =
        ctx &&
        EVP_EncryptInit_ex(ctx,
                           EVP_aes_256_gcm(),
                           nullptr,
                           reinterpret_cast<const unsigned char*>(p_key.constData()),
                           reinterpret_cast<const unsigned char*>(iv.constData())) == 1 &&
        EVP_EncryptUpdate(ctx,
                          nullptr,
                          &ignored,
                          reinterpret_cast<const unsigned char*>(header.constData()),
                          static_cast<int>(header.size())) == 1 &&
        EVP_EncryptUpdate(ctx,
                          reinterpret_cast<unsigned char*>(cipher.data()),
                          &length,
                          reinterpret_cast<const unsigned char*>(p_plaintext.constData()),
                          static_cast<int>(p_plaintext.size())) == 1 &&
        EVP_EncryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(cipher.data()) + length, &finalLength) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, TAG_SIZE, tag.data()) == 1;
    EVP_CIPHER_CTX_free(ctx);
    if (!ok)
    {
        p_error = "Could not encrypt the YubiKey vault.";
        return {};
    }
    cipher.resize(length + finalLength);
    return header + tag + cipher;
}

QByteArray YubiKeyStorage::decrypt(const QByteArray& p_file, const QByteArray& p_key, QString& p_error)
{
    using namespace CredentialStorageConstants;
    p_error.clear();
    const int headerSize = static_cast<int>(QByteArray(VAULT_MAGIC).size()) + CHALLENGE_SIZE + IV_SIZE;
    const int prefixSize = headerSize + TAG_SIZE;
    if (!p_file.startsWith(VAULT_MAGIC) || p_file.size() < prefixSize || p_file.size() > MAX_VAULT_BYTES + prefixSize ||
        p_key.size() != KEY_SIZE)
    {
        p_error = "Invalid or unsupported YubiKey vault file.";
        return {};
    }
    const QByteArray iv = p_file.mid(headerSize - IV_SIZE, IV_SIZE);
    QByteArray tag = p_file.mid(headerSize, TAG_SIZE);
    const QByteArray cipher = p_file.mid(prefixSize);
    QByteArray plain(cipher.size() + EVP_MAX_BLOCK_LENGTH, Qt::Uninitialized);
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    int length = 0;
    int finalLength = 0;
    int ignored = 0;
    const bool ok =
        ctx &&
        EVP_DecryptInit_ex(ctx,
                           EVP_aes_256_gcm(),
                           nullptr,
                           reinterpret_cast<const unsigned char*>(p_key.constData()),
                           reinterpret_cast<const unsigned char*>(iv.constData())) == 1 &&
        EVP_DecryptUpdate(ctx,
                          nullptr,
                          &ignored,
                          reinterpret_cast<const unsigned char*>(p_file.constData()),
                          headerSize) == 1 &&
        EVP_DecryptUpdate(ctx,
                          reinterpret_cast<unsigned char*>(plain.data()),
                          &length,
                          reinterpret_cast<const unsigned char*>(cipher.constData()),
                          static_cast<int>(cipher.size())) == 1 &&
        EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, TAG_SIZE, tag.data()) == 1 &&
        EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(plain.data()) + length, &finalLength) == 1;
    EVP_CIPHER_CTX_free(ctx);
    if (!ok)
    {
        OPENSSL_cleanse(plain.data(), plain.size());
        p_error = "Could not authenticate the vault: wrong YubiKey or damaged file.";
        return {};
    }
    plain.resize(length + finalLength);
    return plain;
}

bool YubiKeyStorage::unlock(QString& p_error)
{
    p_error.clear();
    if (!g_session.key.isEmpty())
    {
        return true;
    }
    g_session.attempted = false;
    return ensureUnlocked(p_error);
}

bool YubiKeyStorage::ensureUnlocked(QString& p_error)
{
    using namespace CredentialStorageConstants;
    ASSUME_EQUAL(QThread::currentThread(), QCoreApplication::instance()->thread());
    p_error.clear();
    if (!g_session.key.isEmpty())
    {
        return true;
    }
    if (g_session.attempted)
    {
        p_error = g_session.error;
        return false;
    }
    g_session.attempted = true;
    g_session.error = "YubiKey vault is locked. Use Unlock / retry YubiKey in Credentials.";
    QByteArray fileData;
    QFile file(filePath());
    if (file.exists())
    {
        if (!file.open(QIODevice::ReadOnly) || file.size() > MAX_VAULT_BYTES + VAULT_PREFIX_SIZE)
        {
            p_error = g_session.error = "Could not read the YubiKey vault.";
            return false;
        }
        fileData = file.readAll();
        const int magicSize = static_cast<int>(QByteArray(VAULT_MAGIC).size());
        if (!fileData.startsWith(VAULT_MAGIC) || fileData.size() < magicSize + CHALLENGE_SIZE + IV_SIZE + TAG_SIZE)
        {
            p_error = g_session.error = "Invalid or unsupported YubiKey vault file.";
            return false;
        }
        g_session.challenge = fileData.mid(magicSize, CHALLENGE_SIZE);
    }
    else
    {
        g_session.challenge.resize(CHALLENGE_SIZE);
        if (RAND_bytes(reinterpret_cast<unsigned char*>(g_session.challenge.data()), CHALLENGE_SIZE) != 1)
        {
            p_error = g_session.error = "Could not generate a YubiKey challenge.";
            return false;
        }
    }
    QByteArray response =
        runYkman({"otp", "calculate", "2", QString::fromLatin1(g_session.challenge.toHex())}, p_error);
    if (response.size() != RESPONSE_SIZE * 2 ||
        QRegularExpression("[^0-9a-fA-F]").match(QString::fromLatin1(response)).hasMatch())
    {
        if (p_error.isEmpty())
        {
            p_error = "Unexpected YubiKey challenge-response output.";
        }
        g_session.error = p_error;
        OPENSSL_cleanse(response.data(), response.size());
        return false;
    }
    QByteArray hmac = QByteArray::fromHex(response);
    OPENSSL_cleanse(response.data(), response.size());
    QByteArray key(KEY_SIZE, Qt::Uninitialized);
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    size_t keyLength = KEY_SIZE;
    const QByteArray info(HKDF_INFO);
    const bool derived =
        ctx && EVP_PKEY_derive_init(ctx) == 1 && EVP_PKEY_CTX_set_hkdf_md(ctx, EVP_sha256()) == 1 &&
        EVP_PKEY_CTX_set1_hkdf_salt(ctx,
                                    reinterpret_cast<const unsigned char*>(g_session.challenge.constData()),
                                    CHALLENGE_SIZE) == 1 &&
        EVP_PKEY_CTX_set1_hkdf_key(ctx, reinterpret_cast<const unsigned char*>(hmac.constData()), RESPONSE_SIZE) == 1 &&
        EVP_PKEY_CTX_add1_hkdf_info(ctx,
                                    reinterpret_cast<const unsigned char*>(info.constData()),
                                    static_cast<int>(info.size())) == 1 &&
        EVP_PKEY_derive(ctx, reinterpret_cast<unsigned char*>(key.data()), &keyLength) == 1 && keyLength == KEY_SIZE;
    EVP_PKEY_CTX_free(ctx);
    OPENSSL_cleanse(hmac.data(), hmac.size());
    if (!derived)
    {
        OPENSSL_cleanse(key.data(), key.size());
        p_error = g_session.error = "Could not derive the vault encryption key.";
        return false;
    }
    QJsonObject credentials;
    if (!fileData.isEmpty())
    {
        QByteArray plain = decrypt(fileData, key, p_error);
        QJsonParseError parseError;
        QJsonDocument doc;
        if (p_error.isEmpty())
        {
            doc = QJsonDocument::fromJson(plain, &parseError);
        }
        OPENSSL_cleanse(plain.data(), plain.size());
        if (!p_error.isEmpty() || parseError.error != QJsonParseError::NoError || !doc.isObject())
        {
            OPENSSL_cleanse(key.data(), key.size());
            if (p_error.isEmpty())
            {
                p_error = "Invalid credential data in YubiKey vault.";
            }
            g_session.error = p_error;
            return false;
        }
        credentials = doc.object();
        for (auto section = credentials.constBegin(); section != credentials.constEnd(); ++section)
        {
            if (!section.value().isObject())
            {
                OPENSSL_cleanse(key.data(), key.size());
                p_error = g_session.error = "Invalid credential section in YubiKey vault.";
                return false;
            }
            const QJsonObject values = section.value().toObject();
            for (auto value = values.constBegin(); value != values.constEnd(); ++value)
            {
                if (!value.value().isString())
                {
                    OPENSSL_cleanse(key.data(), key.size());
                    p_error = g_session.error = "Invalid credential value in YubiKey vault.";
                    return false;
                }
            }
        }
    }
    g_session.key = key;
    g_session.credentials = credentials;
    g_session.error.clear();
    return true;
}

YubiKeyStorage::Result
YubiKeyStorage::execute(const QString& p_service, const QString& p_key, const QString& p_value, Operation p_operation)
{
    Result result;
    if (!ensureUnlocked(result.error))
    {
        return result;
    }
    QJsonObject next = g_session.credentials;
    QJsonObject section = next.value(p_service).toObject();
    if (p_operation == Operation::Read)
    {
        result.success = true;
        result.found = section.contains(p_key);
        result.value = section.value(p_key).toString();
        return result;
    }
    if (p_operation == Operation::Write)
    {
        section.insert(p_key, p_value);
    }
    else
    {
        section.remove(p_key);
    }
    next.insert(p_service, section);
    QByteArray plain = QJsonDocument(next).toJson(QJsonDocument::Compact);
    const QByteArray encrypted = encrypt(plain, g_session.key, g_session.challenge, result.error);
    OPENSSL_cleanse(plain.data(), plain.size());
    if (!result.error.isEmpty())
    {
        return result;
    }
    if (!QDir().mkpath(QFileInfo(filePath()).absolutePath()))
    {
        result.error = "Could not create the YubiKey vault directory.";
        return result;
    }
    QSaveFile file(filePath());
    if (!file.open(QIODevice::WriteOnly) || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) ||
        file.write(encrypted) != encrypted.size() || !file.commit())
    {
        result.error = "Could not save the encrypted YubiKey vault.";
        return result;
    }
    g_session.credentials = next;
    result.success = true;
    return result;
}
