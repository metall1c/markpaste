#include "PortalClipboard.h"

#include "PortalBus.h"
#include "PortalRemoteDesktop.h"
#include "platform/linux/FdIo.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QLoggingCategory>
#include <QtConcurrent>

#include <fcntl.h>
#include <unistd.h>

Q_DECLARE_LOGGING_CATEGORY(lcPortal)

namespace mp::lx {

namespace {
const QString kIface = QStringLiteral("org.freedesktop.portal.Clipboard");

// Что восстанавливаем после вставки: только текст и картинки (ТЗ разд. 10) — как DataControlClipboard
const QStringList kTextMimes = {QStringLiteral("text/plain;charset=utf-8"), QStringLiteral("text/plain"),
                                QStringLiteral("UTF8_STRING")};
const QStringList kImageMimes = {QStringLiteral("image/png"), QStringLiteral("image/jpeg"), QStringLiteral("image/bmp")};

// fd из ответа D-Bus принадлежит QDBusUnixFileDescriptor и закрывается вместе с ним
int takeFd(const QDBusUnixFileDescriptor &d)
{
    return d.isValid() ? ::fcntl(d.fileDescriptor(), F_DUPFD_CLOEXEC, 0) : -1;
}
} // namespace

bool PortalClipboard::isAvailable()
{
    return PortalBus::instance().hasInterface(kIface);
}

PortalClipboard::PortalClipboard(PortalRemoteDesktop *session, QObject *parent)
    : IClipboard(parent), m_session(session)
{
    session->setRequestClipboard(true);
    QDBusConnection bus = PortalBus::instance().connection();
    bus.connect(PortalBus::kService, PortalBus::kPath, kIface, QStringLiteral("SelectionTransfer"), this,
                SLOT(onSelectionTransfer(QDBusObjectPath, QString, uint)));
    bus.connect(PortalBus::kService, PortalBus::kPath, kIface, QStringLiteral("SelectionOwnerChanged"), this,
                SLOT(onSelectionOwnerChanged(QDBusObjectPath, QVariantMap)));
    // Сессия закрылась — наш selection и сведения о чужом больше не действуют
    connect(session, &PortalRemoteDesktop::sessionClosed, this, [this] {
        m_owner = false;
        m_selectionMimes.clear();
    });
}

PortalClipboard::~PortalClipboard() = default;

QString PortalClipboard::sessionHandle() const
{
    return m_session ? m_session->sessionHandle() : QString();
}

QFuture<Status> PortalClipboard::write(const ClipboardPayload &payload, ClipboardFlags flags)
{
    const QString session = sessionHandle();
    if (session.isEmpty())
        return readyFuture(Status::failure(QStringLiteral("Clipboard portal: no RemoteDesktop session")));
    if (!m_session->clipboardEnabled())
        return readyFuture(Status::failure(QStringLiteral("Clipboard portal: access was not granted")));

    m_payload = payload;
    if (flags & ClipboardFlag::ExcludeFromHistory)   // Klipper не сохраняет такие записи (ТЗ разд. 10)
        m_payload.entries.append({QStringLiteral("x-kde-passwordManagerHint"), QByteArrayLiteral("secret")});
    m_owner = true;
    return PortalBus::instance().call(
        kIface, QStringLiteral("SetSelection"),
        {QVariant::fromValue(QDBusObjectPath(session)),
         QVariantMap{{QStringLiteral("mime_types"), m_payload.mimeTypes()}}});
}

void PortalClipboard::onSelectionTransfer(const QDBusObjectPath &session, const QString &mimeType, uint serial)
{
    if (session.path() != sessionHandle())
        return;
    QDBusConnection bus = PortalBus::instance().connection();
    auto done = [session, serial](bool ok) {
        QDBusMessage m = QDBusMessage::createMethodCall(PortalBus::kService, PortalBus::kPath, kIface,
                                                        QStringLiteral("SelectionWriteDone"));
        m << QVariant::fromValue(session) << serial << ok;
        PortalBus::instance().connection().asyncCall(m);
    };
    const ClipboardEntry *entry = m_owner ? m_payload.find(mimeType) : nullptr;
    if (!entry) {
        done(false);
        return;
    }
    const QByteArray data = entry->data;
    QDBusMessage m = QDBusMessage::createMethodCall(PortalBus::kService, PortalBus::kPath, kIface,
                                                    QStringLiteral("SelectionWrite"));
    m << QVariant::fromValue(session) << serial;
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(m), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, data, done, mimeType](QDBusPendingCallWatcher *w) {
        const QDBusPendingReply<QDBusUnixFileDescriptor> reply = *w;
        w->deleteLater();
        const int fd = reply.isError() ? -1 : takeFd(reply.value());
        if (fd < 0) {
            qCWarning(lcPortal) << "SelectionWrite failed:" << reply.error().message();
            done(false);
            return;
        }
        // Получатель читает в своём темпе: пишем в пуле потоков, а не в главном
        QtConcurrent::run([fd, data] { return writeAllAndClose(fd, data, 3000); }).then(this, [done, mimeType](bool ok) {
            if (!ok)
                qCWarning(lcPortal) << "clipboard transfer of" << mimeType << "was not completed";
            done(ok);
        });
    });
}

