#include "PortalClipboard.h"

#include "PortalBus.h"
#include "PortalRemoteDesktop.h"
#include "platform/linux/FdIo.h"

#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QMutexLocker>

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
    : IClipboard(parent), m_session(session), m_server(std::make_unique<PortalSelectionServer>())
{
    session->setRequestClipboard(true);
    m_serverThread.setObjectName(QStringLiteral("PortalSelectionServer"));
    m_server->moveToThread(&m_serverThread);
    m_serverThread.start();
    QDBusConnection bus = PortalBus::instance().connection();
    // QtDBus delivers the signal in the receiver's thread, i.e. in m_serverThread
    bus.connect(PortalBus::kService, PortalBus::kPath, kIface, QStringLiteral("SelectionTransfer"), m_server.get(),
                SLOT(onSelectionTransfer(QDBusObjectPath, QString, uint)));
    bus.connect(PortalBus::kService, PortalBus::kPath, kIface, QStringLiteral("SelectionOwnerChanged"), this,
                SLOT(onSelectionOwnerChanged(QDBusObjectPath, QVariantMap)));
    // The session closed: our selection and what we knew about the foreign one are no longer valid
    connect(session, &PortalRemoteDesktop::sessionClosed, this, [this] {
        m_owner = false;
        m_selectionMimes.clear();
        m_server->clear();
    });
}

PortalClipboard::~PortalClipboard()
{
    PortalBus::instance().connection().disconnect(PortalBus::kService, PortalBus::kPath, kIface,
                                                  QStringLiteral("SelectionTransfer"), m_server.get(),
                                                  SLOT(onSelectionTransfer(QDBusObjectPath, QString, uint)));
    m_serverThread.quit();
    m_serverThread.wait();
    m_server.reset();   // the thread has stopped: safe to delete here
}

QString PortalClipboard::sessionHandle() const
{
    return m_session ? m_session->sessionHandle() : QString();
}

QFuture<Status> PortalClipboard::write(const ClipboardPayload &payload, ClipboardFlags flags)
{
    const QString session = sessionHandle();
    if (session.isEmpty())
        return m_focusClipboard.write(payload, flags);   // "Copy set": needs our window to have focus
    if (!m_session->clipboardEnabled())
        return readyFuture(Status::failure(QStringLiteral("Clipboard portal: access was not granted")));

    m_payload = payload;
    if (flags & ClipboardFlag::ExcludeFromHistory)   // Klipper does not keep such entries (spec section 10)
        m_payload.entries.append({QStringLiteral("x-kde-passwordManagerHint"), QByteArrayLiteral("secret")});
    m_owner = true;
    m_server->setSelection(session, m_payload);
    qCDebug(lcPortal) << "SetSelection" << m_payload.mimeTypes();
    return PortalBus::instance().call(
        kIface, QStringLiteral("SetSelection"),
        {QVariant::fromValue(QDBusObjectPath(session)),
         QVariantMap{{QStringLiteral("mime_types"), m_payload.mimeTypes()}}});
}

void PortalSelectionServer::setSelection(const QString &session, const ClipboardPayload &payload)
{
    QMutexLocker lock(&m_mutex);
    m_session = session;
    m_payload = payload;
}

void PortalSelectionServer::clear()
{
    // The session is kept: a request for a selection that is no longer ours still gets SelectionWriteDone(false)
    QMutexLocker lock(&m_mutex);
    m_payload = {};
}

void PortalSelectionServer::onSelectionTransfer(const QDBusObjectPath &session, const QString &mimeType, uint serial)
{
    // Runs in the server thread and blocks only it: the main thread may be waiting for this very data
    QByteArray data;
    bool have = false;
    {
        QMutexLocker lock(&m_mutex);
        if (m_session.isEmpty() || session.path() != m_session)
            return;
        if (const ClipboardEntry *entry = m_payload.find(mimeType)) {
            data = entry->data;
            have = true;
        }
    }
    QDBusConnection bus = PortalBus::instance().connection();
    bool ok = false;
    QElapsedTimer timer;
    timer.start();
    if (have) {
        QDBusMessage m = QDBusMessage::createMethodCall(PortalBus::kService, PortalBus::kPath, kIface,
                                                        QStringLiteral("SelectionWrite"));
        m << QVariant::fromValue(session) << serial;
        const QDBusReply<QDBusUnixFileDescriptor> reply = bus.call(m, QDBus::Block, 3000);
        const int fd = reply.isValid() ? takeFd(reply.value()) : -1;
        if (fd < 0) {
            qCWarning(lcPortal) << "SelectionWrite failed:" << reply.error().message();
        } else {
            ok = writeAllAndClose(fd, data, 3000);
            if (!ok)
                qCWarning(lcPortal) << "clipboard transfer of" << mimeType << "was not completed";
        }
    }
    qCDebug(lcPortal) << "transfer" << serial << mimeType << (have ? data.size() : -1) << "bytes," << (ok ? "ok" : "FAILED")
                      << "in" << timer.elapsed() << "ms";
    QDBusMessage done = QDBusMessage::createMethodCall(PortalBus::kService, PortalBus::kPath, kIface,
                                                       QStringLiteral("SelectionWriteDone"));
    done << QVariant::fromValue(session) << serial << ok;
    bus.call(done, QDBus::NoBlock);
}

void PortalClipboard::onSelectionOwnerChanged(const QDBusObjectPath &session, const QVariantMap &options)
{
    if (session.path() != sessionHandle())
        return;
    qCDebug(lcPortal) << "SelectionOwnerChanged: ours" << options.value(QStringLiteral("session_is_owner")).toBool()
                      << options.value(QStringLiteral("mime_types")).toStringList();
    if (!options.value(QStringLiteral("session_is_owner")).toBool()) {
        m_owner = false;
        m_server->clear();
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
