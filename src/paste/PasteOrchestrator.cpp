#include "PasteOrchestrator.h"

#include "TempFiles.h"

#include <QLoggingCategory>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent>

Q_LOGGING_CATEGORY(lcPaste, "markpaste.paste")

namespace mp {

namespace {

// Bake all shots in parallel: 26 4K frames take seconds to encode one by one.
QList<EncodedImage> bakeAll(const CaptureSet &set, const PasteOptions &options)
{
    QList<int> indices;
    for (int i = 0; i < set.shots.size(); ++i)
        indices << i;
    return QtConcurrent::blockingMapped(indices, [&set, &options](int i) {
        return encodeForPaste(renderShot(set.shots[i], i, options.style), options.encode);
    });
}

} // namespace

PasteOrchestrator::PasteOrchestrator(IClipboard *clipboard, IInputInjector *injector, QObject *parent)
    : QObject(parent), m_clipboard(clipboard), m_injector(injector)
{
}

bool PasteOrchestrator::canSequential() const
{
    return m_clipboard && m_injector && m_clipboard->canWriteInBackground() && m_injector->isAvailable();
}

QList<PasteOrchestrator::Item> PasteOrchestrator::bakeSequence(const CaptureSet &set, const PasteOptions &options)
{
    QList<Item> images;
    const QList<EncodedImage> baked = bakeAll(set, options);

    if (options.mode == PasteMode::Combined && !baked.isEmpty()) {
        QList<QImage> decoded;
        QList<QString> labels;
        qreal dpr = 1.0;
        for (int i = 0; i < baked.size(); ++i) {
            decoded << QImage::fromData(baked[i].data);
            labels << QString(shotLetter(i));
            dpr = std::max(dpr, set.shots[i].devicePixelRatio);
        }
        const EncodedImage combined = encodeForPaste(composeVertical(decoded, labels, dpr), options.encode);
        images << Item{ClipboardPayload::image(combined.data, combined.mimeType), QStringLiteral("combined")};
    } else {
        for (int i = 0; i < baked.size(); ++i)
            images << Item{ClipboardPayload::image(baked[i].data, baked[i].mimeType), QString(shotLetter(i))};
    }

    QList<Item> items;
    const QString text = buildMessage(set, options.messageTemplate);
    const Item textItem{ClipboardPayload::text(text), QStringLiteral("text")};
    if (options.messageTemplate.textBeforeImages && !text.isEmpty())
        items << textItem;
    items << images;
    if (!options.messageTemplate.textBeforeImages && !text.isEmpty())
        items << textItem;
    return items;
}

ClipboardPayload PasteOrchestrator::filesAndTextPayload(const CaptureSet &set, const PasteOptions &options,
                                                        QStringList *files)
{
    const QList<EncodedImage> baked = bakeAll(set, options);
    QStringList uris;
    QStringList paths;
    for (int i = 0; i < baked.size(); ++i) {
        const QString path = TempFiles::write(QStringLiteral("shot-%1.%2").arg(shotLetter(i)).arg(baked[i].extension),
                                              baked[i].data);
        if (path.isEmpty())
            continue;
        paths << path;
        uris << QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded);
    }
    if (files)
        *files = paths;

