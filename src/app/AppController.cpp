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

// Флаги «уже показано». Раньше хранились в группе «general»: QSettings пишет её в INI как
// [%General] и при следующем запуске читает как «General» — ключ с маленькой буквы не
// находился, и мастер открывался каждый раз (docs/problems.md). Старые значения тоже учитываем.
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
        saveSession();   // последние правки — до выхода
    delete m_preview;
    delete m_editor;
    delete m_strip;
    TempFiles::clear();   // временные PNG — при выходе (ТЗ разд. 10)
}

void AppController::start()
{
    m_settings = Settings::load();
    m_platform = createPlatform(QStringLiteral(MARKPASTE_APP_ID));
    disableWindowAnimations();   // спрятанное перед снимком окно не должно «таять» в кадре

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
        // Подсказать один раз, куда делась лента
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
            scheduleMemoryRelease();   // очистили набор — кадры больше не нужны
    });

    connect(&m_model, &CaptureSetModel::changed, this, [this] {
        if (m_strip && m_editor)
            m_strip->setCurrentShot(m_editor->isVisible() ? m_editor->shotId() : QUuid());
        // Лента видна, только пока в наборе есть снимки; пустой набор — программа в трее
        // (отключается настройкой). Без трея (GNOME без расширения) ленту не прячем:
        // иначе программу не найти (APP-1).
        if (m_settings.keepSetOnRestart && m_saveTimer)
            m_saveTimer->start();
        if (m_strip && m_tray && m_settings.hideStripWhenEmpty && m_model.set().isEmpty() && !m_capturing)
            m_strip->hide();
    });

    if (m_platform.hotkeys) {
        connect(m_platform.hotkeys.get(), &IHotkeys::activated, this, [this](Action a, const ActivationContext &) {
            qCInfo(lcApp) << "hotkey activated" << int(a);
            // Пока открыт мастер, клавиша только проверяется, а не запускает действие
            if (m_wizard && m_wizard->isVisible()) {
                m_wizard->hotkeyActivated(a);
                return;
            }
            switch (a) {
            case Action::Capture: capture(); break;
            case Action::PasteSet: paste(true); break;
            case Action::NewShot: capture(); break;
            case Action::ClearSet: clearSet(); break;
            }
        });
        connect(m_platform.hotkeys.get(), &IHotkeys::triggerChanged, this, [this](Action a, const QString &trigger) {
            m_boundTriggers.insert(a, trigger);
            if (m_wizard)
                m_wizard->setTriggers(m_boundTriggers);
            updatePasteCaption();   // сочетания на кнопке вставки и в подсказке пустой ленты
        });
        connect(m_platform.hotkeys.get(), &IHotkeys::released, this, [this](Action a) {
            if (a == Action::PasteSet)
                m_paste->hotkeyReleased();
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
        // Клик по этому уведомлению — страница релиза. Только по нему: любое следующее
        // уведомление сбрасывает адрес (notify), иначе страницу открыл бы клик по «Ошибка вставки».
        if (m_tray && !(m_strip && m_strip->isVisible()))
            m_trayClickUrl = url;
    });
    m_updates->setEnabled(m_settings.checkUpdates);

    if (m_platform.runningUnderXWayland)
        notify(tr("Markpaste runs under XWayland: global hotkeys and capture will not work."));
}

void AppController::bindHotkeys()
{
    // Все сочетания — одним запросом: на Wayland портал показывает один диалог.
    const QHash<Action, QString> wanted{
        {Action::Capture, m_settings.captureHotkey},
        {Action::PasteSet, m_settings.pasteHotkey},
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
        // Только через сокет: debug-ghost <x> <y> <w> <h> — контур переноса на 2 с
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
        // Только через сокет, для проверки переноса ленты: debug-move-strip <экран> <left|right> <отступ>
        const QStringList a = command.split(u' ');
        for (QScreen *scr : QGuiApplication::screens())
            if (a.size() == 4 && scr->name() == a[1])
                m_strip->moveTo(scr, PanelPlacement{a[2] == u"left" ? PanelEdge::Left : PanelEdge::Right, a[3].toInt(), 12});
    }
    else   // show и повторный запуск из меню (APP-1)
        showStrip();
}

