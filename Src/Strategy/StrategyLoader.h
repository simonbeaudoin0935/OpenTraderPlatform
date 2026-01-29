#pragma once

#include <QString>
#include <QVector>
#include <memory>
#include <expected>
#include <dlfcn.h>

#include "Strategy.h"
#include "StrategySDK.h"

/*
 * StrategyLoader - Dynamically loads strategy .so files and creates instances
 *
 * Handles:
 * - Dynamic loading (dlopen) and unloading (dlclose) of strategy plugins
 * - Factory function resolution (createStrategy, destroyStrategy,
 * getStrategyAPIVersion)
 * - API version compatibility checking
 * - Error reporting for missing symbols or version mismatches
 */
class StrategyLoader final
{
  public:
    enum class Error
    {
        FailedToOpen,     // dlopen() failed
        MissingFactoryFn, // createStrategy not found
        MissingDestroyFn, // destroyStrategy not found
        MissingVersionFn, // getStrategyAPIVersion not found
        VersionMismatch,  // API version incompatible
        InvalidConfig,    // Invalid StrategyConfig
        UnknownError,     // dlopen/dlsym internal error
    };

    // Factory function signatures that strategy plugins must export
    using CreateStrategyFn = Strategy* (*)(const StrategyConfig& config, StrategySDK* p_sdk);
    using DestroyStrategyFn = void (*)(Strategy* strategy);
    using GetVersionFn = const char* (*)();

    struct LoadedPlugin
    {
        void* p_handle;              // dlopen handle
        CreateStrategyFn createFn;   // createStrategy factory
        DestroyStrategyFn destroyFn; // destroyStrategy factory
        QString apiVersion;          // API version string
    };

    /*
     * Load a strategy plugin from a .so file
     *
     * @param p_soPath - Path to .so file (e.g., "/path/to/strategy.so")
     * @return LoadedPlugin on success, Error on failure
     *
     * Expected .so exports:
     *   extern "C" const char* getStrategyAPIVersion();
     *   extern "C" Strategy* createStrategy(const StrategyConfig& config,
     * StrategySDK* sdk);
     *   extern "C" void destroyStrategy(Strategy* strategy);
     */
    [[nodiscard]] static std::expected<LoadedPlugin, Error> loadPlugin(const QString& p_soPath);

    /*
     * Unload a strategy plugin
     *
     * @param p_plugin - LoadedPlugin to unload (calls dlclose)
     */
    static void unloadPlugin(LoadedPlugin& p_plugin);

  private:
    // Expected API version (strategies must match this)
    static constexpr const char* EXPECTED_API_VERSION = "1.0.0";

    /*
     * Check if two version strings are compatible
     * Currently: exact match required (1.0.0 == 1.0.0)
     * Future: could implement semantic versioning
     */
    static bool versionCompatible(const QString& p_strategyVersion);

    /*
     * Get human-readable error message for Error enum
     */
    static QString errorMessage(Error p_error, const QString& p_details = "");
};
