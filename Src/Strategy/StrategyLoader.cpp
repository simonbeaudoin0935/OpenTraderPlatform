#include "StrategyLoader.h"
#include <QDebug>

std::expected<StrategyLoader::LoadedPlugin, QString> StrategyLoader::loadPlugin(const QString& p_soPath)
{
    if (p_soPath.isEmpty())
    {
        return std::unexpected(QString("Invalid strategy configuration: .so path is empty"));
    }

    // Attempt to open the .so file
    void* handle = dlopen(p_soPath.toStdString().c_str(), RTLD_LAZY);
    if (!handle)
    {
        QString detail = QString::fromUtf8(dlerror());
        qWarning() << "Failed to load strategy plugin:" << p_soPath << "Error:" << detail;
        return std::unexpected("Failed to open plugin: " + detail);
    }

    // Clear any existing error
    dlerror();

    // Resolve getStrategyAPIVersion
    auto getVersionFn = reinterpret_cast<GetVersionFn>(dlsym(handle, "getStrategyAPIVersion"));
    const char* dlerrorStr = dlerror();
    if (!getVersionFn || dlerrorStr)
    {
        QString detail = QString::fromUtf8(dlerrorStr);
        qWarning() << "Strategy plugin missing getStrategyAPIVersion:" << p_soPath << "Error:" << detail;
        dlclose(handle);
        return std::unexpected("Missing getStrategyAPIVersion: " + detail);
    }

    // Get and verify API version
    QString apiVersion = QString::fromUtf8(getVersionFn());
    if (!versionCompatible(apiVersion))
    {
        QString detail = QString("expected %1, got %2").arg(EXPECTED_API_VERSION, apiVersion);
        qWarning() << "Strategy plugin API version mismatch:" << "Expected:" << EXPECTED_API_VERSION
                   << "Got:" << apiVersion;
        dlclose(handle);
        return std::unexpected("API version mismatch: " + detail);
    }

    // Resolve createStrategy factory
    dlerror();
    auto createFn = reinterpret_cast<CreateStrategyFn>(dlsym(handle, "createStrategy"));
    dlerrorStr = dlerror();
    if (!createFn || dlerrorStr)
    {
        QString detail = QString::fromUtf8(dlerrorStr);
        qWarning() << "Strategy plugin missing createStrategy:" << p_soPath << "Error:" << detail;
        dlclose(handle);
        return std::unexpected("Missing createStrategy: " + detail);
    }

    // Resolve destroyStrategy factory
    dlerror();
    auto destroyFn = reinterpret_cast<DestroyStrategyFn>(dlsym(handle, "destroyStrategy"));
    dlerrorStr = dlerror();
    if (!destroyFn || dlerrorStr)
    {
        QString detail = QString::fromUtf8(dlerrorStr);
        qWarning() << "Strategy plugin missing destroyStrategy:" << p_soPath << "Error:" << detail;
        dlclose(handle);
        return std::unexpected("Missing destroyStrategy: " + detail);
    }

    // Resolve optional getParameterSchema (backward compatible — absence is fine)
    dlerror();
    auto schemaFn = reinterpret_cast<GetParameterSchemaFn>(dlsym(handle, "getParameterSchema"));
    dlerror(); // clear possible "symbol not found" error — this export is optional

    qInfo() << "Successfully loaded strategy plugin:" << p_soPath << "API version:" << apiVersion
            << (schemaFn ? "| schema: yes" : "| schema: no");
    return LoadedPlugin{handle, createFn, destroyFn, apiVersion, schemaFn};
}

void StrategyLoader::unloadPlugin(LoadedPlugin& p_plugin)
{
    if (p_plugin.p_handle)
    {
        dlclose(p_plugin.p_handle);
        p_plugin.p_handle = nullptr;
        qInfo() << "Unloaded strategy plugin";
    }
}

bool StrategyLoader::versionCompatible(const QString& p_strategyVersion)
{
    // Currently: exact match required
    // Future: implement semantic versioning (e.g., 1.0.x compatible with 1.0.0)
    return p_strategyVersion == EXPECTED_API_VERSION;
}

QString StrategyLoader::errorMessage(Error p_error, const QString& p_details)
{
    switch (p_error)
    {
    case Error::FailedToOpen:
        return "Failed to open strategy plugin: " + p_details;
    case Error::MissingFactoryFn:
        return "Strategy plugin missing createStrategy factory function";
    case Error::MissingDestroyFn:
        return "Strategy plugin missing destroyStrategy factory function";
    case Error::MissingVersionFn:
        return "Strategy plugin missing getStrategyAPIVersion function";
    case Error::VersionMismatch:
        return "Strategy API version mismatch: " + p_details;
    case Error::InvalidConfig:
        return "Invalid strategy configuration";
    case Error::UnknownError:
        return "Unknown error loading strategy plugin";
    }
    return "Unknown error";
}

QJsonArray StrategyLoader::peekParameterSchema(const QString& p_soPath)
{
    void* handle = dlopen(p_soPath.toStdString().c_str(), RTLD_LAZY);
    if (!handle)
        return {};

    dlerror();
    auto schemaFn = reinterpret_cast<GetParameterSchemaFn>(dlsym(handle, "getParameterSchema"));
    QJsonArray schema;
    if (!dlerror() && schemaFn)
        schema = schemaFn();

    dlclose(handle);
    return schema;
}
