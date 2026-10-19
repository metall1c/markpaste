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
    // Сначала спрашиваем, жив ли первый экземпляр. На listen() полагаться нельзя:
    // с UserAccessOption Qt 6 создаёт сокет во временном каталоге и переименовывает его
    // поверх нужного пути, так что listen() «успешен» у каждого нового процесса (docs/problems.md).
    if (sendToRunning(QStringLiteral("ping"), 300))
        return false;
    // Сокет мог остаться от упавшего процесса — никто не ответил, убираем.
    QLocalServer::removeServer(m_name);
    m_server = new QLocalServer(this);
    // UserAccessOption не нужен: сокет лежит в $XDG_RUNTIME_DIR, доступном только владельцу.
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
