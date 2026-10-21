#include "AppController.h"

#include "AppIcon.h"
#include "SettingsDialog.h"
#include "UpdateChecker.h"
#include "core/SetStorage.h"
#include "paste/TempFiles.h"
#include "platform/Autostart.h"
#include "platform/WindowRoles.h"
#include "platform/Memory.h"
#include "ui/AboutDialog.h"
#include "ui/CaptureOverlay.h"
#include "ui/MoveGhost.h"
#include "ui/PreviewDialog.h"
#include "ui/SetupWizard.h"
#include "ui/ShotEditor.h"
#include "ui/StripWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QBuffer>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QMenu>
#include <QScreen>
#include <QMimeData>
#include <QSettings>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent>

Q_LOGGING_CATEGORY(lcApp, "markpaste.app")

namespace mp {

// "Already shown" flags. They used to live in the "general" group: QSettings writes it to INI as
// [%General] and on the next start reads it back as "General", so the lowercase key was not
// found and the wizard opened every time (docs/problems.md). Old values are honored too.
static bool onceFlag(const QSettings &s, const QString &name)
{
    return s.value(QStringLiteral("once/") + name).toBool() || s.value(QStringLiteral("General/") + name).toBool();
}

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<mp::Status>();
    qRegisterMetaType<mp::Action>();
    qRegisterMetaType<mp::ActivationContext>();
}

AppController::~AppController()
{
    if (m_saveTimer && m_saveTimer->isActive())
        saveSession();   // the latest edits, before quitting
    delete m_preview;
    delete m_editor;
    delete m_strip;
    TempFiles::clear();   // temporary PNGs are removed on exit (spec section 10)
}