void PortalClipboard::onSelectionOwnerChanged(const QDBusObjectPath &session, const QVariantMap &options)
{
    if (session.path() != sessionHandle())
        return;
    if (!options.value(QStringLiteral("session_is_owner")).toBool()) {
        m_owner = false;
        m_selectionMimes = options.value(QStringLiteral("mime_types")).toStringList();
    }
}

QFuture<QByteArray> PortalClipboard::read(const QString &mimeType)
{
    auto promise = std::make_shared<QPromise<QByteArray>>();
    promise->start();
    QDBusMessage m = QDBusMessage::createMethodCall(PortalBus::kService, PortalBus::kPath, kIface,
                                                    QStringLiteral("SelectionRead"));
    m << QVariant::fromValue(QDBusObjectPath(sessionHandle())) << mimeType;
    auto *watcher = new QDBusPendingCallWatcher(PortalBus::instance().connection().asyncCall(m), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, promise](QDBusPendingCallWatcher *w) {
        const QDBusPendingReply<QDBusUnixFileDescriptor> reply = *w;
        w->deleteLater();
        const int fd = reply.isError() ? -1 : takeFd(reply.value());
        if (fd < 0) {
            promise->addResult(QByteArray());
            promise->finish();
            return;
        }
        readAllAsync(fd, 1000, this).then(this, [promise](const QByteArray &data) {
            promise->addResult(data);
            promise->finish();
        });
    });
    return promise->future();
}

QFuture<ClipboardPayload> PortalClipboard::snapshot()
{
    if (m_owner)
        return readyFuture(m_payload);
    if (sessionHandle().isEmpty() || !m_session->clipboardEnabled())
        return readyFuture(ClipboardPayload());

    // Типы чужого selection приходят сигналом SelectionOwnerChanged; если его ещё не было,
    // пробуем обычный текст — самый частый случай
    QString textMime, imageMime;
    for (const QString &mime : kTextMimes)
        if (m_selectionMimes.isEmpty() ? mime == kTextMimes.first() : m_selectionMimes.contains(mime)) {
            textMime = mime;
            break;
        }
    for (const QString &mime : kImageMimes)
        if (m_selectionMimes.contains(mime)) {
            imageMime = mime;
            break;
        }

    auto promise = std::make_shared<QPromise<ClipboardPayload>>();
    promise->start();
    auto result = std::make_shared<ClipboardPayload>();
    auto finish = [promise, result] {
        promise->addResult(*result);
        promise->finish();
    };
    auto readImage = [this, imageMime, result, finish] {
        if (imageMime.isEmpty())
            return finish();
        read(imageMime).then(this, [imageMime, result, finish](const QByteArray &data) {
            if (!data.isEmpty())
                result->addImage(data, imageMime.toLatin1());
            finish();
        });
    };
    if (textMime.isEmpty()) {
        readImage();
    } else {
        read(textMime).then(this, [result, readImage](const QByteArray &data) {
            if (!data.isEmpty())
                result->addText(QString::fromUtf8(data));
            readImage();
        });
    }
    return promise->future();
}

} // namespace mp::lx
