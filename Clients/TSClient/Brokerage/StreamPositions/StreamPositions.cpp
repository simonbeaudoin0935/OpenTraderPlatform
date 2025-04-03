#include "StreamPositions.h"

StreamPositions::StreamPositions(QString &account, QObject *parent) :
    Stream(parent),
    account(account)
{}

StreamPositions::~StreamPositions()
{

}

bool StreamPositions::processJsonObject(const QJsonObject &jsonObj)
{
    Position position(jsonObj);

#warning  I need to parse streamStatus string Provides information about the stream status. When the initial snapshot is complete, "EndSnapshot" is returned. When the server is about to shut down, "GoAway" is returned to indicate that the stream will close because of server shutdown, and that a new stream will need to be started by the client.
    if (position.isValid()) {
        emit receivedNewPosition(account, position);
        return true;
    } else {
        QJsonDocument doc(jsonObj);
        QString jsonString = QString(doc.toJson(QJsonDocument::Indented));
        qCWarning(StreamLog) << Q_FUNC_INFO <<
            "Position update object invalid : " << jsonString <<
            "Malformed object to string : " << position.toJsonString();

        return false;
    }

    return false;
}
