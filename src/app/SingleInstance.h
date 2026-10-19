#pragma once

#include <QObject>
#include <QStringList>

class QLocalServer;

namespace ps {

// Только один экземпляр (APP-2). Повторный запуск передаёт команду работающему
// экземпляру через локальный сокет и завершается. Так же работает CLI
// `pointshot --capture` для окружений без портала клавиш (CAP-7).
class SingleInstance : public QObject
{
    Q_OBJECT
public:
    explicit SingleInstance(const QString &appId, QObject *parent = nullptr);

    // true — мы первый экземпляр и слушаем сокет.
    bool listen();
    // Отправить команду работающему экземпляру. false — его нет.
    bool sendToRunning(const QString &command, int timeoutMs = 1000);

signals:
    void commandReceived(const QString &command);

private:
    QString m_name;
    QLocalServer *m_server = nullptr;
};

} // namespace ps
