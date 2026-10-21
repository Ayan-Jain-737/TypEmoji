/**
 * SingleInstance implementation - QLocalServer/QLocalSocket IPC
 */
#include "SingleInstance.h"
#include <QtCore/QDebug>
#include <QtCore/QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>

SingleInstance::SingleInstance(const QString& instanceKey, QObject* parent)
    : QObject(parent), m_instanceKey(instanceKey)
{
    // Try to connect to existing instance
    QLocalSocket socket;
    socket.connectToServer(m_instanceKey);

    if (socket.waitForConnected(100)) {
        // Another instance is running
        m_anotherRunning = true;
        qDebug() << "Another instance is already running";
        socket.disconnectFromServer();
    } else {
        // No other instance, create server
        m_anotherRunning = false;
        createServer();
        qDebug() << "SingleInstance created server:" << m_instanceKey;
    }
}

SingleInstance::~SingleInstance()
{
    if (m_server) {
        m_server->close();
        delete m_server;
    }
}

void SingleInstance::createServer()
{
    m_server = new QLocalServer(this);
    connect(m_server, &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);

    // Remove any stale socket file
    QLocalServer::removeServer(m_instanceKey);

    if (!m_server->listen(m_instanceKey)) {
        qWarning() << "Failed to listen on" << m_instanceKey << ":" << m_server->errorString();
    }
}

void SingleInstance::connectToExisting()
{
    QLocalSocket socket;
    socket.connectToServer(m_instanceKey);

    if (socket.waitForConnected(500)) {
        socket.write("show\n");
        socket.flush();
        socket.waitForBytesWritten(500);
        socket.disconnectFromServer();
    }
}

void SingleInstance::setMessageHandler(std::function<void(const QString&)> handler)
{
    m_messageHandler = handler;
}

void SingleInstance::sendShowMessage()
{
    connectToExisting();
}

void SingleInstance::onNewConnection()
{
    QLocalSocket* socket = m_server->nextPendingConnection();
    if (!socket) return;

    connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
        QByteArray data = socket->readAll();
        QString msg = QString::fromUtf8(data).trimmed();
        if (m_messageHandler) {
            m_messageHandler(msg);
        }
        emit messageReceived(msg);
    });

    connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
}