void AppController::handleStartup(const QString &command)
{
    // Обычный запуск (в том числе автозапуск): пустой набор — только значок в трее,
    // если лента прячется при пустом наборе
    QSettings s;
    if (command == u"show" && !onceFlag(s, QStringLiteral("setupShown"))) {
        // Первый запуск — мастер проверки вместо уведомления (APP-4)
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

// --- Захват ---

void AppController::hideOwnWindows()
{
    m_editorWasVisible = m_editor && m_editor->isVisible();
    m_stripWasVisible = m_strip && m_strip->isVisible();
    if (m_editorWasVisible)
        m_editor->hide();
    if (m_stripWasVisible)
        m_strip->hide();
}

void AppController::restoreOwnWindows()
{
    if (m_stripWasVisible)
        m_strip->present();
    if (m_editorWasVisible)
        m_editor->show();
}

void AppController::capture()
{
    if (m_capturing || m_paste->isRunning())
        return;
    if (!m_platform.capture) {
        notify(tr("Screen capture is not available on this system"));
        return;
    }
    if (m_settings.clearOnNextCapture && !m_model.set().isEmpty() && m_editor && !m_editor->isVisible())
        m_model.clear();
    if (m_model.set().isFull()) {   // CAP-8
        showStrip();
        m_strip->showMessage(tr("The set is full (26 shots)"));
        return;
    }
    m_capturing = true;

    // Свои окна не должны попасть в снимок. Компоновщику нужно время, чтобы их убрать.
    const bool hadWindows = (m_editor && m_editor->isVisible()) || (m_strip && m_strip->isVisible());
    hideOwnWindows();
    auto timer = std::make_shared<QElapsedTimer>();
    timer->start();
    QTimer::singleShot(hadWindows ? 180 : 0, this, [this, timer] {
        m_platform.capture->captureAllScreens().then(this, [this, timer](const CaptureResult &r) {
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
            connect(m_capture, &CaptureSession::selected, this, [this, timer](const CaptureSelection &sel) {
                m_capturing = false;
                m_capture->deleteLater();
                onCaptured(sel.image, sel.devicePixelRatio);
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

void AppController::onCaptured(const QImage &image, qreal dpr)
{
    // PNG в памяти — чтобы 26 кадров 4K укладывались в NFR-2. Кодируем в пуле потоков.
    QtConcurrent::run([image] {
        QByteArray png;
        QBuffer buf(&png);
        buf.open(QIODevice::WriteOnly);
        image.save(&buf, "PNG", 50);   // 50 — баланс скорости и размера, сжатие без потерь
        return png;
    }).then(this, [this, size = image.size(), dpr](const QByteArray &png) {
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
        openEditor(id, true);
    });
}

void AppController::openEditor(const QUuid &shotId, bool fresh)
{
    m_editor->showShot(shotId, fresh);
    m_strip->setCurrentShot(shotId);
}

// --- Вставка ---

void AppController::updatePasteCaption()
{
    if (!m_strip)
        return;
    const bool sequential = m_paste->canSequential();
    // На Wayland кнопка вставки работает в режиме 1, только если лента не забирает фокус
    // (layer-shell); иначе она копирует набор (ТЗ разд. 10).
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
        // Фактические сочетания: KDE отдаёт все сразу — «Ctrl+Alt+V, Alt+V», и какое из них
        // нажато, не говорит. Маска нужна, если хоть одно — с Alt без Ctrl; лишняя (при Ctrl+Alt+V)
        // безвредна: нажатие Ctrl, когда Ctrl уже зажат
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
        opt.mode = PasteMode::FilesAndText;   // запасной режим — полноценный (ТЗ разд. 9)

    // Активно другое окно программы (настройки, предпросмотр, мастер): Ctrl+V ушёл бы в него, а не
    // в чат, а лента сказала бы «Вставлено». Редактор не в счёт — его прячем ниже; лента фокус не
    // забирает (docs/problems.md, 2026-10-20)
    if (opt.mode != PasteMode::FilesAndText) {
        QWidget *active = QApplication::activeWindow();
        if (active && active != m_editor && active != m_strip) {
            notify(tr("Click into the chat first: a Markpaste window is active"));
            return;
        }
    }

    if (m_editor && m_editor->isVisible()) {
        m_editor->commitPendingEdits();   // последний комментарий — тоже в сообщение
        m_editor->hide();   // иначе фокус может остаться в редакторе, а не в чате
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
    // Буквы в имени файлов — как в полном наборе
    QStringList files;
    PasteOptions opt = m_settings.pasteOptions();
    QList<QUrl> urls;
    for (const QUuid &id : shots) {
        const int index = m_model.set().shotIndex(id);
        if (index < 0)
            continue;   // снимок успели удалить
        const QImage img = renderShot(m_model.set().shots[index], index, opt.style);
        const EncodedImage enc = encodeForPaste(img, opt.encode);
        const QString path = TempFiles::write(QStringLiteral("shot-%1.%2").arg(shotLetter(index)).arg(enc.extension), enc.data);
        if (!path.isEmpty())
            urls << QUrl::fromLocalFile(path);
    }
    scheduleMemoryRelease();
    auto *md = new QMimeData;
    md->setUrls(urls);
    // Режим 3: файлы перетаскиванием, текст — в буфер, вставить вторым действием.
    // Без комментариев текста нет — буфер не трогаем и о тексте не пишем
    if (const QString text = buildMessage(m_model.set(), opt.messageTemplate); shots.size() > 1 && !text.isEmpty()) {
        m_platform.clipboard->write(ClipboardPayload::text(text), ClipboardFlag::ExcludeFromHistory);
        m_strip->showMessage(tr("Text copied — paste it after dropping the files"), 6000);
    }
    return md;
}

// --- Окна, трей, уведомления ---

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
    if (old.captureHotkey != m_settings.captureHotkey || old.pasteHotkey != m_settings.pasteHotkey)
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

// Запекание в пуле потоков оставляет освобождённые кадры в аренах malloc: отдаём их системе
// через пару секунд после тяжёлой операции, одним вызовом на серию (NFR-2, platform/Memory.h)
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

// --- Набор на диске (NFR-5, APP-6) и предпросмотр (PST-6) ---

void AppController::restoreSession()
{
    if (!m_settings.keepSetOnRestart) {
        SetStorage::removeSession(sessionDirectory());   // опцию выключили — следов не оставляем
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
    m_saveTimer->stop();   // только что прочитали — писать нечего
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
    // Немодальное, одно на программу: повторный вызов поднимает уже открытое
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
    // Своя папка на каждое сохранение: A.png и message.md прошлых наборов не перезаписываются
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
        return;   // GNOME без расширения: всё доступно и без трея (APP-1)
    m_tray = new QSystemTrayIcon(appIcon(), this);
    m_tray->setToolTip(QStringLiteral("Markpaste"));
    auto *menu = new QMenu;
    menu->addAction(tr("Capture"), this, &AppController::capture);
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
        // Клик по значку — показать или спрятать ленту (и пустую). Снять экран — Print или меню.
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
    m_trayClickUrl.clear();   // клик по новому уведомлению ничего не открывает
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
