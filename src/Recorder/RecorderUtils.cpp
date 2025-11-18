#include "RecorderUtils.h"

QString streamErrorToString(Stream::StreamError error) {
    switch (error) {
        case Stream::StreamError::Timeout: return "Timeout";
        case Stream::StreamError::BadRequest: return "BadRequest";
        case Stream::StreamError::DualLogon: return "DualLogon";
        case Stream::StreamError::GoAway: return "GoAway";
        case Stream::StreamError::InternalServerError: return "InternalServerError";
        case Stream::StreamError::InvalidSymbol: return "InvalidSymbol";
        case Stream::StreamError::Unknown: return "Unknown";
        default: return "Unknown";
    }
}