void AppController::start()
{
    m_settings = Settings::load();
    m_platform = createPlatform(QStringLiteral(MARKPASTE_APP_ID));
    disableWindowAnimations();   // a window hidden before a capture must not "fade out" in the frame

    m_paste = new PasteOrchestrator(m_platform.clipboard.get(), m_platform.injector.get(), this);
    connect(m_paste, &PasteOrchestrator::progress, this, [this](int done, int total) {
        if (m_strip)
            m_strip->setProgress(done, total);
    });
    connect(m_paste, &PasteOrchestrator::finished, this, [this](const Status &s, PasteMode mode) {
        scheduleMemoryRelease();
        if (m_strip)
            m_strip->setProgress(1, 1);
        if (!s.ok) {
            notify(tr("Paste failed: %1").arg(s.error));
            return;
        }
        m_setDelivered = true;
        if (mode == PasteMode::FilesAndText) {
            notify(tr("Set copied — press Ctrl+V in the chat"));
            return;
        }
        if (m_settings.clearAfterPaste)
            clearSet();
    });

    m_strip = new StripWindow(&m_model);
    m_strip->setStyle(m_settings.renderStyle());
    m_strip->setDragProvider([this](const QList<QUuid> &shots) { return dragData(shots); });
    connect(m_strip, &StripWindow::shotActivated, this, [this](const QUuid &id) { openEditor(id, false); });
    connect(m_strip, &StripWindow::newShotRequested, this, &AppController::capture);
    connect(m_strip, &StripWindow::pasteRequested, this, [this] { paste(false); });
    connect(m_strip, &StripWindow::copyRequested, this, &AppController::copySet);
    connect(m_strip, &StripWindow::clearRequested, this, &AppController::clearSet);
    connect(m_strip, &StripWindow::cancelPasteRequested, m_paste, &PasteOrchestrator::cancel);
    connect(m_strip, &StripWindow::settingsRequested, this, &AppController::showSettings);
    connect(m_strip, &StripWindow::previewRequested, this, &AppController::showPreview);
    connect(m_strip, &StripWindow::saveToFolderRequested, this, &AppController::saveSetToFolder);
    connect(m_strip, &StripWindow::aboutRequested, this, &AppController::showAbout);
    connect(m_strip, &StripWindow::hideRequested, this, [this] {
        m_strip->hide();
        if (!m_tray)
            return;
        // Tell once where the strip went
        QSettings s;
        if (!onceFlag(s, QStringLiteral("hideHintShown"))) {
            s.setValue(QStringLiteral("once/hideHintShown"), true);
            m_tray->showMessage(QStringLiteral("Markpaste"), tr("The strip is in the tray. Click the tray icon to show it."),
                                appIcon(), 5000);
        }
    });
    updatePasteCaption();

    m_editor = new ShotEditor(&m_model);
    m_editor->setStyle(m_settings.renderStyle());
    m_editor->setPen(m_settings.markPen);
    m_editor->setWindowIcon(appIcon());
    connect(m_editor, &ShotEditor::penChanged, this, [this](const MarkPen &pen) {
        m_settings.markPen = pen;
        m_settings.save();
    });
    connect(m_editor, &ShotEditor::newShotRequested, this, &AppController::capture);
    connect(m_editor, &ShotEditor::doneRequested, m_editor, &QWidget::hide);
    connect(m_model.undoStack(), &QUndoStack::indexChanged, this, [this] {
        if (m_model.set().isEmpty())
            scheduleMemoryRelease();   // the set was cleared, the frames are no longer needed
    });

    connect(&m_model, &CaptureSetModel::changed, this, [this] {
        m_setDelivered = false;   // the set changed after pasting: this is new work, do not clear it
        if (m_strip && m_editor)
            m_strip->setCurrentShot(m_editor->isVisible() ? m_editor->shotId() : QUuid());
        // The strip is visible only while the set has shots; an empty set means the program sits in the tray
        // (a setting turns this off). Without a tray (GNOME without the extension) the strip is not hidden:
        // otherwise the program cannot be found (APP-1).
        if (m_settings.keepSetOnRestart && m_saveTimer)
            m_saveTimer->start();
        if (m_strip && m_tray && m_settings.hideStripWhenEmpty && m_model.set().isEmpty() && !m_capturing)
            m_strip->hide();
    });

    if (m_platform.hotkeys) {
        connect(m_platform.hotkeys.get(), &IHotkeys::activated, this, [this](Action a, const ActivationContext &) {
            qCInfo(lcApp) << "hotkey activated" << int(a);
            if (a == Action::PasteSet && m_paste->isRunning()) {
                m_paste->hotkeyPressed();   // a chord change while the keys are held (the Ctrl mask)
                return;
            }
            // While the wizard is open, a hotkey is only tested, it does not trigger the action
            if (m_wizard && m_wizard->isVisible()) {
                m_wizard->hotkeyActivated(a);
                return;
            }
            switch (a) {
            case Action::Capture: capture(); break;
            case Action::PasteSet: paste(true); break;
            case Action::NewShot: capture(); break;
            case Action::ClearSet: clearSet(); break;
            case Action::CaptureToStrip: captureToStrip(); break;
            }
        });
        connect(m_platform.hotkeys.get(), &IHotkeys::triggerChanged, this, [this](Action a, const QString &trigger) {
            m_boundTriggers.insert(a, trigger);
            if (m_wizard)
                m_wizard->setTriggers(m_boundTriggers);
            updatePasteCaption();   // shortcuts on the paste button and in the empty strip hint
        });
        connect(m_platform.hotkeys.get(), &IHotkeys::released, this, [this](Action a) {
            if (a == Action::PasteSet) {
                qCInfo(lcApp) << "hotkey released" << int(a);
                m_paste->hotkeyReleased();
            }
        });
        bindHotkeys();
    }

    setupTray();

    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(1000);
    connect(m_saveTimer, &QTimer::timeout, this, &AppController::saveSession);
    restoreSession();

    m_updates = new UpdateChecker(QStringLiteral(MARKPASTE_VERSION), QUrl(QStringLiteral(MARKPASTE_UPDATE_URL)), this);
    connect(m_updates, &UpdateChecker::updateAvailable, this, [this](const QString &version, const QUrl &page) {
        const QUrl url = page.isValid() && !page.isEmpty() ? page : QUrl(QStringLiteral(MARKPASTE_RELEASES_URL));
        notify(tr("Markpaste %1 is available. Update it with your package manager.").arg(version));
        // Clicking this notification opens the release page. Only this one: any later
        // notification resets the address (notify), otherwise a click on "Paste failed" would open the page.
        if (m_tray && !(m_strip && m_strip->isVisible()))
            m_trayClickUrl = url;
    });
    m_updates->setEnabled(m_settings.checkUpdates);

    if (m_platform.runningUnderXWayland)
        notify(tr("Markpaste runs under XWayland: global hotkeys and capture will not work."));
}

