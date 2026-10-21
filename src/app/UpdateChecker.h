#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QTimer;

namespace mp {

// Уведомление о новой версии (APP-7, Linux): раз в сутки один GET к API релизов.
// Это единственный сетевой запрос программы, отключается в настройках (NFR-4).
// Обновление — через пакетный менеджер, программа сама ничего не скачивает.
//
// Источник — GitHub: GET api.github.com/repos/<владелец>/<имя>/releases/latest →
// {"tag_name": "v0.2.0", "html_url": …}. Ответ GitLab (`_links.self`) тоже понимаем.
// Адрес задаётся при сборке (MARKPASTE_UPDATE_URL), пустой — проверка выключена в сборке.
class UpdateChecker : public QObject
{
    Q_OBJECT
public:
    UpdateChecker(const QString &currentVersion, const QUrl &url, QObject *parent = nullptr);

    // Включить или выключить периодическую проверку. Первая — через startDelayMs после
    // включения, если с прошлой прошло больше суток (время хранится в настройках).
    void setEnabled(bool enabled, int startDelayMs = 30000);
    void checkNow();

    // Разбор ответа и сравнение версий — для тестов. Пустая строка — новее нет.
    static QString newerVersion(const QByteArray &json, const QString &currentVersion);
    // Страница релиза из ответа: html_url (GitHub) или _links.self (GitLab)
    static QUrl releasePage(const QByteArray &json);

signals:
    // Сообщается один раз на версию: повтор после перезапуска не надоедает
    void updateAvailable(const QString &version, const QUrl &page);

private:
    void onReply(const QByteArray &body, int httpStatus, const QString &error);

    QString m_current;
    QUrl m_url;
    QNetworkAccessManager *m_net = nullptr;
    QTimer *m_timer;
    bool m_busy = false;
};

} // namespace mp
