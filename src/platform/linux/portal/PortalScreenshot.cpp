#include "PortalScreenshot.h"

#include "PortalBus.h"

#include <QFile>
#include <QGuiApplication>
#include <QImageReader>
#include <QScreen>
#include <QUrl>
#include <QtConcurrent>

namespace mp::lx {

static const QString kIface = QStringLiteral("org.freedesktop.portal.Screenshot");

PortalScreenshot::PortalScreenshot(QObject *parent)
    : ICapture(parent)
{
}

bool PortalScreenshot::isAvailable()
{
    return PortalBus::instance().hasInterface(kIface);
}

QFuture<CaptureResult> PortalScreenshot::captureAllScreens()
{
    struct ScreenInfo {
        QString name;
        QRect geometry;
        qreal dpr;
    };
    QList<ScreenInfo> screens;
    QRect desktop;
    for (QScreen *s : QGuiApplication::screens()) {
        screens.append({s->name(), s->geometry(), s->devicePixelRatio()});
        desktop = desktop.united(s->geometry());
    }

    const QVariantMap options{{QStringLiteral("interactive"), false}, {QStringLiteral("modal"), false}};
    return PortalBus::instance()
        .request(kIface, QStringLiteral("Screenshot"), {QString()}, options, {}, 0)
        .then(this, [screens, desktop](const PortalResponse &r) {
            if (!r.ok())
                return readyFuture(CaptureResult{Status::failure(QStringLiteral("Screenshot: ") + r.describe()), {}});
            const QString path = QUrl(r.results.value(QStringLiteral("uri")).toString()).toLocalFile();
            // Decoding a PNG of the whole desktop takes hundreds of ms, so it goes to the thread pool (spec section 8).
            return QtConcurrent::run([path, screens, desktop] {
                CaptureResult result;
                QImageReader reader(path);
                QImage full = reader.read();
                QFile::remove(path);   // the shot must not stay on disk (NFR-4)
                if (full.isNull()) {
                    result.status = Status::failure(QStringLiteral("cannot read portal screenshot: %1").arg(reader.errorString()));
                    return result;
                }
                const qreal scale = desktop.width() > 0 ? qreal(full.width()) / desktop.width() : 1.0;
                for (const ScreenInfo &s : screens) {
                    ScreenFrame f;
                    f.screenName = s.name;
                    f.logicalGeometry = s.geometry;
                    f.scale = scale;
                    const QRect src(qRound((s.geometry.x() - desktop.x()) * scale),
                                    qRound((s.geometry.y() - desktop.y()) * scale),
                                    qRound(s.geometry.width() * scale), qRound(s.geometry.height() * scale));
                    f.image = full.copy(src);
                    result.frames.append(f);
                }
                return result;
            });
        })
        .unwrap();
}

} // namespace mp::lx
