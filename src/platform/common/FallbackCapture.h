#pragma once

#include "platform/Interfaces.h"

#include <memory>

namespace mp {

// A primary capture method with a fallback: if the primary fails (for example, KWin does not consider
// the program trusted), switch to the fallback and never try the primary again.
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

} // namespace mp
