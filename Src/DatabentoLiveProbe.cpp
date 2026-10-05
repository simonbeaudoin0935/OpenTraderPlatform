#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>

#include <databento/constants.hpp>
#include <databento/enums.hpp>
#include <databento/live.hpp>
#include <databento/log.hpp>
#include <databento/record.hpp>
#include <databento/symbol_map.hpp>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "SecureStorage.h"

namespace
{
    constexpr auto kService = "Databento";
    constexpr auto kKeyName = "api_key";

    [[nodiscard]] QString loadApiKey(const QString& p_cliKey)
    {
        if (!p_cliKey.isEmpty())
        {
            return p_cliKey;
        }

        const QString envKey = qEnvironmentVariable("DATABENTO_API_KEY");
        if (!envKey.isEmpty())
        {
            return envKey;
        }

        SecureStorage storage;
        return storage.retrieveValuesSync(kService, {kKeyName}).value(kKeyName);
    }

    [[nodiscard]] std::string resolveSymbol(const databento::PitSymbolMap& p_symbolMap,
                                            std::uint32_t p_instrumentId)
    {
        const auto it = p_symbolMap.Find(p_instrumentId);
        if (it != p_symbolMap.Map().end())
        {
            return it->second;
        }

        return "#" + std::to_string(p_instrumentId);
    }

    [[nodiscard]] double toPrice(const std::int64_t p_fixedPrice)
    {
        return static_cast<double>(p_fixedPrice) / databento::kFixedPriceScale;
    }

    void printTrade(const databento::PitSymbolMap& p_symbolMap, const databento::TradeMsg& p_trade)
    {
        std::cout << "trade symbol=" << resolveSymbol(p_symbolMap, p_trade.hd.instrument_id)
                  << " instrument_id=" << p_trade.hd.instrument_id
                  << " price=" << toPrice(p_trade.price)
                  << " size=" << p_trade.size
                  << " sequence=" << p_trade.sequence
                  << '\n';
    }

    void printMbp1(const databento::PitSymbolMap& p_symbolMap, const databento::Mbp1Msg& p_mbp1)
    {
        const auto& level = p_mbp1.levels.front();
        std::cout << "mbp1 symbol=" << resolveSymbol(p_symbolMap, p_mbp1.hd.instrument_id)
                  << " instrument_id=" << p_mbp1.hd.instrument_id
                  << " bid=" << toPrice(level.bid_px)
                  << " x " << level.bid_sz
                  << " ask=" << toPrice(level.ask_px)
                  << " x " << level.ask_sz
                  << " sequence=" << p_mbp1.sequence
                  << '\n';
    }
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("OpenTraderPlatform");

    QCommandLineParser parser;
    parser.setApplicationDescription("Temporary Databento live probe for EQUS.MINI-style feeds");
    parser.addHelpOption();

    const QCommandLineOption symbolOption("symbol", "Raw symbol to subscribe to.", "symbol", "NVDA");
    const QCommandLineOption datasetOption("dataset", "Databento dataset to use.", "dataset", "EQUS.MINI");
    const QCommandLineOption apiKeyOption("api-key", "Databento API key override.", "api-key");
    const QCommandLineOption timeoutOption("timeout-seconds", "How long to wait for market data.", "seconds", "15");
    const QCommandLineOption recordLimitOption("records", "How many market-data records to print before exiting.", "count", "10");

    parser.addOption(symbolOption);
    parser.addOption(datasetOption);
    parser.addOption(apiKeyOption);
    parser.addOption(timeoutOption);
    parser.addOption(recordLimitOption);
    parser.process(app);

    bool timeoutOk = false;
    const int timeoutSeconds = parser.value(timeoutOption).toInt(&timeoutOk);
    if (!timeoutOk || timeoutSeconds <= 0)
    {
        std::cerr << "Invalid --timeout-seconds value\n";
        return 2;
    }

