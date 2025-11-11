#include <QCommandLineParser>
#include <QFileInfo>

#include "ArgumentParser.h"
#include "Settings.h"


void parseArguments(const QStringList &args) {

    QCommandLineParser parser;
    parser.setApplicationDescription("TradeStation Trading Algorithm");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption configCriteriaOption("criterias", "Path to the criterias file (e.g., criterias.ini)", "file", "./criterias.ini");
    parser.addOption(configCriteriaOption);

    QCommandLineOption cacheRootDirOption("cache-root-dir", "Root directory for cache files", "dir");
    parser.addOption(cacheRootDirOption);

    // Process command-line arguments
    parser.process(args);



    QString criteriaFile = parser.value(configCriteriaOption);
    {
        QFileInfo fileInfo(criteriaFile);
        if (!fileInfo.exists() || !fileInfo.isFile()) {
            qFatal() << "Error: The specified path does not exist or is not a file : " << criteriaFile;
        }
    }
    criteriaSettings = new QSettings(criteriaFile, QSettings::IniFormat);

    QString cacheDir = parser.value(cacheRootDirOption);
    if (!cacheDir.isEmpty()) {
        cacheRootDir = cacheDir;
    }
}
