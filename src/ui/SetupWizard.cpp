#include "SetupWizard.h"

#include "Controls.h"
#include "platform/Autostart.h"

#include <QCheckBox>
#include <QElapsedTimer>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
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

// Step number in a circle
class StepBadge : public QWidget
{
public:
    explicit StepBadge(int number) : m_number(QString::number(number)) { setFixedSize(22, 22); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const UiColors c = UiColors::from(palette());
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(c.border, 1));
        p.setBrush(c.raised);
        p.drawEllipse(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
        QFont f = smallerFont(font());
        f.setBold(true);
        p.setFont(f);
        p.setPen(c.text);
        p.drawText(rect(), Qt::AlignCenter, m_number);
    }

private:
    QString m_number;
};

// A wizard step: a card with a number and a title
Card *section(int number, const QString &title, QVBoxLayout *&layout)
{
    auto *card = new Card;
    layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);
    auto *head = new QHBoxLayout;
    head->setSpacing(8);
    head->addWidget(new StepBadge(number));
    auto *label = new QLabel(title);
    QFont f = label->font();
    f.setWeight(QFont::DemiBold);
    label->setFont(f);
    head->addWidget(label, 1);
    layout->addLayout(head);
    return card;
}

ActionButton *wizardButton(const QString &text)
{
    auto *b = new ActionButton(text);
    b->setFocusPolicy(Qt::StrongFocus);
    return b;
}

QLabel *mutedLabel(const QString &text)
{
    QLabel *l = wrapLabel(text);
    setMutedText(l);
    return l;
}

} // namespace

SetupWizard::SetupWizard(Platform *platform, const QString &appId, QWidget *parent)
    : QDialog(parent), m_platform(platform), m_appId(appId)
{
    setWindowTitle(tr("Markpaste setup"));
    useAccentSelection(this);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    auto *title = new QLabel(tr("Checking this system"));
    QFont titleFont = title->font();
    titleFont.setWeight(QFont::DemiBold);
    if (titleFont.pointSizeF() > 0)
        titleFont.setPointSizeF(titleFont.pointSizeF() * 1.35);
    title->setFont(titleFont);
    layout->addWidget(title);
    QString introText = tr("Let's check that capture, hotkeys and paste work on this system.");
    if (platform->nativeWayland)   // on Windows and X11 nobody asks for permissions
        introText += u' ' + tr("On Wayland the desktop may ask for permission — allow it once, and it will not ask again.");
    layout->addWidget(mutedLabel(introText.toHtmlEscaped()));
    layout->addSpacing(2);

    // --- Environment ---
    QVBoxLayout *l = nullptr;
    layout->addWidget(section(1, tr("System"), l));
    l->addWidget(wrapLabel(QStringLiteral("<b>%1</b>").arg(platform->environment.toHtmlEscaped())));
    if (platform->runningUnderXWayland) {
        auto *x = new StatusLine;
        x->setState(StatusLine::State::Failed, tr("The program runs under XWayland: global hotkeys and capture will not work. "
                                                  "Start it without QT_QPA_PLATFORM=xcb."));
        l->addWidget(x);
    }
    for (const QString &note : platform->notes) {
        auto *n = new StatusLine;
        n->setState(StatusLine::State::Warning, note);
        l->addWidget(n);
    }

    // --- Capture ---
    layout->addWidget(section(2, tr("Screen capture"), l));
    l->addWidget(mutedLabel(platform->capture ? tr("Method: %1").arg(platform->capture->name().toHtmlEscaped())
                                              : tr("No capture method available.")));
    auto *captureRow = new QHBoxLayout;
    captureRow->setSpacing(12);
    m_captureButton = wizardButton(tr("Test capture"));
    m_captureButton->setEnabled(platform->capture != nullptr);
    m_captureStatus = new StatusLine;
    // Until the check has run, the result line is hidden, so the button does not stretch into its place
    captureRow->addWidget(m_captureButton, 0, Qt::AlignLeft | Qt::AlignTop);
    captureRow->addWidget(m_captureStatus, 1, Qt::AlignVCenter);
    l->addLayout(captureRow);
    connect(m_captureButton, &QPushButton::clicked, this, &SetupWizard::checkCapture);

    // --- Hotkeys ---
    layout->addWidget(section(3, tr("Hotkeys"), l));
    m_hotkeyStatus = new StatusLine;
    l->addWidget(m_hotkeyStatus);
    // GNOME: Print is taken by its screenshot tool (spec section 9, problems.md 2026-10-21)
    m_desktopKeyStatus = new StatusLine;
    m_desktopKeyStatus->hide();
    l->addWidget(m_desktopKeyStatus);
    m_desktopKeyButton = wizardButton(QString());
    m_desktopKeyButton->hide();
    connect(m_desktopKeyButton, &QPushButton::clicked, this, &SetupWizard::releaseDesktopKeys);
    {
        auto *row = new QHBoxLayout;
        row->addWidget(m_desktopKeyButton);
        row->addStretch();
        l->addLayout(row);
    }
    if (platform->hotkeys && platform->hotkeys->canOpenSystemSettings()) {
        auto *sys = wizardButton(tr("Change in system settings…"));
        connect(sys, &QPushButton::clicked, this, [this] { m_platform->hotkeys->openSystemSettings(); });
        auto *row = new QHBoxLayout;
        row->addWidget(sys);
        row->addStretch();
        l->addLayout(row);
    }

    // --- Paste ---
    layout->addWidget(section(4, tr("Paste into chats"), l));
    QString how = tr("Clipboard: %1").arg(platform->clipboard ? platform->clipboard->name().toHtmlEscaped() : QStringLiteral("—"));
    how += QStringLiteral("<br>") + tr("Ctrl+V emulation: %1")
               .arg(platform->injector && platform->injector->isAvailable() ? platform->injector->name().toHtmlEscaped()
                                                                             : QStringLiteral("—"));
    l->addWidget(mutedLabel(how));
    m_pasteField = new QLineEdit;
    m_pasteField->setPlaceholderText(tr("The test text will be pasted here"));
    l->addWidget(m_pasteField);
    auto *pasteRow = new QHBoxLayout;
    pasteRow->setSpacing(12);
    m_pasteButton = wizardButton(tr("Test paste"));
    m_pasteStatus = new StatusLine;
    pasteRow->addWidget(m_pasteButton, 0, Qt::AlignLeft | Qt::AlignTop);
    pasteRow->addWidget(m_pasteStatus, 1, Qt::AlignVCenter);
    l->addLayout(pasteRow);
    connect(m_pasteButton, &QPushButton::clicked, this, &SetupWizard::checkPaste);

    // --- Autostart ---
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

    // "Settings…" on the left, the primary "Done" on the right
    auto *settings = wizardButton(tr("Settings…"));
    connect(settings, &QPushButton::clicked, this, &SetupWizard::openSettingsRequested);
    auto *done = new ActionButton(tr("Done"), ActionButton::Kind::Primary);
    done->setGlyph(Icon::Check);
    done->setFocusPolicy(Qt::StrongFocus);
    done->setDefault(true);
    connect(done, &QPushButton::clicked, this, &QDialog::accept);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(settings);
    buttons->addStretch();
    buttons->addWidget(done);
    layout->addStretch();
    layout->addSpacing(4);
    layout->addLayout(buttons);

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
    resize(600, sizeHint().height());
}

