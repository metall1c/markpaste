#pragma once

#include "platform/Interfaces.h"

#include <memory>

namespace ps {

// Основной способ захвата с запасным: если основной отказал (например, KWin не считает
// программу доверенной), переключаемся на запасной и больше основной не пробуем.
class FallbackCapture : public ICapture
{
    Q_OBJECT
public:
    FallbackCapture(std::unique_ptr<ICapture> primary, std::unique_ptr<ICapture> secondary, QObject *parent = nullptr);

    QFuture<CaptureResult> captureAllScreens() override;
    CaptureCaps capabilities() const override;
    QString name() const override;

private:
    std::unique_ptr<ICapture> m_primary;
    std::unique_ptr<ICapture> m_secondary;
    bool m_primaryFailed = false;
};

} // namespace ps
