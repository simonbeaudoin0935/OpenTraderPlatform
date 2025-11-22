#ifndef RECORDER_UTILS_H
#define RECORDER_UTILS_H

#include <QString>
#include "Clients/TSClient/Stream/Stream.h"

QString streamErrorToString(Stream::StreamError error);

#endif // RECORDER_UTILS_H