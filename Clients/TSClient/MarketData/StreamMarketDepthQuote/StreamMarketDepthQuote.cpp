#include "StreamMarketDepthQuote.h"

StreamMarketDepthQuote::StreamMarketDepthQuote() :
    Stream()
{
}

StreamMarketDepthQuote::~StreamMarketDepthQuote()
{
}

bool StreamMarketDepthQuote::processJsonObject(const QJsonObject& jsonObj)
{
    qCritical() << "NOT YET IMPLEMENTED";

    return false;
}
