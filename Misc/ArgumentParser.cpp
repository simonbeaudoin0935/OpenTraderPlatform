#include <QCommandLineParser>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QFile>
#include <QTextStream>

#include "ArgumentParser.h"
#include "Settings.h"

void parseArguments(const QStringList &args) {
    QCommandLineParser parser;
    parser.setApplicationDescription("TradeStation Trading Algorithm");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption configConfigOption("config", "Path to the config file (e.g., config.ini)", "file", "./config.ini");
    parser.addOption(configConfigOption);

    QCommandLineOption configCriteriaOption("criterias", "Path to the criterias file (e.g., criterias.ini)", "file", "./criterias.ini");
    parser.addOption(configCriteriaOption);

    QCommandLineOption loggingOption("logging", "Path to the logging configuration file (e.g., logging.ini)", "file", "./logging.ini");
    parser.addOption(loggingOption);

    // Process command-line arguments
    parser.process(args);

    // Handle logging configuration
    QString logFile = parser.value(loggingOption);
    {
        QFileInfo fileInfo(logFile);
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            qWarning() << "Fatal: The specified logging configuration file does not exist or is not a file : " << logFile;
        } else {
            QFile file(logFile);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                qFatal() << "Fatal: Could not open logging configuration file:" << logFile;
            }
            
            QString filterRules;
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (!line.isEmpty() && !line.startsWith('#')) {  // Skip empty lines and comments
                    if (!filterRules.isEmpty()) {
                        filterRules += '\n';
                    }
                    filterRules += line;
                }
            }
            file.close();
            
            QLoggingCategory::setFilterRules(filterRules);
            qDebug() << "Using logging configuration from:" << logFile;
        }
    }

    QString configFile = parser.value(configConfigOption);
    {
        QFileInfo fileInfo(configFile);
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            qCritical() << "Error: The specified path does not exist or is not a file : " << configFile;
            // TODO tackle
        }
    }
    configSettings = new QSettings(configFile, QSettings::IniFormat);

    QString criteriaFile = parser.value(configCriteriaOption);
    {
        QFileInfo fileInfo(criteriaFile);
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            qFatal() << "Error: The specified path does not exist or is not a file : " << criteriaFile;
        }
    }
    criteriaSettings = new QSettings(criteriaFile, QSettings::IniFormat);
}
