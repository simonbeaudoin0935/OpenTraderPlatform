#pragma once

#include <QBuffer>
#include <QLoggingCategory>
#include <QNetworkReply>

Q_DECLARE_LOGGING_CATEGORY(MockNetworkReplyLog)

/**
 * @class MockNetworkReply
 * @brief Mock implementation of QNetworkReply for replay data injection
 *
 * This class allows ReplayEngine to inject recorded market data into the
 * existing Stream infrastructure without modifying the Stream class.
 *
 * Usage:
 * 1. TSClient creates MockNetworkReply instead of real QNetworkReply in replay mode
 * 2. Stream connects to readyRead() signal as usual
 * 3. ReplayEngine calls injectData() with recorded JSON
 * 4. MockNetworkReply emits readyRead(), Stream calls readAll()
 * 5. Stream processes data normally
 *
 * Threading: Created in TSClient thread, injectData() must be called via
 * QMetaObject::invokeMethod with Qt::QueuedConnection from other threads.
 */
class MockNetworkReply : public QNetworkReply
{
    Q_OBJECT

  public:
    /**
     * @brief Construct a MockNetworkReply
     * @param p_parent Parent object (usually TSClient or Stream)
     */
    explicit MockNetworkReply(QObject* p_parent = nullptr);
    ~MockNetworkReply() override;

    Q_DISABLE_COPY(MockNetworkReply)

    /**
     * @brief Inject data to be read by the Stream
     * @param p_data Raw data (typically JSON with newline)
     *
     * Appends data to internal buffer and emits readyRead().
     * Thread-safe when called via QMetaObject::invokeMethod.
     */
    void injectData(const QByteArray& p_data);

    /**
     * @brief Check if there is data available to read
     */
    [[nodiscard]] qint64 bytesAvailable() const override;

    /**
     * @brief Check if the reply is sequential (always true for streams)
     */
    [[nodiscard]] bool isSequential() const override;

    /**
     * @brief Abort the reply (required by QNetworkReply interface)
     */
    void abort() override;

  protected:
    /**
     * @brief Read data from the internal buffer
     * @param p_data Destination buffer
     * @param p_maxlen Maximum bytes to read
     * @return Number of bytes read, or -1 on error
     */
    qint64 readData(char* p_data, qint64 p_maxlen) override;

    /**
     * @brief Write data (not supported for mock reply)
     */
    qint64 writeData(const char* p_data, qint64 p_len) override;

  private:
    QBuffer m_buffer;
    bool m_aborted = false;
};