void AppController::bindHotkeys()
{
    // All shortcuts in one request: on Wayland the portal shows a single dialog.
    const QHash<Action, QString> wanted{
        {Action::Capture, m_settings.captureHotkey},
        {Action::PasteSet, m_settings.pasteHotkey},
        {Action::CaptureToStrip, m_settings.captureToStripHotkey},
    };
    m_platform.hotkeys->bindAll(wanted).then(this, [this](const QHash<Action, BindResult> &result) {
        for (auto it = result.cbegin(); it != result.cend(); ++it) {
            qCInfo(lcApp) << "hotkey" << int(it.key()) << "->"
                          << (it->status.ok ? (it->trigger.isEmpty() ? QStringLiteral("<not assigned>") : it->trigger)
                                            : it->status.error);
            m_boundTriggers.insert(it.key(), it->status.ok ? it->trigger : QString());
        }
        if (m_wizard)
            m_wizard->setTriggers(m_boundTriggers);
        updatePasteCaption();
    });
}

void AppController::handleCommand(const QString &command)
{
    qCInfo(lcApp) << "command:" << command;
    if (command == u"capture")
        capture();
    else if (command == u"capture-to-strip")
        captureToStrip();
    else if (command == u"paste")
        paste(false);
    else if (command == u"copy")
        copySet();
    else if (command == u"clear")
        clearSet();
    else if (command == u"settings")
        showSettings();
    else if (command == u"setup")
        showSetupWizard();
    else if (command == u"about")
        showAbout();
    else if (command == u"quit")
        QApplication::quit();
    else if (command.startsWith(u"debug-ghost ")) {
        // Socket only: debug-ghost <x> <y> <w> <h> shows the move outline for 2 s
        const QStringList a = command.split(u' ');
        if (a.size() == 5) {
            auto *ghost = new MoveGhost(this);
            ghost->show(QRect(a[1].toInt(), a[2].toInt(), a[3].toInt(), a[4].toInt()), m_settings.accent);
            QTimer::singleShot(2000, ghost, [ghost] {
                ghost->hide();
                ghost->deleteLater();
            });
        }
    } else if (command.startsWith(u"debug-move-strip ")) {
        // Socket only, for testing strip moves: debug-move-strip <screen> <left|right> <offset>
        const QStringList a = command.split(u' ');
        for (QScreen *scr : QGuiApplication::screens())
            if (a.size() == 4 && scr->name() == a[1])
                m_strip->moveTo(scr, PanelPlacement{a[2] == u"left" ? PanelEdge::Left : PanelEdge::Right, a[3].toInt(), 12});
    }
    else   // show, and a repeated launch from the menu (APP-1)
        showStrip();
}

void AppController::handleStartup(const QString &command)
{
    // Normal start (autostart included): with an empty set only the tray icon shows,
    // if the strip hides when the set is empty
    QSettings s;
    if (command == u"show" && !onceFlag(s, QStringLiteral("setupShown"))) {
        // First start: the check wizard instead of a notification (APP-4)
        s.setValue(QStringLiteral("once/setupShown"), true);
        s.setValue(QStringLiteral("once/welcomeShown"), true);
        showSetupWizard();
        return;
    }
    if (command == u"show" && m_tray && m_settings.hideStripWhenEmpty && m_model.set().isEmpty()) {
        if (!onceFlag(s, QStringLiteral("welcomeShown"))) {
            s.setValue(QStringLiteral("once/welcomeShown"), true);
            const QString key = m_boundTriggers.value(Action::Capture, m_settings.captureHotkey);
            m_tray->showMessage(QStringLiteral("Markpaste"),
                                tr("Markpaste is in the tray. Press %1 to capture an area.").arg(key), appIcon(), 6000);
        }
        return;
    }
    handleCommand(command);
}

// --- Capture ---

void AppController::hideOwnWindows()
{
    m_editorWasVisible = m_editor && m_editor->isVisible();
    m_stripWasVisible = m_strip && m_strip->isVisible();
    if (m_editorWasVisible)
        m_editor->hide();
    if (m_stripWasVisible)
        m_strip->hideForCapture();
}

void AppController::restoreOwnWindows()
{
    if (m_stripWasVisible)
        m_strip->showAfterCapture();
    if (m_editorWasVisible)
        m_editor->show();
}

void AppController::capture()
{
    startCapture(false);
}

void AppController::captureToStrip()
{
    startCapture(true);
}

