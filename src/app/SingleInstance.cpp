#include "SingleInstance.h"

#include "platform/Paths.h"

#include <QLocalServer>
#include <QLocalSocket>

namespace mp {

SingleInstance::SingleInstance(const QString &appId, QObject *parent)
    : QObject(parent)
{
    m_name = singleInstanceSocketName(appId);
}

bool SingleInstance::sendToRunning(const QString &command, int timeoutMs)
{
    QLocalSocket socket;
    socket.connectToServer(m_name);
    if (!socket.waitForConnected(timeoutMs))
        return false;
    socket.write(command.toUtf8() + '\n');
    socket.flush();
    socket.waitForBytesWritten(timeoutMs);
    socket.disconnectFromServer();
    return true;
}

bool SingleInstance::listen()
{
    // First ask whether the first instance is alive. listen() cannot be relied on:
    // with UserAccessOption Qt 6 creates the socket in a temporary directory and renames it
    // over the target path, so listen() "succeeds" for every new process (docs/problems.md).
    if (sendToRunning(QStringLiteral("ping"), 300))
        return false;
    // The socket may be left over from a crashed process: nobody answered, remove it.
    QLocalServer::removeServer(m_name);
    m_server = new QLocalServer(this);
    // UserAccessOption is not needed: the socket lives in $XDG_RUNTIME_DIR, accessible only to its owner.
    if (!m_server->listen(m_name))
        return false;
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *s = m_server->nextPendingConnection()) {
            connect(s, &QLocalSocket::readyRead, s, [this, s] {
                while (s->canReadLine()) {
                    const QString cmd = QString::fromUtf8(s->readLine()).trimmed();
                    if (!cmd.isEmpty() && cmd != u"ping")
                        emit commandReceived(cmd);
                }
            });
            connect(s, &QLocalSocket::disconnected, s, &QObject::deleteLater);
        }
    });
    return true;
}

} // namespace mp
