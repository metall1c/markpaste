#include "FallbackCapture.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcCapture, "markpaste.capture")

namespace mp {

FallbackCapture::FallbackCapture(std::unique_ptr<ICapture> primary, std::unique_ptr<ICapture> secondary,
                                 QObject *parent)
    : ICapture(parent), m_primary(std::move(primary)), m_secondary(std::move(secondary))
{
}

QFuture<CaptureResult> FallbackCapture::captureAllScreens()
{
    if (m_primaryFailed || !m_primary)
        return m_secondary->captureAllScreens();
    return m_primary->captureAllScreens()
        .then(this, [this](const CaptureResult &r) -> QFuture<CaptureResult> {
            if (r.status.ok || !m_secondary)
                return readyFuture(r);
            qCWarning(lcCapture) << m_primary->name() << "failed:" << r.status.error << "— switching to"
                                 << m_secondary->name();
            m_primaryFailed = true;
            const bool replaced = r.programReplaced;
            return m_secondary->captureAllScreens().then([replaced](CaptureResult second) {
                second.programReplaced = second.programReplaced || replaced;
                return second;
            });
        })
        .unwrap();
}

CaptureCaps FallbackCapture::capabilities() const
{
    return (m_primaryFailed || !m_primary) ? m_secondary->capabilities() : m_primary->capabilities();
}

QString FallbackCapture::name() const
{
    return (m_primaryFailed || !m_primary) ? m_secondary->name() : m_primary->name();
}

} // namespace mp