void SetupWizard::setStatus(StatusLine *label, Result result, const QString &text)
{
    using State = StatusLine::State;
    label->setState(result == Result::Ok        ? State::Ok
                    : result == Result::Warning ? State::Warning
                    : result == Result::Failed  ? State::Failed
                                                : State::Pending,
                    text);
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
                  tr("Global hotkeys are not available here. Assign the commands “markpaste --capture”, "
                     "“markpaste --paste” and “markpaste --capture-to-strip” to keys in the system keyboard settings."));
        return;
    }
    // Capture to strip is an extra hotkey: shown if assigned, but it does not affect the check result
    struct Line {
        Action action;
        QString name;
        bool required;
    };
    const QList<Line> actions{{Action::Capture, tr("Shot with editor"), true},
                              {Action::PasteSet, tr("Paste set"), true},
                              {Action::CaptureToStrip, tr("Shot without editor"), false}};
    QStringList lines;
    bool allPressed = true, anyMissing = false;
    for (const auto &[action, name, required] : actions) {
        const QString trigger = m_triggers.value(action);
        QString state;
        if (!required) {
            if (trigger.isEmpty())
                continue;
            state = m_pressed.value(action) ? tr("%1 — works").arg(trigger) : tr("press %1 to test").arg(trigger);
        } else if (trigger.isEmpty()) {
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
    updateDesktopKeys();
}

void SetupWizard::updateDesktopKeys()
{
    m_heldKeys.clear();
    QStringList holders;
    if (m_platform->hotkeys) {
        for (Action a : {Action::Capture, Action::PasteSet, Action::CaptureToStrip}) {
            const QString trigger = m_triggers.value(a);
            if (trigger.isEmpty() || m_pressed.value(a) || m_heldKeys.contains(trigger))
                continue;   // a key that has fired is ours
            if (!m_keyHolders.contains(trigger))
                m_keyHolders.insert(trigger, m_platform->hotkeys->desktopKeyHolder(trigger));
            const QString holder = m_keyHolders.value(trigger);
            if (holder.isEmpty())
                continue;
            m_heldKeys << trigger;
            holders << tr("%1 is taken by %2, so Markpaste does not get it.").arg(trigger, holder);
        }
    }
    const bool show = !m_heldKeys.isEmpty();
    m_desktopKeyStatus->setVisible(show);
    m_desktopKeyButton->setVisible(show);
    if (!show)
        return;
    m_desktopKeyStatus->setState(StatusLine::State::Warning,
                                 holders.join(u' ') + u' '
                                     + tr("The button frees the key in the system settings; "
                                          "the program settings can give it back."));
    m_desktopKeyButton->setText(tr("Give %1 to Markpaste").arg(m_heldKeys.join(QStringLiteral(", "))));
}

void SetupWizard::releaseDesktopKeys()
{
    for (const QString &key : std::as_const(m_heldKeys)) {
        const Status st = m_platform->hotkeys->releaseDesktopKey(key);
        if (!st.ok) {
            m_desktopKeyStatus->setState(StatusLine::State::Failed, tr("Could not free %1: %2").arg(key, st.error));
            return;
        }
        m_keyHolders.remove(key);   // check again: GNOME applies the change at once
    }
    updateHotkeys();
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

    // The same chain as in mode 1 (PasteOrchestrator): save the clipboard → input session → write →
    // Ctrl+V → wait → close the session → restore the clipboard. For a portal clipboard
    // (needsInputSession) the session opens first, and the clipboard is restored before it closes.
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
        // Focus may have left after the portal dialog: give it back to the field
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
                        // The session cannot be closed right away: the key event reaches
                        // the compositor asynchronously (docs/problems.md)
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
