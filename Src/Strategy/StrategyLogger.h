#pragma once

#include <QString>
#include <QDateTime>
#include <QVector>
#include <QMutex>
#include <memory>

/// @brief Log message with timestamp and level
struct StrategyLogMessage
{
    QDateTime timestamp;
    QtMsgType level; // Debug, Warning, Critical, etc.
    QString message;
};

/// @brief Per-strategy logging system
/// Captures logs from strategy thread and provides access via API
/// Logs are stored in circular buffer (max 10000 messages)
/// Can be saved to file at any time
class StrategyLogger
{
  public:
    /// @brief Create logger for a strategy
    /// @param p_strategyName Name of strategy (for log file naming)
    explicit StrategyLogger(const QString& p_strategyName);
    ~StrategyLogger() = default;

    /// @brief Log a message
    void log(QtMsgType p_level, const QString& p_message);

    /// @brief Get all logged messages (thread-safe copy)
    [[nodiscard]] QVector<StrategyLogMessage> getMessages() const;

    /// @brief Get messages since a specific timestamp
    [[nodiscard]] QVector<StrategyLogMessage> getMessagesSince(const QDateTime& p_since) const;

    /// @brief Get most recent N messages
    [[nodiscard]] QVector<StrategyLogMessage> getRecentMessages(int p_count) const;

    /// @brief Clear all logged messages
    void clear();

    /// @brief Save logs to file in ~/.local/share/L2Trader/logs/
    /// @return File path on success, empty string on failure
    [[nodiscard]] QString saveToFile();

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
    QVector<StrategyLogMessage> m_messages;    ///< Circular buffer of log messages
    mutable QMutex m_mutex;                    ///< Protect m_messages from concurrent access
    static constexpr int MAX_MESSAGES = 10000; ///< Max messages in buffer
};
