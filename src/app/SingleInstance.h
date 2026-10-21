#pragma once

#include <QObject>
#include <QStringList>

class QLocalServer;

namespace mp {

// Single instance (APP-2). A repeated launch passes its command to the running
// instance over a local socket and exits. The CLI `markpaste --capture` works the
// same way, for environments without a hotkeys portal (CAP-7).
class SingleInstance : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstance(const QString &appId, QObject *parent = nullptr);

    // true: we are the first instance and listen on the socket.
    bool listen();
    // Send a command to the running instance. false: there is none.
    bool sendToRunning(const QString &command, int timeoutMs = 1000);

signals:
    void commandReceived(const QString &command);

private:
    QString m_name;
    QLocalServer *m_server = nullptr;
};

} // namespace mp
