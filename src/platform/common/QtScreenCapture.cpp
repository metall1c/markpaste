#include "QtScreenCapture.h"

#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>

namespace ps {

QFuture<CaptureResult> QtScreenCapture::captureAllScreens()
{
    CaptureResult r;
    for (QScreen *s : QGuiApplication::screens()) {
        ScreenFrame f;
        f.screenName = s->name();
        f.logicalGeometry = s->geometry();
        f.scale = s->devicePixelRatio();
        f.image = s->grabWindow(0).toImage();
        f.image.setDevicePixelRatio(1.0);
        if (f.image.isNull()) {
            r.status = Status::failure(QStringLiteral("QScreen::grabWindow returned nothing for %1").arg(f.screenName));
            continue;
        }
        r.frames.append(f);
    }
    return readyFuture(r);
}

} // namespace ps