    bool recordLimitOk = false;
    const int recordLimit = parser.value(recordLimitOption).toInt(&recordLimitOk);
    if (!recordLimitOk || recordLimit <= 0)
    {
        std::cerr << "Invalid --records value\n";
        return 2;
    }

    const QString apiKey = loadApiKey(parser.value(apiKeyOption));
    if (apiKey.isEmpty())
    {
        std::cerr << "No Databento API key found. Use --api-key, DATABENTO_API_KEY, or OpenTraderPlatform secure storage.\n";
        return 2;
    }

    const std::string symbol = parser.value(symbolOption).toStdString();
    const std::string dataset = parser.value(datasetOption).toStdString();

    std::cout << "Starting Databento probe"
              << " dataset=" << dataset
              << " symbol=" << symbol
              << " timeout_seconds=" << timeoutSeconds
              << " records=" << recordLimit
              << '\n';

    auto logReceiver = std::make_unique<databento::ConsoleLogReceiver>(databento::LogLevel::Info);
    databento::PitSymbolMap symbolMap;

    try
    {
        auto client = databento::LiveBlocking::Builder()
                          .SetLogReceiver(logReceiver.get())
                          .SetKey(apiKey.toStdString())
                          .SetDataset(dataset)
                          .BuildBlocking();

        client.Subscribe({symbol}, databento::Schema::Mbp1, databento::SType::RawSymbol);
        client.Subscribe({symbol}, databento::Schema::Trades, databento::SType::RawSymbol);

        const databento::Metadata metadata = client.Start();
        std::cout << "Connected successfully.\n";
        std::cout << metadata << '\n';

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeoutSeconds);
        int marketDataCount = 0;
        bool sawAnyRecord = false;
        bool sawAcceptedSubscription = false;

        while (marketDataCount < recordLimit)
        {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline)
            {
                break;
            }

            const auto remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
            const databento::Record* record = client.NextRecord(remaining);
            if (record == nullptr)
            {
                break;
            }

            sawAnyRecord = true;
            symbolMap.OnRecord(*record);

            if (const auto* error = record->GetIf<databento::ErrorMsg>())
            {
                std::cerr << "Gateway error: " << error->Err() << '\n';
                client.Stop();
                return 1;
            }

            if (const auto* system = record->GetIf<databento::SystemMsg>())
            {
                const std::string message = system->Msg();
                if (message.find("succeeded") != std::string::npos)
                {
                    sawAcceptedSubscription = true;
                }

                if (!system->IsHeartbeat())
                {
                    std::cout << "system " << message << '\n';
                }
                continue;
            }

            if (const auto* mapping = record->GetIf<databento::SymbolMappingMsg>())
            {
                std::cout << "mapping instrument_id=" << mapping->hd.instrument_id
                          << ' ' << mapping->STypeInSymbol()
                          << " -> " << mapping->STypeOutSymbol()
                          << '\n';
                continue;
            }

            if (const auto* mbp1 = record->GetIf<databento::Mbp1Msg>())
            {
                printMbp1(symbolMap, *mbp1);
                ++marketDataCount;
                continue;
            }

            if (const auto* trade = record->GetIf<databento::TradeMsg>())
            {
                printTrade(symbolMap, *trade);
                ++marketDataCount;
                continue;
            }

            std::cout << "record rtype=" << databento::ToString(record->RType())
                      << " instrument_id=" << record->Header().instrument_id
                      << '\n';
        }

        client.Stop();

        if (marketDataCount > 0)
        {
            std::cout << "Probe succeeded after receiving " << marketDataCount
                      << " market-data records.\n";
            return 0;
        }

        if (sawAcceptedSubscription || sawAnyRecord)
        {
            std::cout << "Probe connected and subscriptions were accepted, but no MBP-1 or trade records arrived before timeout.\n";
            return 0;
        }

        std::cerr << "Probe timed out waiting for live data.\n";
        return 1;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Probe failed: " << exception.what() << '\n';
        return 1;
    }
}
