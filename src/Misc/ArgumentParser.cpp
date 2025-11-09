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

    // Process command-line arguments
    parser.process(args);



    QString configFile = parser.value(configConfigOption);
    {
        QFileInfo fileInfo(configFile);
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            qWarning() << "IGNORE: The specified path does not exist or is not a file : " << configFile;
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