    ClipboardPayload p;
    if (!uris.isEmpty()) {
        // RFC 2483: lines separated by CRLF
        p.entries.append({QStringLiteral("text/uri-list"), (uris.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n")).toUtf8()});
        p.entries.append({QStringLiteral("x-special/gnome-copied-files"),
                          (QStringLiteral("copy\n") + uris.join(u'\n')).toUtf8()});
    }
    if (const QString text = buildMessage(set, options.messageTemplate); !text.isEmpty())
        p.addText(text);   // without comments, only files and an image
    if (!baked.isEmpty())
        p.addImage(baked.first().data, baked.first().mimeType);   // for programs that need a single image
    return p;
}

void PasteOrchestrator::after(int ms, std::function<void()> fn)
{
    const quint64 gen = m_generation;
    QTimer::singleShot(std::max(0, ms), this, [this, gen, fn = std::move(fn)] {
        if (gen == m_generation && m_running)
            fn();
    });
}

void PasteOrchestrator::start(const CaptureSet &set, const PasteOptions &options)
{
    if (m_running || !m_clipboard)
        return;
    ++m_generation;
    m_running = true;
    m_cancelled = false;
    m_keysInFlight = false;
    m_sessionOpen = false;
    m_sessionOpening = false;
    m_sessionWaiters.clear();
    m_released = !options.waitHotkeyRelease;
    m_onReleased = nullptr;
    m_options = options;
    m_saved = {};
    m_items.clear();
    m_index = 0;
    const quint64 gen = m_generation;

    if (set.isEmpty()) {
        finish(Status::failure(tr("The set is empty")));
        return;
    }

    if (options.mode == PasteMode::FilesAndText) {
        QtConcurrent::run([set, options] { return filesAndTextPayload(set, options, nullptr); })
            .then(this, [this, gen](const ClipboardPayload &payload) {
                if (gen != m_generation || !m_running)
                    return;
                m_clipboard->write(payload, ClipboardFlag::ExcludeFromHistory).then(this, [this, gen](const Status &s) {
                    if (gen == m_generation)
                        finish(s);
                });
            });
        return;
    }

    if (!canSequential()) {
        finish(Status::failure(tr("Sequential paste is not available here")));
        return;
    }

    // Alt+V is still held: open the session right away without waiting for baking, and while Alt is held,
    // send a "mask" so that the chat window does not take the Alt release for a single press and open its menu
    if (options.maskAltTap && !m_released && m_injector) {
        openSession([this] {
            if (!m_released) {
                qCInfo(lcPaste) << "Alt still held: modifier mask sent";
                m_injector->sendModifierMask();
            } else
                qCInfo(lcPaste) << "Alt released before the session opened: no mask";
        });
    }

    QtConcurrent::run([set, options] { return bakeSequence(set, options); })
        .then(this, [this, gen](const QList<Item> &items) {
            if (gen == m_generation && m_running)
                runSequence(items);
        });
}

void PasteOrchestrator::openSession(std::function<void()> next)
{
    if (m_sessionOpen)
        return next();
    m_sessionWaiters << std::move(next);
    if (m_sessionOpening)
        return;
    m_sessionOpening = true;
    const quint64 gen = m_generation;
    m_injector->begin().then(this, [this, gen](const Status &s) {
        if (gen != m_generation || !m_running) {
            if (s.ok && !m_running && m_injector)
                m_injector->end();   // the paste was cancelled while the session was opening: do not leave it open
            return;
        }
        m_sessionOpening = false;
        if (!s.ok)
            return finish(s);
        m_sessionOpen = true;
        for (auto &fn : std::exchange(m_sessionWaiters, {}))
            fn();
    });
}

void PasteOrchestrator::runSequence(QList<Item> items)
{
    m_items = std::move(items);
    emit progress(0, int(m_items.size()));

    // Wait for the shortcut to be released: otherwise held Shift/Alt would turn our Ctrl+V into another
    // shortcut (spec section 10). If it is not released in time, go on after a timeout.
    auto proceed = [this] {
        auto go = [this] { after(m_options.timing.afterReleaseMs, [this] { step(); }); };
        if (m_released)
            return go();
        m_onReleased = go;
        after(m_options.timing.releaseTimeoutMs, [this] {
            if (!m_released) {
                qCInfo(lcPaste) << "hotkey release not reported, continuing by timeout";
                hotkeyReleased();
            }
        });
    };
    auto openSession = [this](std::function<void()> next) { this->openSession(std::move(next)); };
    auto saveClipboard = [this](std::function<void()> next) {
        if (!m_options.restoreClipboard)
            return next();
        const quint64 gen = m_generation;
        m_clipboard->snapshot().then(this, [this, gen, next](const ClipboardPayload &saved) {
            if (gen != m_generation || !m_running)
                return;
            m_saved = saved;
            qCDebug(lcPaste) << "saved clipboard:" << saved.mimeTypes();
            next();
        });
    };

    // The clipboard via the portal is available only inside an input session: session first, clipboard second
    if (m_clipboard->needsInputSession())
        openSession([=] { saveClipboard(proceed); });
    else
        saveClipboard([=] { openSession(proceed); });
}

void PasteOrchestrator::hotkeyReleased()
{
    m_released = true;
    if (auto fn = std::exchange(m_onReleased, nullptr))
        fn();
}

void PasteOrchestrator::step()
{
    if (m_cancelled)
        return finish(Status::failure(tr("Cancelled")));

    if (m_index >= m_items.size()) {
        // Key events reach the compositor asynchronously: the session is not closed right away,
        // otherwise "V released" may be lost (docs/problems.md).
        // A portal clipboard (needsInputSession) is restored before closing the session, the others after.
        const bool restoreInSession = m_clipboard->needsInputSession();
        after(m_options.timing.afterTextMs, [this, restoreInSession] {
            if (m_sessionOpen && !restoreInSession) {
                m_injector->end();
                m_sessionOpen = false;
            }
            if (m_saved.isEmpty())
                return finish(Status::success());
            after(m_options.timing.restoreAfterMs, [this] {
                qCDebug(lcPaste) << "restoring clipboard:" << m_saved.mimeTypes();
                const quint64 gen = m_generation;
                m_clipboard->write(m_saved, ClipboardFlag::None).then(this, [this, gen](const Status &) {
                    if (gen == m_generation) {
                        m_saved = {};
                        finish(Status::success());   // closes the session if it is still open
                    }
                });
            });
        });
        return;
    }

    const Item &item = m_items[m_index];
    qCDebug(lcPaste) << "step" << m_index << item.what << item.payload.mimeTypes();
    const quint64 gen = m_generation;
    m_clipboard->write(item.payload, ClipboardFlag::ExcludeFromHistory).then(this, [this, gen](const Status &w) {
        if (gen != m_generation || !m_running)
            return;
        if (!w.ok)
            return finish(w);
        after(m_options.timing.settleMs, [this] {
            const quint64 gen2 = m_generation;
            m_keysInFlight = true;
            m_injector->sendPaste().then(this, [this, gen2](const Status &k) {
                if (gen2 != m_generation || !m_running)
                    return;
                m_keysInFlight = false;
                if (!k.ok)
                    return finish(k);
                ++m_index;
                emit progress(m_index, int(m_items.size()));
                if (m_index >= m_items.size())
                    step();
                else
                    after(m_options.timing.afterImageMs, [this] { step(); });
            });
        });
    });
}

void PasteOrchestrator::cancel()
{
    if (!m_running)
        return;
    m_cancelled = true;
    // Do not break off in the middle of a key sequence: Ctrl could stay held. The step after
    // it will see m_cancelled. Delayed callbacks of the current run are cut off by m_generation.
    if (!m_keysInFlight)
        finish(Status::failure(tr("Cancelled")));
}

void PasteOrchestrator::finish(const Status &status)
{
    if (!m_running)
        return;
    // On error or cancel, still restore the clipboard if it was saved.
    // The write goes before the session closes: for the Clipboard portal writing is no longer possible after that.
    if (!status.ok && !m_saved.isEmpty() && m_clipboard)
        m_clipboard->write(m_saved, ClipboardFlag::None);
    if (m_sessionOpen && m_injector) {
        m_injector->end();
        m_sessionOpen = false;
    }
    m_saved = {};
    m_items.clear();   // the baked images are no longer needed: do not keep them until the next paste
    m_running = false;
    m_onReleased = nullptr;
    ++m_generation;
    if (!status.ok)
        qCWarning(lcPaste) << "paste finished with error:" << status.error;
    emit finished(status, m_options.mode);
}

} // namespace mp