void AppController::startCapture(bool toStrip)
{
    if (m_capturing || m_paste->isRunning())
        return;
    if (!m_platform.capture) {
        notify(tr("Screen capture is not available on this system"));
        return;
    }
    // A new set only after pasting: shots captured straight to the strip pile up one after another,
    // and an "editor is closed" check would clear the set on each of them (ADR 0022)
    if (m_settings.clearOnNextCapture && m_setDelivered && !m_model.set().isEmpty() && m_editor && !m_editor->isVisible())
        m_model.clear();
    if (m_model.set().isFull()) {   // CAP-8
        showStrip();
        m_strip->showMessage(tr("The set is full (26 shots)"));
        return;
    }
    m_capturing = true;

    // Our own windows must not get into the shot. The compositor needs time to remove them.
    const bool hadWindows = (m_editor && m_editor->isVisible()) || (m_strip && m_strip->isVisible());
    hideOwnWindows();
    auto timer = std::make_shared<QElapsedTimer>();
    timer->start();
    QTimer::singleShot(hadWindows ? 180 : 0, this, [this, timer, toStrip] {
        m_platform.capture->captureAllScreens().then(this, [this, timer, toStrip](const CaptureResult &r) {
            qCInfo(lcApp) << "captured" << r.frames.size() << "screens via" << m_platform.capture->name() << "in"
                          << timer->elapsed() << "ms";
            if (r.programReplaced)
                notify(tr("Markpaste was updated while running. Restart it to capture without delays and dialogs."));
            if (!r.status.ok && r.frames.isEmpty()) {
                m_capturing = false;
                restoreOwnWindows();
                notify(tr("Capture failed: %1").arg(r.status.error));
                return;
            }
            m_capture = new CaptureSession(r.frames, m_settings.quickCapture, this);
            connect(m_capture, &CaptureSession::selected, this, [this, toStrip](const CaptureSelection &sel) {
                m_capturing = false;
                m_capture->deleteLater();
                onCaptured(sel.image, sel.devicePixelRatio, toStrip);
            });
            connect(m_capture, &CaptureSession::cancelled, this, [this] {
                m_capturing = false;
                m_capture->deleteLater();
                restoreOwnWindows();
            });
            m_capture->start();
            qCInfo(lcApp) << "overlay shown" << timer->elapsed() << "ms after request";   // NFR-1
        });
    });
}

void AppController::onCaptured(const QImage &image, qreal dpr, bool toStrip)
{
    // PNG in memory, so that 26 4K frames fit into NFR-2. Encoding runs in the thread pool.
    QtConcurrent::run([image] {
        QByteArray png;
        QBuffer buf(&png);
        buf.open(QIODevice::WriteOnly);
        image.save(&buf, "PNG", 50);   // 50 balances speed and size; the compression is lossless
        return png;
    }).then(this, [this, size = image.size(), dpr, toStrip](const QByteArray &png) {
        Shot shot;
        shot.sourcePng = png;
        shot.sourceSize = size;
        shot.devicePixelRatio = dpr;
        const QUuid id = m_model.addShot(shot);
        if (id.isNull()) {
            restoreOwnWindows();
            return;
        }
        showStrip();
        if (toStrip) {
            // Shot goes to the strip only; an editor open before the capture returns to its shot
            if (m_editorWasVisible)
                m_editor->show();
            return;
        }
        openEditor(id, true);
    });
}

void AppController::openEditor(const QUuid &shotId, bool fresh)
{
    m_editor->showShot(shotId, fresh);
    m_strip->setCurrentShot(shotId);
}

// --- Paste ---

void AppController::updatePasteCaption()
{
    if (!m_strip)
        return;
    const bool sequential = m_paste->canSequential();
    // On Wayland the paste button works in mode 1 only if the strip does not take focus
    // (layer-shell); otherwise it copies the set (spec section 10).
    const bool buttonCanSequence = sequential && (!m_platform.nativeWayland || m_platform.supportsLayerShell);
    m_strip->setPasteMode(buttonCanSequence, shortcutLabel(m_boundTriggers.value(Action::PasteSet)));
    m_strip->setCaptureHotkey(shortcutLabel(m_boundTriggers.value(Action::Capture)));
}

