#pragma once

#include "platform/Interfaces.h"
#include "PortalBus.h"

namespace ps::lx {

// Эмуляция Ctrl+V через портал RemoteDesktop (только клавиатура).
//
// Сессия открывается перед серией вставок и закрывается сразу после (ТЗ разд. 9):
// пока она открыта, окружение может показывать индикатор удалённого управления.
// Первый раз портал показывает диалог разрешения; с persist_mode=2 он отдаёт
// restore_token, по которому следующие сессии открываются без диалога.
class PortalRemoteDesktop : public IInputInjector
{
    Q_OBJECT
public:
    explicit PortalRemoteDesktop(QObject *parent = nullptr);
    ~PortalRemoteDesktop() override;

    bool isAvailable() const override;
    QFuture<Status> begin() override;
    void end() override;
    QFuture<Status> waitModifiersReleased(std::chrono::milliseconds timeout) override;
    QFuture<Status> sendPaste() override;
    bool canDetectForegroundChange() const override { return false; }
    QString name() const override { return QStringLiteral("xdg-desktop-portal RemoteDesktop"); }

    void setRestoreToken(const QString &token) { m_restoreToken = token; }
    QString restoreToken() const { return m_restoreToken; }

    // Запросить доступ к буферу (портал Clipboard) в этой же сессии. Нужно до begin().
    void setRequestClipboard(bool enabled) { m_requestClipboard = enabled; }
    bool clipboardEnabled() const { return m_clipboardEnabled; }
    QString sessionHandle() const { return m_session; }

    // evdev-коды (linux/input-event-codes.h). Нажимаем по коду, а не по символу:
    // при русской раскладке символа «v» нет (ТЗ разд. 10).
    static constexpr int kKeyLeftCtrl = 29;
    static constexpr int kKeyV = 47;

signals:
    void restoreTokenChanged(const QString &token);
    void sessionClosed();

private slots:
    void onSessionClosed(const QVariantMap &details);

private:
    void fail(const QString &error);
    void finishBegin(const Status &status);
    QFuture<Status> key(int keycode, bool pressed);

    QString m_restoreToken;
    QString m_session;
    bool m_requestClipboard = false;
    bool m_clipboardEnabled = false;
    bool m_starting = false;
    QList<std::shared_ptr<QPromise<Status>>> m_beginWaiters;
};

} // namespace ps::lx
