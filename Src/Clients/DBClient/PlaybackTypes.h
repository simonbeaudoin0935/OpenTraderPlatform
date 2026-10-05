#pragma once

#include <QtGlobal>

/**
 * @brief Replay playback enums — lightweight header with no Databento dependency.
 *
 * Separated from DBClient.h so that consumers (MainAlgo, GUI) can use these types
 * without pulling in the heavy Databento headers.
 */
namespace Playback
{
    enum class State : quint8
    {
        Stopped,
        Playing,
        Paused
    };

    enum class Speed : int
    {
        SuperSlow = 1,        ///< 0.01x speed
        VerySlow = 10,        ///< 0.1x speed
        Half = 50,            ///< 0.5x speed
        Normal = 100,         ///< 1.0x speed (real-time)
        Double = 200,         ///< 2.0x speed
        Fast5x = 500,         ///< 5.0x speed
        Fast10x = 1000,       ///< 10.0x speed
        Fast25x = 2500,       ///< 25.0x speed
        Fast50x = 5000,       ///< 50.0x speed
        Fast100x = 10000,     ///< 100.0x speed
        AsFastAsPossible = -1 ///< 0ms timer
    };
} // namespace Playback
