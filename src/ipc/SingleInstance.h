/**
 * SingleInstance - Ensures only one instance runs (QLocalServer)
 */
#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <memory>

class QLocalServer;
class QLocalSocket;

class SingleInstance : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstance(const QString& instanceKey, QObject* parent = nullptr);
    ~SingleInstance() override;

    bool isAnotherRunning() const { return m_anotherRunning; }

    void setMessageHandler(std::function<void(const QString&)> handler);
    void sendShowMessage();

signals:
    void messageReceived(const QString& message);

private slots:
    void onNewConnection();

private:
    void createServer();
    void connectToExisting();

    QString m_instanceKey;
    QLocalServer* m_server = nullptr;
    bool m_anotherRunning = false;
    std::function<void(const QString&)> m_messageHandler;
};