void AppController::paste(bool fromHotkey)
{
    if (m_paste->isRunning() || m_model.set().isEmpty())
        return;

    PasteOptions opt = m_settings.pasteOptions();
    opt.waitHotkeyRelease = fromHotkey;
    if (fromHotkey) {
        // Actual shortcuts: KDE reports all of them at once ("Ctrl+Alt+V, Alt+V") and does not say
        // which one was pressed. The mask is needed if at least one has Alt without Ctrl; an extra one
        // (with Ctrl+Alt+V) is harmless: pressing Ctrl while Ctrl is already held
        const QString triggers = m_boundTriggers.value(Action::PasteSet, m_settings.pasteHotkey);
        for (const QString &trigger : triggers.split(u',', Qt::SkipEmptyParts)) {
            const QStringList keys = trigger.split(u'+', Qt::SkipEmptyParts);
            auto has = [&keys](QStringView k) {
                return std::any_of(keys.cbegin(), keys.cend(),
                                   [k](const QString &x) { return x.trimmed().compare(k, Qt::CaseInsensitive) == 0; });
            };
            if (has(u"Alt") && !has(u"Ctrl") && !has(u"Control") && !has(u"Primary"))
                opt.maskAltTap = true;
        }
    }
    const bool buttonCanSequence = m_paste->canSequential() && (!m_platform.nativeWayland || m_platform.supportsLayerShell);
    if (opt.mode != PasteMode::FilesAndText && (!m_paste->canSequential() || (!fromHotkey && !buttonCanSequence)))
        opt.mode = PasteMode::FilesAndText;   // the fallback mode is a full one (spec section 9)

    // Another program window is active (settings, preview, wizard): Ctrl+V would go to it rather than
    // to the chat, and the strip would say "Pasted". The editor does not count, it is hidden below; the strip
    // does not take focus (docs/problems.md, 2026-10-20)
    if (opt.mode != PasteMode::FilesAndText) {
        QWidget *active = QApplication::activeWindow();
        if (active && active != m_editor && active != m_strip) {
            notify(tr("Click into the chat first: a Markpaste window is active"));
            return;
        }
    }

    if (m_editor && m_editor->isVisible()) {
        m_editor->commitPendingEdits();   // the last comment goes into the message too
        m_editor->hide();   // otherwise focus may stay in the editor instead of the chat
    }
    m_paste->start(m_model.set(), opt);
}

void AppController::copySet()
{
    if (m_paste->isRunning() || m_model.set().isEmpty())
        return;
    if (m_editor && m_editor->isVisible())
        m_editor->commitPendingEdits();
    PasteOptions opt = m_settings.pasteOptions();
    opt.mode = PasteMode::FilesAndText;
    m_paste->start(m_model.set(), opt);
}

void AppController::clearSet()
{
    m_model.clear();
    TempFiles::clear();
    if (m_editor)
        m_editor->hide();
}

QMimeData *AppController::dragData(const QList<QUuid> &shots)
{
    CaptureSet subset;
    for (const QUuid &id : shots)
        if (const Shot *s = m_model.shot(id))
            subset.shots << *s;
    if (subset.isEmpty())
        return nullptr;
    // Letters in file names are the same as in the full set
    QStringList files;
    PasteOptions opt = m_settings.pasteOptions();
    QList<QUrl> urls;
    for (const QUuid &id : shots) {
        const int index = m_model.set().shotIndex(id);
        if (index < 0)
            continue;   // the shot was deleted meanwhile
        const QImage img = renderShot(m_model.set().shots[index], index, opt.style);
        const EncodedImage enc = encodeForPaste(img, opt.encode);
        const QString path = TempFiles::write(QStringLiteral("shot-%1.%2").arg(shotLetter(index)).arg(enc.extension), enc.data);
        if (!path.isEmpty())
            urls << QUrl::fromLocalFile(path);
    }
    scheduleMemoryRelease();
    auto *md = new QMimeData;
    md->setUrls(urls);
    // Mode 3: files by drag and drop, the text goes to the clipboard, pasted as a second action.
    // Without comments there is no text: leave the clipboard alone and say nothing about text
    if (const QString text = buildMessage(m_model.set(), opt.messageTemplate); shots.size() > 1 && !text.isEmpty()) {
        m_platform.clipboard->write(ClipboardPayload::text(text), ClipboardFlag::ExcludeFromHistory);
        m_strip->showMessage(tr("Text copied — paste it after dropping the files"), 6000);
    }
    return md;
}

// --- Windows, tray, notifications ---

void AppController::showStrip()
{
    m_strip->present();
}

