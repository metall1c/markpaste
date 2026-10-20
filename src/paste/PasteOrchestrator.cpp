#include "PasteOrchestrator.h"

#include "TempFiles.h"

#include <QLoggingCategory>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent>

Q_LOGGING_CATEGORY(lcPaste, "markpaste.paste")

namespace mp {

namespace {

// Запекание всех снимков параллельно: 26 кадров 4K последовательно кодируются секундами.
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
        // RFC 2483: строки через CRLF
        p.entries.append({QStringLiteral("text/uri-list"), (uris.join(QStringLiteral("\r\n")) + QStringLiteral("\r\n")).toUtf8()});
        p.entries.append({QStringLiteral("x-special/gnome-copied-files"),
                          (QStringLiteral("copy\n") + uris.join(u'\n')).toUtf8()});
    }
    if (const QString text = buildMessage(set, options.messageTemplate); !text.isEmpty())
        p.addText(text);   // без комментариев — только файлы и картинка
    if (!baked.isEmpty())
        p.addImage(baked.first().data, baked.first().mimeType);   // для программ, которым нужна одна картинка
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

    QtConcurrent::run([set, options] { return bakeSequence(set, options); })
        .then(this, [this, gen](const QList<Item> &items) {
            if (gen == m_generation && m_running)
                runSequence(items);
        });
}

void PasteOrchestrator::runSequence(QList<Item> items)
{
    m_items = std::move(items);
    emit progress(0, int(m_items.size()));

    // Ждём отпускания сочетания: иначе зажатые Shift/Alt превратят наш Ctrl+V в другое
    // сочетание (ТЗ разд. 10). Не дождались — идём по таймауту.
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
    auto openSession = [this](std::function<void()> next) {
        const quint64 gen = m_generation;
        m_injector->begin().then(this, [this, gen, next](const Status &s) {
            if (gen != m_generation || !m_running)
                return;
            if (!s.ok)
                return finish(s);
            m_sessionOpen = true;
            next();
        });
    };
    auto saveClipboard = [this](std::function<void()> next) {
        if (!m_options.restoreClipboard)
            return next();
        const quint64 gen = m_generation;
        m_clipboard->snapshot().then(this, [this, gen, next](const ClipboardPayload &saved) {
            if (gen != m_generation || !m_running)
                return;
            m_saved = saved;
            next();
        });
    };

    // Буфер через портал доступен только внутри сессии ввода: сначала сессия, потом буфер
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
        // События клавиш доходят до композитора асинхронно: сессию закрываем не сразу,
        // иначе можно потерять «V отпущена» (docs/problems.md).
        // Буфер через портал (needsInputSession) возвращаем до закрытия сессии, остальные — после.
        const bool restoreInSession = m_clipboard->needsInputSession();
        after(m_options.timing.afterTextMs, [this, restoreInSession] {
            if (m_sessionOpen && !restoreInSession) {
                m_injector->end();
                m_sessionOpen = false;
            }
            if (m_saved.isEmpty())
                return finish(Status::success());
            after(m_options.timing.restoreAfterMs, [this] {
                const quint64 gen = m_generation;
                m_clipboard->write(m_saved, ClipboardFlag::None).then(this, [this, gen](const Status &) {
                    if (gen == m_generation) {
                        m_saved = {};
                        finish(Status::success());   // закроет сессию, если она ещё открыта
                    }
                });
            });
        });
        return;
    }

    const Item &item = m_items[m_index];
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
    // Посреди серии клавиш не обрываем: иначе Ctrl может остаться зажатым. Шаг после
    // неё увидит m_cancelled. Отложенные колбэки текущего запуска отсекает m_generation.
    if (!m_keysInFlight)
        finish(Status::failure(tr("Cancelled")));
}

void PasteOrchestrator::finish(const Status &status)
{
    if (!m_running)
        return;
    // При ошибке или отмене всё равно возвращаем буфер, если успели его сохранить.
    // Запись уходит раньше закрытия сессии: для портала Clipboard после неё писать уже нельзя.
    if (!status.ok && !m_saved.isEmpty() && m_clipboard)
        m_clipboard->write(m_saved, ClipboardFlag::None);
    if (m_sessionOpen && m_injector) {
        m_injector->end();
        m_sessionOpen = false;
    }
    m_saved = {};
    m_items.clear();   // готовые картинки больше не нужны: не держим их до следующей вставки
    m_running = false;
    m_onReleased = nullptr;
    ++m_generation;
    if (!status.ok)
        qCWarning(lcPaste) << "paste finished with error:" << status.error;
    emit finished(status, m_options.mode);
}

} // namespace mp
