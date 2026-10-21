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

// What is restored after a paste: only text and images (spec section 10), as in DataControlClipboard
const QStringList kTextMimes = {QStringLiteral("text/plain;charset=utf-8"), QStringLiteral("text/plain"),
                                QStringLiteral("UTF8_STRING")};
const QStringList kImageMimes = {QStringLiteral("image/png"), QStringLiteral("image/jpeg"), QStringLiteral("image/bmp")};
constexpr int kTextReadMs = 1000;    // as in DataControlClipboard: the owner takes longer to encode an image
constexpr int kImageReadMs = 3000;

// The fd from the D-Bus reply belongs to QDBusUnixFileDescriptor and is closed with it
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
    // The session closed: our selection and what we knew about the foreign one are no longer valid
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
    if (flags & ClipboardFlag::ExcludeFromHistory)   // Klipper does not keep such entries (spec section 10)
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
        // The receiver reads at its own pace: write in the thread pool, not in the main thread
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

QFuture<QByteArray> PortalClipboard::read(const QString &mimeType, int timeoutMs)
{
    auto promise = std::make_shared<QPromise<QByteArray>>();
    promise->start();
    QDBusMessage m = QDBusMessage::createMethodCall(PortalBus::kService, PortalBus::kPath, kIface,
                                                    QStringLiteral("SelectionRead"));
    m << QVariant::fromValue(QDBusObjectPath(sessionHandle())) << mimeType;
    auto *watcher = new QDBusPendingCallWatcher(PortalBus::instance().connection().asyncCall(m), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, promise, timeoutMs](QDBusPendingCallWatcher *w) {
        const QDBusPendingReply<QDBusUnixFileDescriptor> reply = *w;
        w->deleteLater();
        const int fd = reply.isError() ? -1 : takeFd(reply.value());
        if (fd < 0) {
            promise->addResult(QByteArray());
            promise->finish();
            return;
        }
        readAllAsync(fd, timeoutMs, this).then(this, [promise](const QByteArray &data) {
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

    // The types of a foreign selection arrive with the SelectionOwnerChanged signal; if it has not come yet,
    // try plain text, the most common case
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
        read(imageMime, kImageReadMs).then(this, [imageMime, result, finish](const QByteArray &data) {
            if (!data.isEmpty())
                result->addImage(data, imageMime.toLatin1());
            finish();
        });
    };
    if (textMime.isEmpty()) {
        readImage();
    } else {
        read(textMime, kTextReadMs).then(this, [result, readImage](const QByteArray &data) {
            if (!data.isEmpty())
                result->addText(QString::fromUtf8(data));
            readImage();
        });
    }
    return promise->future();
}

} // namespace mp::lx
