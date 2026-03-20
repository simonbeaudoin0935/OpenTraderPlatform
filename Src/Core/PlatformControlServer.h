#pragma once

#include <QByteArray>
#include <QObject>
#include <QHash>
#include <QLocalServer>

class MainApp;
class QLocalSocket;
class QJsonObject;

class PlatformControlServer : public QObject
{
  public:
    explicit PlatformControlServer(MainApp* p_mainApp, QObject* p_parent = nullptr);
    ~PlatformControlServer() override;

    [[nodiscard]] bool startListening();
    void stopListening();

  private:
    void handleNewConnection();
    void handleSocketReadyRead(QLocalSocket* p_socket);
    void handleSocketDisconnected(QLocalSocket* p_socket);
    void sendResponse(QLocalSocket* p_socket, const QJsonObject& p_response);

    MainApp* m_mainApp = nullptr;
    QLocalServer m_server;
    QHash<QLocalSocket*, QByteArray> m_socketBuffers;
};
