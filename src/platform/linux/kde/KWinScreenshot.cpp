#include "KWinScreenshot.h"

#include "platform/linux/FdIo.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QScreen>

#include <fcntl.h>
#include <unistd.h>

#include <memory>

Q_LOGGING_CATEGORY(lcKWin, "markpaste.kwin")

namespace mp::lx {

static const QString kService = QStringLiteral("org.kde.KWin");
static const QString kPath = QStringLiteral("/org/kde/KWin/ScreenShot2");
static const QString kIface = QStringLiteral("org.kde.KWin.ScreenShot2");

KWinScreenshot::KWinScreenshot(QObject *parent)
    : ICapture(parent)
{
}

bool KWinScreenshot::isPresent()
{
    QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, QStringLiteral("org.freedesktop.DBus.Properties"),
                                                      QStringLiteral("Get"));
    msg << kIface << QStringLiteral("Version");
    const QDBusMessage reply = QDBusConnection::sessionBus().call(msg, QDBus::Block, 1000);
    return reply.type() == QDBusMessage::ReplyMessage;
}

namespace {

struct Pending {
    QPromise<CaptureResult> promise;
    QList<ScreenFrame> frames;
    int left = 0;
    QString error;
};

void finishOne(const std::shared_ptr<Pending> &p)
{
    if (--p->left > 0)
        return;
    CaptureResult r;
    if (!p->error.isEmpty())
        r.status = Status::failure(p->error);
    r.frames = p->frames;
    p->promise.addResult(r);
    p->promise.finish();
}

} // namespace

QFuture<CaptureResult> KWinScreenshot::captureAllScreens()
{
    // QScreen читаем в главном потоке (ТЗ разд. 8).
    const QList<QScreen *> screens = QGuiApplication::screens();
    auto pending = std::make_shared<Pending>();
    pending->promise.start();
    pending->left = int(screens.size());
    pending->frames.resize(screens.size());
    if (screens.isEmpty()) {
        pending->promise.addResult(CaptureResult{Status::failure(QStringLiteral("no screens")), {}});
        pending->promise.finish();
        return pending->promise.future();
    }

    for (int i = 0; i < screens.size(); ++i) {
        QScreen *screen = screens[i];
        ScreenFrame &frame = pending->frames[i];
        frame.screenName = screen->name();
        frame.logicalGeometry = screen->geometry();
        frame.scale = screen->devicePixelRatio();

        int fds[2];
        if (::pipe2(fds, O_CLOEXEC) != 0) {
            pending->error = QStringLiteral("pipe2 failed");
            finishOne(pending);
            continue;
        }
        QDBusMessage msg = QDBusMessage::createMethodCall(kService, kPath, kIface, QStringLiteral("CaptureScreen"));
        const QVariantMap options{
            {QStringLiteral("native-resolution"), true},
            {QStringLiteral("include-cursor"), false},
        };
        msg << screen->name() << options << QVariant::fromValue(QDBusUnixFileDescriptor(fds[1]));
        ::close(fds[1]);   // QDBusUnixFileDescriptor держит свою копию

        // Чтение начинаем сразу: KWin пишет в pipe, и большой кадр не влезет в буфер pipe.
        auto data = std::make_shared<QByteArray>();
        auto meta = std::make_shared<QVariantMap>();
        auto parts = std::make_shared<int>(2);   // ответ D-Bus + данные
        auto assemble = [pending, data, meta, parts, i] {
            if (--*parts > 0)
                return;
            ScreenFrame &f = pending->frames[i];
            if (meta->isEmpty()) {
                finishOne(pending);
                return;
            }
            const int w = meta->value(QStringLiteral("width")).toInt();
            const int h = meta->value(QStringLiteral("height")).toInt();
            const int stride = meta->value(QStringLiteral("stride")).toInt();
            const auto format = QImage::Format(meta->value(QStringLiteral("format")).toUInt());
            if (w <= 0 || h <= 0 || data->size() < qint64(stride) * h) {
                pending->error = QStringLiteral("KWin returned an incomplete frame for %1").arg(f.screenName);
            } else {
                // copy(): QImage поверх чужого буфера не владеет им
                f.image = QImage(reinterpret_cast<const uchar *>(data->constData()), w, h, stride, format).copy();
                const qreal scale = meta->value(QStringLiteral("scale")).toDouble();
                if (scale > 0)
                    f.scale = scale;
            }
            finishOne(pending);
        };

        readAllAsync(fds[0], 5000, this).then(this, [data, assemble](const QByteArray &bytes) {
            *data = bytes;
            assemble();
        });

        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(msg, 5000), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [pending, meta, assemble](QDBusPendingCallWatcher *w) {
            QDBusPendingReply<QVariantMap> reply = *w;
            w->deleteLater();
            if (reply.isError()) {
                pending->error = QStringLiteral("KWin ScreenShot2: %1").arg(reply.error().message());
                qCWarning(lcKWin) << pending->error;
            } else {
                *meta = reply.value();
            }
            assemble();
        });
    }
    return pending->promise.future();
}

} // namespace mp::lx