void AppController::showSettings()
{
    SettingsDialog dlg(m_settings, m_platform, m_boundTriggers);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const Settings old = m_settings;
    m_settings = dlg.settings();
    m_settings.save();
    const QString appId = QStringLiteral(MARKPASTE_APP_ID);
    if (isAutostartSupported() && dlg.autostart() != isAutostartEnabled(appId)
        && !setAutostartEnabled(appId, dlg.autostart()))
        notify(tr("Could not change autostart: %1").arg(autostartFilePath(appId)));
    m_strip->setStyle(m_settings.renderStyle());
    m_editor->setStyle(m_settings.renderStyle());
    if (old.keepSetOnRestart != m_settings.keepSetOnRestart) {
        if (m_settings.keepSetOnRestart)
            saveSession();
        else {
            m_saveTimer->stop();
            SetStorage::removeSession(sessionDirectory());
        }
    }
    if (old.uiLanguage != m_settings.uiLanguage)
        notify(tr("The interface language changes after restart"));
    if (old.checkUpdates != m_settings.checkUpdates)
        m_updates->setEnabled(m_settings.checkUpdates, 5000);
    if (old.hideStripWhenEmpty != m_settings.hideStripWhenEmpty && m_tray && m_model.set().isEmpty()) {
        if (m_settings.hideStripWhenEmpty)
            m_strip->hide();
        else
            showStrip();
    }
    if (old.captureHotkey != m_settings.captureHotkey || old.pasteHotkey != m_settings.pasteHotkey
        || old.captureToStripHotkey != m_settings.captureToStripHotkey)
        notify(tr("New hotkeys take effect after restart"));
}

void AppController::showSetupWizard()
{
    if (!m_wizard) {
        m_wizard = new SetupWizard(&m_platform, QStringLiteral(MARKPASTE_APP_ID));
        m_wizard->setAttribute(Qt::WA_DeleteOnClose);
        m_wizard->setWindowIcon(appIcon());
        m_wizard->setTriggers(m_boundTriggers);
        connect(m_wizard, &SetupWizard::openSettingsRequested, this, &AppController::showSettings);
    }
    m_wizard->show();
    m_wizard->raise();
    m_wizard->activateWindow();
}

// Baking in the thread pool leaves freed frames in malloc arenas: hand them back to the system
// a couple of seconds after a heavy operation, one call per burst (NFR-2, platform/Memory.h)
void AppController::scheduleMemoryRelease()
{
    if (!m_memoryTimer) {
        m_memoryTimer = new QTimer(this);
        m_memoryTimer->setSingleShot(true);
        m_memoryTimer->setInterval(2000);
        connect(m_memoryTimer, &QTimer::timeout, this, [] { releaseFreedMemory(); });
    }
    m_memoryTimer->start();
}

// --- Set on disk (NFR-5, APP-6) and preview (PST-6) ---

void AppController::restoreSession()
{
    if (!m_settings.keepSetOnRestart) {
        SetStorage::removeSession(sessionDirectory());   // the option was turned off: leave no traces
        return;
    }
    QString error;
    const std::optional<CaptureSet> set = SetStorage::loadSession(sessionDirectory(), &error);
    if (!set) {
        qCWarning(lcApp) << "saved set is not readable:" << error;
        notify(tr("The saved set could not be read: %1").arg(error));
        return;
    }
    qCInfo(lcApp) << "restored set:" << set->shots.size() << "shots";
    if (!set->isEmpty())
        m_model.resetWithoutHistory(*set);
    m_saveTimer->stop();   // just read it, nothing to write
}

void AppController::saveSession()
{
    m_saveTimer->stop();
    if (!m_settings.keepSetOnRestart)
        return;
    QString error;
    if (!SetStorage::saveSession(m_model.set(), sessionDirectory(), &error))
        qCWarning(lcApp) << "saving the set failed:" << error;
}

void AppController::showPreview()
{
    if (m_model.set().isEmpty())
        return;
    if (!m_preview) {
        m_preview = new PreviewDialog;
        m_preview->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_preview, &QObject::destroyed, this, &AppController::scheduleMemoryRelease);
        m_preview->setWindowIcon(appIcon());
        connect(m_preview, &PreviewDialog::saveToFolderRequested, this, &AppController::saveSetToFolder);
    }
    m_preview->showSet(m_model.set(), m_settings.pasteOptions());
    m_preview->show();
    m_preview->raise();
    m_preview->activateWindow();
}

