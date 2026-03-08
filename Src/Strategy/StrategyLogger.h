#pragma once

#include <QString>
#include <QDateTime>
#include <QVector>
#include <QMutex>
#include <QFile>
#include <QTextStream>
#include <memory>

/// @brief Log message with timestamp and level
struct StrategyLogMessage
{
    QDateTime timestamp;
    QtMsgType level; // Debug, Warning, Critical, etc.
    QString message;
};

/// @brief Per-strategy logging system
/// Opens a log file immediately on construction and appends each message as it
/// arrives (same as the platform AppLogs).  Messages are also kept in a
/// circular in-memory buffer for live display in the UI.
class StrategyLogger
{
  public:
    /// @brief Create logger for a strategy and open its log file immediately.
    /// @param p_strategyName Name of strategy (used for log file naming)
    explicit StrategyLogger(const QString& p_strategyName);
    ~StrategyLogger() = default;

    /// @brief Log a message — written to file and in-memory buffer immediately.
    void log(QtMsgType p_level, const QString& p_message);

    /// @brief Get all logged messages (thread-safe copy)
    [[nodiscard]] QVector<StrategyLogMessage> getMessages() const;

    /// @brief Get messages since a specific timestamp
    [[nodiscard]] QVector<StrategyLogMessage> getMessagesSince(const QDateTime& p_since) const;

    /// @brief Get most recent N messages
    [[nodiscard]] QVector<StrategyLogMessage> getRecentMessages(int p_count) const;

    /// @brief Clear the in-memory buffer (does not truncate the log file).
    void clear();

    /// @brief Returns the path of the log file opened at construction.
    [[nodiscard]] QString saveToFile() const;

    /// @brief Get the log file path for this strategy
    [[nodiscard]] QString getLogFilePath() const;

    /// @brief Get strategy name
    [[nodiscard]] QString getStrategyName() const
    {
        return m_strategyName;
    }

    /// @brief Get message count
    [[nodiscard]] int messageCount() const;

  private:
    QString m_strategyName;
    QString m_filePath;
    std::unique_ptr<QFile> m_file;
    std::unique_ptr<QTextStream> m_stream;
    QVector<StrategyLogMessage> m_messages;    ///< Circular buffer for live UI display
    mutable QMutex m_mutex;                    ///< Protect members from concurrent access
    static constexpr int MAX_MESSAGES = 10000; ///< Max messages in buffer
};
