#include "StrategyLoader.h"
#include <QDebug>

std::expected<StrategyLoader::LoadedPlugin, StrategyLoader::Error> StrategyLoader::loadPlugin(const QString& p_soPath)
{
    if (p_soPath.isEmpty())
    {
        return std::unexpected(Error::InvalidConfig);
    }

    // Attempt to open the .so file
    void* handle = dlopen(p_soPath.toStdString().c_str(), RTLD_LAZY);
    if (!handle)
    {
        qWarning() << "Failed to load strategy plugin:" << p_soPath << "Error:" << dlerror();
        return std::unexpected(Error::FailedToOpen);
    }

    // Clear any existing error
    dlerror();

    // Resolve getStrategyAPIVersion
    auto getVersionFn = reinterpret_cast<GetVersionFn>(dlsym(handle, "getStrategyAPIVersion"));
    const char* dlerrorStr = dlerror();
    if (!getVersionFn || dlerrorStr)
    {
        qWarning() << "Strategy plugin missing getStrategyAPIVersion:" << p_soPath << "Error:" << dlerrorStr;
        dlclose(handle);
        return std::unexpected(Error::MissingVersionFn);
    }

    // Get and verify API version
    QString apiVersion = QString::fromUtf8(getVersionFn());
    if (!versionCompatible(apiVersion))
    {
        qWarning() << "Strategy plugin API version mismatch:" << "Expected:" << EXPECTED_API_VERSION
                   << "Got:" << apiVersion;
        dlclose(handle);
        return std::unexpected(Error::VersionMismatch);
    }

    // Resolve createStrategy factory
    dlerror();
    auto createFn = reinterpret_cast<CreateStrategyFn>(dlsym(handle, "createStrategy"));
    dlerrorStr = dlerror();
    if (!createFn || dlerrorStr)
    {
        qWarning() << "Strategy plugin missing createStrategy:" << p_soPath << "Error:" << dlerrorStr;
        dlclose(handle);
        return std::unexpected(Error::MissingFactoryFn);
    }

    // Resolve destroyStrategy factory
    dlerror();
    auto destroyFn = reinterpret_cast<DestroyStrategyFn>(dlsym(handle, "destroyStrategy"));
    dlerrorStr = dlerror();
    if (!destroyFn || dlerrorStr)
    {
        qWarning() << "Strategy plugin missing destroyStrategy:" << p_soPath << "Error:" << dlerrorStr;
        dlclose(handle);
        return std::unexpected(Error::MissingDestroyFn);
    }

    qInfo() << "Successfully loaded strategy plugin:" << p_soPath << "API version:" << apiVersion;
    return LoadedPlugin{handle, createFn, destroyFn, apiVersion};
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
