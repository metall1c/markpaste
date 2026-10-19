#pragma once

#include "platform/Interfaces.h"

namespace ps::lx {

// Быстрый захват на KDE через org.kde.KWin.ScreenShot2 (ТЗ разд. 9).
//
// Без диалогов и вспышки, каждый монитор отдельно и в физических пикселях
// (native-resolution), так что смешанный масштаб мониторов не портит картинку (CAP-6).
// KWin пускает только «доверенные» программы: в .desktop-файле должно быть
// X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2, а путь в Exec — совпадать
// с исполняемым файлом процесса. Иначе вызов вернёт ошибку NoAuthorized.
class KWinScreenshot : public ICapture
{
    Q_OBJECT
public:
    explicit KWinScreenshot(QObject *parent = nullptr);

    // Интерфейс есть на шине. Права проверяются только при вызове.
    static bool isPresent();

    QFuture<CaptureResult> captureAllScreens() override;
    CaptureCaps capabilities() const override { return CaptureCap::Silent; }
    QString name() const override { return QStringLiteral("KWin ScreenShot2"); }
};

} // namespace ps::lx
