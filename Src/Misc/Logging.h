#pragma once

#include <QtGlobal>
#include <QTextStream>
#include <QMap>
#include <QSettings>
#include <QObject>
#include <QMetaEnum>

#include "Assume.h"

#define DEBUG qCDebug(LOGGING_CATEGORY) << this->objectName()
#define sDEBUG qCDebug(LOGGING_CATEGORY) << __FUNCTION__

#define INFO qCInfo(LOGGING_CATEGORY) << this->objectName()
#define sINFO qCInfo(LOGGING_CATEGORY) << __FUNCTION__

#define WARNING qCWarning(LOGGING_CATEGORY) << this->objectName()
#define sWARNING qCWarning(LOGGING_CATEGORY) << __FUNCTION__

#define CRITICAL qCCritical(LOGGING_CATEGORY) << this->objectName()
#define sCRITICAL qCCritical(LOGGING_CATEGORY) << __FUNCTION__

#define FATAL qFatal()

namespace QtEnum
{
    // Converts an enum value to its string representation
    // @note Works only for enums registered with Q_ENUM
    // @note Assumes the enum value is valid; asserts if not
    template<typename Enum> QString toString(Enum value)
    {
        const QMetaEnum meta = QMetaEnum::fromType<Enum>();

        const char* keyPtr = meta.valueToKey(static_cast<int>(value));
        ASSUME_TRUE(keyPtr != nullptr);

        return QString(keyPtr);
    }

    // Converts a string to the corresponding enum value
    // @note Assumes the string is valid; asserts if not
    template<typename Enum> Enum fromString(QStringView str)
    {
        bool ok = false;
        const QMetaEnum meta = QMetaEnum::fromType<Enum>();

        int value = meta.keyToValue(str.toLatin1().constData(), &ok);
        if (!ok)
        {
            qCritical() << "QtEnum::fromString: Invalid enum string:" << str << "for enum type" << meta.name();
            Q_UNREACHABLE();
        }


        return static_cast<Enum>(value);
    }
} // namespace QtEnum

void initLogging();
void reinstallColoredMessageHandler();

// Singleton to broadcast log messages to GUI
class LogBroadcaster : public QObject
{
    Q_OBJECT
  public:
    static LogBroadcaster& instance();

    void broadcastLogMessage(const QString& message);

  signals:
    void logMessageReceived(const QString& message);

  private:
    LogBroadcaster() : QObject(nullptr) {}
    ~LogBroadcaster() = default;

    Q_DISABLE_COPY(LogBroadcaster) // Delete copy constructor and assignment operator
};

class LoggingConfig
{
  public:
    static LoggingConfig& instance();

    bool isCategoryEnabled(const QString& category) const;
    void setCategoryEnabled(const QString& category, bool enabled);
    QStringList getCategories() const;
    void writeConfigToDisk();

    bool isDebugDisabled() const;
    void setDebugDisabled(bool disabled);
    bool isInfoDisabled() const;
    void setInfoDisabled(bool disabled);

  private:
    LoggingConfig();
    ~LoggingConfig() = default;

    Q_DISABLE_COPY(LoggingConfig) // Delete copy constructor and assignment operator

    QMap<QString, bool> m_categoryEnabled;
    QStringList m_categories;
    QSettings m_settings;
};