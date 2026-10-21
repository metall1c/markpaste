#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QTimer;

namespace mp {

// New version notification (APP-7, Linux): one GET to the releases API once a day.
// This is the program's only network request; it can be turned off in settings (NFR-4).
// Updating is done by the package manager, the program downloads nothing itself.
//
// Source: GitHub, GET api.github.com/repos/<owner>/<name>/releases/latest →
// {"tag_name": "v0.2.0", "html_url": …}. A GitLab answer (`_links.self`) is understood too.
// The URL is set at build time (MARKPASTE_UPDATE_URL); empty disables the check in the build.
class UpdateChecker : public QObject
{
    Q_OBJECT
public:
    UpdateChecker(const QString &currentVersion, const QUrl &url, QObject *parent = nullptr);

    // Turn the periodic check on or off. The first one runs startDelayMs after it is
    // enabled, if more than a day has passed since the last one (the time is kept in settings).
    void setEnabled(bool enabled, int startDelayMs = 30000);
    void checkNow();

    // Parsing the answer and comparing versions, for tests. An empty string means nothing newer.
    static QString newerVersion(const QByteArray &json, const QString &currentVersion);
    // Release page from the answer: html_url (GitHub) or _links.self (GitLab)
    static QUrl releasePage(const QByteArray &json);

signals:
    // Reported once per version: a repeat after a restart does not nag
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
