#include "UpdateChecker.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSettings>
#include <QTimer>
#include <QVersionNumber>

#include <algorithm>

Q_LOGGING_CATEGORY(lcUpdate, "markpaste.update")

namespace mp {

static constexpr qint64 kIntervalMs = 24LL * 60 * 60 * 1000;

UpdateChecker::UpdateChecker(const QString &currentVersion, const QUrl &url, QObject *parent)
    : QObject(parent), m_current(currentVersion), m_url(url), m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &UpdateChecker::checkNow);
}

void UpdateChecker::setEnabled(bool enabled, int startDelayMs)
{
    m_timer->stop();
    if (!enabled || !m_url.isValid() || m_url.isEmpty())
        return;
    const QDateTime last = QSettings().value(QStringLiteral("updates/lastCheck")).toDateTime();
    const qint64 since = last.isValid() ? last.msecsTo(QDateTime::currentDateTime()) : kIntervalMs;
    // since < 0 means the clock went back: do not wait longer than a day (and do not overflow int)
    m_timer->start(int(std::clamp<qint64>(kIntervalMs - since, startDelayMs, std::max<qint64>(startDelayMs, kIntervalMs))));
}

void UpdateChecker::checkNow()
{
    if (m_busy || m_url.isEmpty())
        return;
    if (!m_net)
        m_net = new QNetworkAccessManager(this);
    m_busy = true;
    QNetworkRequest req(m_url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Markpaste/%1").arg(m_current));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setTransferTimeout(15000);
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        onReply(reply->readAll(), reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(),
                reply->error() == QNetworkReply::NoError ? QString() : reply->errorString());
    });
}

void UpdateChecker::onReply(const QByteArray &body, int httpStatus, const QString &error)
{
    m_busy = false;
    QSettings s;
    s.setValue(QStringLiteral("updates/lastCheck"), QDateTime::currentDateTime());
    m_timer->start(int(kIntervalMs));
    // A network error or a closed project (404): silently, only to the log; this is not the user's concern
    if (!error.isEmpty()) {
        qCInfo(lcUpdate) << "update check failed:" << httpStatus << error;
        return;
    }
    const QString version = newerVersion(body, m_current);
    qCInfo(lcUpdate) << "update check: current" << m_current << "newer" << (version.isEmpty() ? QStringLiteral("none") : version);
    if (version.isEmpty() || s.value(QStringLiteral("updates/notifiedVersion")).toString() == version)
        return;
    s.setValue(QStringLiteral("updates/notifiedVersion"), version);
    emit updateAvailable(version, releasePage(body));
}

QUrl UpdateChecker::releasePage(const QByteArray &json)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    QString page = o.value(QStringLiteral("html_url")).toString();
    if (page.isEmpty())
        page = o.value(QStringLiteral("_links")).toObject().value(QStringLiteral("self")).toString();
    const QUrl url(page);
    return url.scheme() == u"https" ? url : QUrl();
}

QString UpdateChecker::newerVersion(const QByteArray &json, const QString &currentVersion)
{
    const QJsonObject o = QJsonDocument::fromJson(json).object();
    // GitHub drafts and pre-releases, GitLab upcoming releases are not a new version
    if (o.value(QStringLiteral("upcoming_release")).toBool() || o.value(QStringLiteral("draft")).toBool()
        || o.value(QStringLiteral("prerelease")).toBool())
        return {};
    QString tag = o.value(QStringLiteral("tag_name")).toString().trimmed();
    if (tag.startsWith(u'v') || tag.startsWith(u'V'))
        tag.remove(0, 1);
    const QVersionNumber latest = QVersionNumber::fromString(tag);
    const QVersionNumber current = QVersionNumber::fromString(currentVersion);
    if (latest.isNull() || QVersionNumber::compare(latest, current) <= 0)
        return {};
    return latest.toString();
}

} // namespace mp
