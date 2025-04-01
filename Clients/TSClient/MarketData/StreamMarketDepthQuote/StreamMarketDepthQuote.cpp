#include "StreamMarketDepthQuote.h"

StreamMarketDepthQuote::StreamMarketDepthQuote(QNetworkReply *reply) :
    Stream(reply)
{

}

StreamMarketDepthQuote::~StreamMarketDepthQuote()
{

}

void StreamMarketDepthQuote::processJson(QByteArray &json)
{

}
