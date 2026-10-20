#include "SetupWizard.h"

#include "platform/Autostart.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>
#include <memory>

namespace mp {

namespace {

QLabel *wrapLabel(const QString &text = {})
{
    auto *l = new QLabel(text);
    l->setWordWrap(true);
    l->setTextFormat(Qt::RichText);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

QGroupBox *section(const QString &title, QVBoxLayout *&layout)
{
    auto *box = new QGroupBox(title);
    layout = new QVBoxLayout(box);
    layout->setSpacing(6);
    return box;
}

} // namespace

SetupWizard::SetupWizard(Platform *platform, const QString &appId, QWidget *parent)
    : QDialog(parent), m_platform(platform), m_appId(appId)
{
    setWindowTitle(tr("Markpaste setup"));
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    QString introText = tr("Let's check that capture, hotkeys and paste work on this system.");
    if (platform->nativeWayland)   // на Windows и X11 разрешений никто не спрашивает
        introText += u' ' + tr("On Wayland the desktop may ask for permission — allow it once, and it will not ask again.");
    auto *intro = wrapLabel(introText);
    layout->addWidget(intro);

    // --- Окружение ---
    QVBoxLayout *l = nullptr;
    layout->addWidget(section(tr("1. System"), l));
    QString env = QStringLiteral("<b>%1</b>").arg(platform->environment.toHtmlEscaped());
    if (platform->runningUnderXWayland)
        env += QStringLiteral("<br><span style='color:#c62828'>%1</span>")
                   .arg(tr("The program runs under XWayland: global hotkeys and capture will not work. "
                           "Start it without QT_QPA_PLATFORM=xcb.").toHtmlEscaped());
    for (const QString &note : platform->notes)
        env += QStringLiteral("<br>⚠ %1").arg(note.toHtmlEscaped());
    l->addWidget(wrapLabel(env));

    // --- Захват ---
    layout->addWidget(section(tr("2. Screen capture"), l));
    l->addWidget(wrapLabel(platform->capture ? tr("Method: %1").arg(platform->capture->name().toHtmlEscaped())
                                             : tr("No capture method available.")));
    auto *captureRow = new QHBoxLayout;
    m_captureButton = new QPushButton(tr("Test capture"));
    m_captureButton->setEnabled(platform->capture != nullptr);
    m_captureStatus = wrapLabel();
    captureRow->addWidget(m_captureButton);
    captureRow->addWidget(m_captureStatus, 1);
    l->addLayout(captureRow);
    connect(m_captureButton, &QPushButton::clicked, this, &SetupWizard::checkCapture);

    // --- Клавиши ---
    layout->addWidget(section(tr("3. Hotkeys"), l));
    m_hotkeyStatus = wrapLabel();
    l->addWidget(m_hotkeyStatus);
    if (platform->hotkeys && platform->hotkeys->canOpenSystemSettings()) {
        auto *sys = new QPushButton(tr("Change in system settings…"));
        connect(sys, &QPushButton::clicked, this, [this] { m_platform->hotkeys->openSystemSettings(); });
        auto *row = new QHBoxLayout;
        row->addWidget(sys);
        row->addStretch();
        l->addLayout(row);
    }

    // --- Вставка ---
    layout->addWidget(section(tr("4. Paste into chats"), l));
    QString how = tr("Clipboard: %1").arg(platform->clipboard ? platform->clipboard->name().toHtmlEscaped() : QStringLiteral("—"));
    how += QStringLiteral("<br>") + tr("Ctrl+V emulation: %1")
               .arg(platform->injector && platform->injector->isAvailable() ? platform->injector->name().toHtmlEscaped()
                                                                             : QStringLiteral("—"));
    l->addWidget(wrapLabel(how));
    m_pasteField = new QLineEdit;
    m_pasteField->setPlaceholderText(tr("The test text will be pasted here"));
    l->addWidget(m_pasteField);
    auto *pasteRow = new QHBoxLayout;
    m_pasteButton = new QPushButton(tr("Test paste"));
    m_pasteStatus = wrapLabel();
    pasteRow->addWidget(m_pasteButton);
    pasteRow->addWidget(m_pasteStatus, 1);
    l->addLayout(pasteRow);
    connect(m_pasteButton, &QPushButton::clicked, this, &SetupWizard::checkPaste);

    // --- Автозапуск ---
    if (isAutostartSupported()) {
        m_autostart = new QCheckBox(tr("Start Markpaste when I log in"));
        m_autostart->setChecked(isAutostartEnabled(m_appId));
        connect(m_autostart, &QCheckBox::toggled, this, [this](bool on) {
            if (!setAutostartEnabled(m_appId, on)) {
                const QSignalBlocker block(m_autostart);
                m_autostart->setChecked(!on);
            }
        });
        layout->addWidget(m_autostart);
    }

    auto *buttons = new QDialogButtonBox;
    auto *settings = buttons->addButton(tr("Settings…"), QDialogButtonBox::ActionRole);
    connect(settings, &QPushButton::clicked, this, &SetupWizard::openSettingsRequested);
    auto *done = buttons->addButton(tr("Done"), QDialogButtonBox::AcceptRole);
    done->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addStretch();
    layout->addWidget(buttons);

    updateHotkeys();
    if (!platform->capture)
        setStatus(m_captureStatus, Result::Failed, tr("Capture is not possible in this environment."));
    if (!platform->clipboard || !platform->injector || !platform->injector->isAvailable()
        || !platform->clipboard->canWriteInBackground()) {
        m_pasteButton->setEnabled(false);
        m_pasteField->setEnabled(false);
        setStatus(m_pasteStatus, Result::Warning,
                  tr("Automatic paste is not available here. The “Paste set” button will copy the set "
                     "(files + text) — then press Ctrl+V in the chat yourself."));
    }
    resize(560, sizeHint().height());
}

void SetupWizard::setStatus(QLabel *label, Result result, const QString &text)
{
    static const QHash<Result, std::pair<QString, QString>> look{
        {Result::Pending, {QStringLiteral("…"), QStringLiteral("palette(text)")}},
        {Result::Ok, {QStringLiteral("✓"), QStringLiteral("#2e7d32")}},
        {Result::Warning, {QStringLiteral("⚠"), QStringLiteral("#b26a00")}},
        {Result::Failed, {QStringLiteral("✗"), QStringLiteral("#c62828")}},
    };
    const auto &[mark, color] = look.value(result);
    label->setText(QStringLiteral("<span style='color:%1'><b>%2</b> %3</span>").arg(color, mark, text.toHtmlEscaped()));
    if (label == m_captureStatus)
        m_captureResult = result;
    else if (label == m_hotkeyStatus)
        m_hotkeyResult = result;
    else if (label == m_pasteStatus)
        m_pasteResult = result;
}

void SetupWizard::checkCapture()
{
    if (!m_platform->capture)
        return;
    m_captureButton->setEnabled(false);
    setStatus(m_captureStatus, Result::Pending, tr("Capturing…"));
    auto timer = std::make_shared<QElapsedTimer>();
    timer->start();
    const bool silent = m_platform->capture->capabilities().testFlag(CaptureCap::Silent);
    m_platform->capture->captureAllScreens().then(this, [this, timer, silent](const CaptureResult &r) {
        m_captureButton->setEnabled(true);
        if (!r.status.ok || r.frames.isEmpty()) {
            setStatus(m_captureStatus, Result::Failed,
                      tr("Capture failed: %1").arg(r.status.error.isEmpty() ? tr("no image") : r.status.error));
            return;
        }
        QStringList sizes;
        for (const ScreenFrame &f : r.frames)
            sizes << QStringLiteral("%1×%2").arg(f.image.width()).arg(f.image.height());
        QString text = tr("%n screen(s): %1 · %2 ms", nullptr, int(r.frames.size()))
                           .arg(sizes.join(QStringLiteral(", ")))
                           .arg(timer->elapsed());
        if (!silent)
            text += QStringLiteral(" · ") + tr("the desktop may show its own dialog on each capture");
        setStatus(m_captureStatus, silent ? Result::Ok : Result::Warning, text);
    });
}

void SetupWizard::setTriggers(const QHash<Action, QString> &triggers)
{
    m_triggers = triggers;
    updateHotkeys();
}

void SetupWizard::hotkeyActivated(Action action)
{
    m_pressed[action] = true;
    updateHotkeys();
}

void SetupWizard::updateHotkeys()
{
    if (!m_platform->hotkeys) {
        setStatus(m_hotkeyStatus, Result::Failed,
                  tr("Global hotkeys are not available here. Assign the commands “markpaste --capture” and "
                     "“markpaste --paste” to keys in the system keyboard settings."));
        return;
    }
    const QList<std::pair<Action, QString>> actions{{Action::Capture, tr("Capture")}, {Action::PasteSet, tr("Paste set")}};
    QStringList lines;
    bool allPressed = true, anyMissing = false;
    for (const auto &[action, name] : actions) {
        const QString trigger = m_triggers.value(action);
        QString state;
        if (trigger.isEmpty()) {
            anyMissing = true;
            allPressed = false;
            state = tr("not assigned");
        } else if (m_pressed.value(action)) {
            state = tr("%1 — works").arg(trigger);
        } else {
            allPressed = false;
            state = tr("press %1 to test").arg(trigger);
        }
        lines << QStringLiteral("%1: %2").arg(name, state);
    }
    const Result r = anyMissing ? Result::Warning : allPressed ? Result::Ok : Result::Pending;
    setStatus(m_hotkeyStatus, r, lines.join(QStringLiteral(" · ")));
}

void SetupWizard::checkPaste()
{
    IClipboard *clipboard = m_platform->clipboard.get();
    IInputInjector *injector = m_platform->injector.get();
    if (m_pasteRunning || !clipboard || !injector)
        return;
    m_pasteRunning = true;
    m_pasteButton->setEnabled(false);
    m_pasteField->clear();
    setStatus(m_pasteStatus, Result::Pending, tr("Pasting… do not switch windows"));
    const QString expected = tr("Markpaste test %1").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")));

    // Цепочка как в режиме 1 (PasteOrchestrator): сохранить буфер → сессия ввода → записать →
    // Ctrl+V → подождать → закрыть сессию → вернуть буфер. Для буфера через портал
    // (needsInputSession) сессия открывается первой, а буфер возвращается до её закрытия.
    const bool inSession = clipboard->needsInputSession();
    auto saved = std::make_shared<ClipboardPayload>();
    auto restoreAndEnd = [=] {
        if (!saved->isEmpty() && inSession)
            clipboard->write(*saved, ClipboardFlag::None);
        injector->end();
        if (!saved->isEmpty() && !inSession)
            clipboard->write(*saved, ClipboardFlag::None);
    };
    auto paste = [=, this] {
        // После диалога портала фокус мог уйти: возвращаем его полю
        raise();
        activateWindow();
        m_pasteField->setFocus();
        clipboard->write(ClipboardPayload::text(expected), ClipboardFlag::ExcludeFromHistory)
            .then(this, [=, this](const Status &written) {
                if (!written) {
                    restoreAndEnd();
                    finishPaste(written, expected);
                    return;
                }
                QTimer::singleShot(80, this, [=, this] {
                    injector->sendPaste().then(this, [=, this](const Status &sent) {
                        // Закрывать сессию сразу нельзя: событие клавиши доходит до
                        // композитора асинхронно (docs/problems.md)
                        QTimer::singleShot(m_pasteWaitMs, this, [=, this] {
                            restoreAndEnd();
                            finishPaste(sent, expected);
                        });
                    });
                });
            });
    };
    auto save = [=, this](std::function<void()> next) {
        clipboard->snapshot().then(this, [saved, next](const ClipboardPayload &p) {
            *saved = p;
            next();
        });
    };
    auto begin = [=, this](std::function<void()> next) {
        injector->begin().then(this, [=, this](const Status &begun) {
            if (!begun) {
                finishPaste(begun, expected);
                return;
            }
            next();
        });
    };
    if (inSession)
        begin([=] { save(paste); });
    else
        save([=] { begin(paste); });
}

void SetupWizard::finishPaste(const Status &status, const QString &expected)
{
    m_pasteRunning = false;
    m_pasteButton->setEnabled(true);
    if (!status.ok)
        setStatus(m_pasteStatus, Result::Failed, tr("Paste failed: %1").arg(status.error));
    else if (m_pasteField->text().contains(expected))
        setStatus(m_pasteStatus, Result::Ok, tr("Ctrl+V reached the window — pasting a set will work."));
    else
        setStatus(m_pasteStatus, Result::Failed,
                  tr("Ctrl+V was sent, but the text did not appear. Click into the field and try again; "
                     "if it still fails, use the “files + text” paste mode."));
}

} // namespace mp
