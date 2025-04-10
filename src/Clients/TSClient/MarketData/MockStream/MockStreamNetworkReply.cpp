#include <QIODevice>

#include "MockStreamNetworkReply.h"

MockStreamNetworkReply::MockStreamNetworkReply(const QString &mockDataFile, QObject* parent)
    : QNetworkReply(parent) {
    open(QIODevice::ReadOnly);
    setAttribute(QNetworkRequest::HttpStatusCodeAttribute, 200); // Simulate success

    file.setFileName(mockDataFile);

    if (!file.open(QIODevice::ReadOnly)) {
        qFatal() << "Error: Could not open file" << file.fileName() << ":" << file.errorString();
    }

    textSteam.setDevice(&file);
}

void MockStreamNetworkReply::setData(const QByteArray& data) {
    QByteArray *array = new QByteArray(data);
    buffer.enqueue(array);

    emit readyRead(); // Trigger reading of mock data

    // TODO IMPLEMENT THE FINISHED AS WELL
    //emit finished();
}


qint64 MockStreamNetworkReply::bytesAvailable() const {
    if (buffer.isEmpty()){
        return 0;
    } else {
        return buffer.head()->size();
    }
}

void MockStreamNetworkReply::abort() {
    Q_ASSERT_X(0, "abort", "TODO implement");
}

qint64 MockStreamNetworkReply::readData(char* data, qint64 maxSize) {

    if (buffer.isEmpty()){
        return 0;
    }

    QByteArray *array = buffer.dequeue();

    qint64 size = array->size();

    if(size > maxSize){
        qFatal() << "fuck";
    }

    memcpy(data, array->constData(), array->size());

    delete array;

    return size;
}
