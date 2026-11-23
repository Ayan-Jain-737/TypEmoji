/**
 * SingleInstance implementation - QLockFile & QLocalServer/QLocalSocket IPC
 */
#include "SingleInstance.h"
#include <QtCore/QDebug>
#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QLocalServer>
#include <QLocalSocket>

SingleInstance::SingleInstance(const QString& instanceKey, QObject* parent)
    : QObject(parent), m_instanceKey(instanceKey)
{
    // 1. Process-level lock file guarantees race-free single instance
    QString lockPath = QDir::tempPath() + "/" + m_instanceKey + ".lock";
    m_lockFile = std::make_unique<QLockFile>(lockPath);
    m_lockFile->setStaleLockTime(30000);

    if (!m_lockFile->tryLock(200)) {
        // Lock held by another instance
        m_anotherRunning = true;
        qDebug() << "SingleInstance: Lock file held by another instance";
        return;
    }

    // 2. We hold the lock. Check if an active server is listening
    QLocalSocket socket;
    socket.connectToServer(m_instanceKey);

    if (socket.waitForConnected(200)) {
        // Another instance is running and responding
        m_anotherRunning = true;
        qDebug() << "SingleInstance: Another instance is already running";
        socket.disconnectFromServer();
        m_lockFile->unlock();
        return;
    }

    // 3. No other instance. Create server
    m_anotherRunning = false;
    createServer();
    qDebug() << "SingleInstance created server:" << m_instanceKey;
}

SingleInstance::~SingleInstance()
{
    if (m_server) {
        m_server->close();
        delete m_server;
        QLocalServer::removeServer(m_instanceKey);
    }
    if (m_lockFile && m_lockFile->isLocked()) {
        m_lockFile->unlock();
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
        m_anotherRunning = true;
        if (m_lockFile && m_lockFile->isLocked()) {
            m_lockFile->unlock();
        }
    }
}

void SingleInstance::connectToExisting()
{
    sendMessage("show");
}

bool SingleInstance::sendMessage(const QString& message)
{
    QLocalSocket socket;
    socket.connectToServer(m_instanceKey);

    if (socket.waitForConnected(500)) {
        QByteArray data = message.toUtf8();
        if (!data.endsWith('\n')) {
            data.append('\n');
        }
        socket.write(data);
        socket.flush();
        socket.waitForBytesWritten(500);
        socket.disconnectFromServer();
        return true;
    }
    return false;
}

void SingleInstance::setMessageHandler(std::function<void(const QString&)> handler)
{
    m_messageHandler = handler;
}

void SingleInstance::sendShowMessage()
{
    sendMessage("show");
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