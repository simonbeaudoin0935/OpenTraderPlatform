#include <QCommandLineParser>

#include "argumentparser.h"
#include "settings.h"

void parseArguments(const QStringList &args) {
    QCommandLineParser parser;
    parser.setApplicationDescription("TradeStation Trading Algorithm");
    parser.addHelpOption();
    parser.addVersionOption();


    QCommandLineOption configTokensOption("tokens", "Path to the tokens file (e.g., tokens.ini)", "file", "./tokens.ini");
    parser.addOption(configTokensOption);

    QCommandLineOption configConfigOption("config", "Path to the config file (e.g., config.ini)", "file", "./config.ini");
    parser.addOption(configConfigOption);

    QCommandLineOption configCriteriaOption("criterias", "Path to the criterias file (e.g., criterias.ini)", "file", "./criterias.ini");
    parser.addOption(configCriteriaOption);

    // Process command-line arguments
    parser.process(args);

    QString tokensFile = parser.value(configTokensOption);
    if(tokensFile.isEmpty()){
        qFatal() << "No tokens file specified. Usage: ./tradestation_algo --tokens <tokens_file>";
    }

    tokensSettings = new QSettings(tokensFile, QSettings::IniFormat);

    QString configFile = parser.value(configConfigOption);
    if(configFile.isEmpty()){
        qFatal() << "No config file specified. Usage: ./tradestation_algo --config <config_file>";
    }

    configSettings = new QSettings(configFile, QSettings::IniFormat);

    QString criteriaFile = parser.value(configCriteriaOption);
    if(criteriaFile.isEmpty()){
        qFatal() << "No criteria file file specified. Usage: ./tradestation_algo --criteria <criteria_file>";
    }

    criteriaSettings = new QSettings(criteriaFile, QSettings::IniFormat);

}