void AppController::showAbout()
{
    // Modeless, one per program: a repeated call raises the one already open
    if (!m_about) {
        AboutInfo info;
        info.version = QStringLiteral(MARKPASTE_VERSION);
        info.homepage = QStringLiteral(MARKPASTE_HOMEPAGE_URL);
        info.environment = m_platform.environment;
        info.capture = m_platform.capture ? m_platform.capture->name() : QString();
        info.hotkeys = m_platform.hotkeys ? m_platform.hotkeys->name() : QString();
        info.clipboard = m_platform.clipboard ? m_platform.clipboard->name() : QString();
        info.injector = m_platform.injector && m_platform.injector->isAvailable() ? m_platform.injector->name() : QString();
        m_about = new AboutDialog(info);
        m_about->setAttribute(Qt::WA_DeleteOnClose);
        m_about->setWindowIcon(appIcon());
    }
    m_about->show();
    m_about->raise();
    m_about->activateWindow();
}

void AppController::saveSetToFolder()
{
    if (m_model.set().isEmpty())
        return;
    QSettings s;
    const QString start = s.value(QStringLiteral("export/lastDir"),
                                  QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();
    const QString parent = QFileDialog::getExistingDirectory(m_preview ? static_cast<QWidget *>(m_preview) : nullptr,
                                                             tr("Save the set to a folder"), start);
    if (parent.isEmpty())
        return;
    s.setValue(QStringLiteral("export/lastDir"), parent);
    // A separate folder for every save: A.png and message.md of earlier sets are not overwritten
    const QString dir = QDir(parent).filePath(
        QStringLiteral("Markpaste %1").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH-mm-ss"))));
    const CaptureSet set = m_model.set();
    const RenderStyle style = m_settings.renderStyle();
    const MessageTemplate tpl = m_settings.messageTemplate();
    QtConcurrent::run([set, style, tpl, dir] {
        QString error;
        const QStringList files = SetStorage::exportSet(set, style, tpl, dir, &error);
        return files.isEmpty() ? error : QString();
    }).then(this, [this, dir](const QString &error) {
        if (!error.isEmpty()) {
            notify(tr("Could not save the set: %1").arg(error));
            return;
        }
        scheduleMemoryRelease();
        qCInfo(lcApp) << "set saved to a folder";
        notify(tr("Saved to %1").arg(QDir::toNativeSeparators(dir)));
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });
}

void AppController::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;   // GNOME without the extension: everything is reachable without a tray (APP-1)
    m_tray = new QSystemTrayIcon(appIcon(), this);
    m_tray->setToolTip(QStringLiteral("Markpaste"));
    auto *menu = new QMenu;
    menu->addAction(tr("Shot with editor"), this, &AppController::capture);
    menu->addAction(tr("Shot without editor"), this, &AppController::captureToStrip);
    menu->addAction(tr("Show set"), this, &AppController::showStrip);
    menu->addAction(tr("Preview message…"), this, &AppController::showPreview);
    menu->addAction(tr("Save to folder…"), this, &AppController::saveSetToFolder);
    menu->addAction(tr("Settings"), this, &AppController::showSettings);
    menu->addAction(tr("Check setup…"), this, &AppController::showSetupWizard);
    menu->addAction(tr("About Markpaste"), this, &AppController::showAbout);
    menu->addSeparator();
    menu->addAction(tr("Quit"), qApp, &QApplication::quit);
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::messageClicked, this, [this] {
        if (const QUrl url = std::exchange(m_trayClickUrl, {}); url.isValid())
            QDesktopServices::openUrl(url);
    });
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r != QSystemTrayIcon::Trigger)
            return;
        // Clicking the icon shows or hides the strip (even an empty one). Capturing is Print or the menu.
        if (m_strip->isVisible())
            m_strip->hide();
        else
            showStrip();
    });
    m_tray->show();
}

void AppController::notify(const QString &text)
{
    qCInfo(lcApp) << "notify:" << text;
    m_trayClickUrl.clear();   // clicking a new notification opens nothing
    if (m_strip && m_strip->isVisible())
        m_strip->showMessage(text, 6000);
    else if (m_tray)
        m_tray->showMessage(QStringLiteral("Markpaste"), text, appIcon(), 5000);
    else {
        showStrip();
        m_strip->showMessage(text, 6000);
    }
}

} // namespace mp
