#include "AppController.h"

#include "AppIcon.h"
#include "SettingsDialog.h"
#include "paste/TempFiles.h"
#include "ui/CaptureOverlay.h"
#include "ui/MoveGhost.h"
#include "ui/ShotEditor.h"
#include "ui/StripWindow.h"

#include <QApplication>
#include <QBuffer>
#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QMenu>
#include <QScreen>
#include <QMimeData>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent>

Q_LOGGING_CATEGORY(lcApp, "pointshot.app")

namespace ps {

AppController::AppController(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<ps::Status>();
    qRegisterMetaType<ps::Action>();
    qRegisterMetaType<ps::ActivationContext>();
}

AppController::~AppController()
{
    delete m_editor;
    delete m_strip;
    TempFiles::clear();   // временные PNG — при выходе (ТЗ разд. 10)
}

void AppController::start()
{
    m_settings = Settings::load();
    m_platform = createPlatform(QStringLiteral(POINTSHOT_APP_ID));

    m_paste = new PasteOrchestrator(m_platform.clipboard.get(), m_platform.injector.get(), this);
    connect(m_paste, &PasteOrchestrator::progress, this, [this](int done, int total) {
        if (m_strip)
            m_strip->setProgress(done, total);
    });
    connect(m_paste, &PasteOrchestrator::finished, this, [this](const Status &s, PasteMode mode) {
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
    connect(m_strip, &StripWindow::shotActivated, this, &AppController::openEditor);
    connect(m_strip, &StripWindow::newShotRequested, this, &AppController::capture);
    connect(m_strip, &StripWindow::pasteRequested, this, [this] { paste(false); });
    connect(m_strip, &StripWindow::copyRequested, this, &AppController::copySet);
    connect(m_strip, &StripWindow::clearRequested, this, &AppController::clearSet);
    connect(m_strip, &StripWindow::cancelPasteRequested, m_paste, &PasteOrchestrator::cancel);
    connect(m_strip, &StripWindow::settingsRequested, this, &AppController::showSettings);
    connect(m_strip, &StripWindow::hideRequested, this, [this] {
        m_strip->hide();
        if (!m_tray)
            return;
        // Подсказать один раз, куда делась лента
        QSettings s;
        if (!s.value(QStringLiteral("general/hideHintShown")).toBool()) {
            s.setValue(QStringLiteral("general/hideHintShown"), true);
            m_tray->showMessage(QStringLiteral("Pointshot"), tr("The strip is in the tray. Click the tray icon to show it."),
                                appIcon(), 5000);
        }
    });
    updatePasteCaption();

    m_editor = new ShotEditor(&m_model);
    m_editor->setStyle(m_settings.renderStyle());
    m_editor->setWindowIcon(appIcon());
    connect(m_editor, &ShotEditor::newShotRequested, this, &AppController::capture);
    connect(m_editor, &ShotEditor::doneRequested, m_editor, &QWidget::hide);

    connect(&m_model, &CaptureSetModel::changed, this, [this] {
        if (m_strip && m_editor)
            m_strip->setCurrentShot(m_editor->isVisible() ? m_editor->shotId() : QUuid());
        // Лента видна, только пока в наборе есть снимки; пустой набор — программа в трее.
        // Без трея (GNOME без расширения) ленту не прячем: иначе программу не найти (APP-1).
        if (m_strip && m_tray && m_model.set().isEmpty() && !m_capturing)
            m_strip->hide();
    });

    if (m_platform.hotkeys) {
        connect(m_platform.hotkeys.get(), &IHotkeys::activated, this, [this](Action a, const ActivationContext &) {
            qCInfo(lcApp) << "hotkey activated" << int(a);
            switch (a) {
            case Action::Capture: capture(); break;
            case Action::PasteSet: paste(true); break;
            case Action::NewShot: capture(); break;
            case Action::ClearSet: clearSet(); break;
            }
        });
        connect(m_platform.hotkeys.get(), &IHotkeys::released, this, [this](Action a) {
            if (a == Action::PasteSet)
                m_paste->hotkeyReleased();
        });
        bindHotkeys();
    }

    setupTray();
    if (m_platform.runningUnderXWayland)
        notify(tr("Pointshot runs under XWayland: global hotkeys and capture will not work."));
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
    // Обычный запуск (в том числе автозапуск): пустой набор — только значок в трее
    if (command == u"show" && m_tray && m_model.set().isEmpty()) {
        QSettings s;
        if (!s.value(QStringLiteral("general/welcomeShown")).toBool()) {
            s.setValue(QStringLiteral("general/welcomeShown"), true);
            const QString key = m_boundTriggers.value(Action::Capture, m_settings.captureHotkey);
            m_tray->showMessage(QStringLiteral("Pointshot"),
                                tr("Pointshot is in the tray. Press %1 to capture an area.").arg(key), appIcon(), 6000);
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
        openEditor(id);
    });
}

void AppController::openEditor(const QUuid &shotId)
{
    m_editor->showShot(shotId);
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
    if (buttonCanSequence)
        m_strip->setPasteCaption(tr("Paste set"), tr("Pastes all shots and the text into the active chat window"));
    else
        m_strip->setPasteCaption(tr("Copy set"), tr("Copies files and text — then press Ctrl+V in the chat"));
}

void AppController::paste(bool fromHotkey)
{
    if (m_paste->isRunning() || m_model.set().isEmpty())
        return;
    if (m_editor && m_editor->isVisible())
        m_editor->hide();   // иначе фокус может остаться в редакторе, а не в чате

    PasteOptions opt = m_settings.pasteOptions();
    opt.waitHotkeyRelease = fromHotkey;
    const bool buttonCanSequence = m_paste->canSequential() && (!m_platform.nativeWayland || m_platform.supportsLayerShell);
    if (opt.mode != PasteMode::FilesAndText && (!m_paste->canSequential() || (!fromHotkey && !buttonCanSequence)))
        opt.mode = PasteMode::FilesAndText;   // запасной режим — полноценный (ТЗ разд. 9)
    m_paste->start(m_model.set(), opt);
}

void AppController::copySet()
{
    if (m_paste->isRunning() || m_model.set().isEmpty())
        return;
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
        const QImage img = renderShot(m_model.set().shots[index], index, opt.style);
        const EncodedImage enc = encodeForPaste(img, opt.encode);
        const QString path = TempFiles::write(QStringLiteral("shot-%1.%2").arg(shotLetter(index)).arg(enc.extension), enc.data);
        if (!path.isEmpty())
            urls << QUrl::fromLocalFile(path);
    }
    auto *md = new QMimeData;
    md->setUrls(urls);
    if (shots.size() > 1) {
        // Режим 3: файлы перетаскиванием, текст — в буфер, вставить вторым действием
        const QString text = buildMessage(m_model.set(), opt.messageTemplate);
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
    SettingsDialog dlg(m_settings, m_platform);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const Settings old = m_settings;
    m_settings = dlg.settings();
    m_settings.save();
    m_strip->setStyle(m_settings.renderStyle());
    m_editor->setStyle(m_settings.renderStyle());
    if (old.captureHotkey != m_settings.captureHotkey || old.pasteHotkey != m_settings.pasteHotkey)
        notify(tr("New hotkeys take effect after restart"));
}

void AppController::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;   // GNOME без расширения: всё доступно и без трея (APP-1)
    m_tray = new QSystemTrayIcon(appIcon(), this);
    m_tray->setToolTip(QStringLiteral("Pointshot"));
    auto *menu = new QMenu;
    menu->addAction(tr("Capture"), this, &AppController::capture);
    menu->addAction(tr("Show set"), this, &AppController::showStrip);
    menu->addAction(tr("Settings"), this, &AppController::showSettings);
    menu->addSeparator();
    menu->addAction(tr("Quit"), qApp, &QApplication::quit);
    m_tray->setContextMenu(menu);
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
    if (m_strip && m_strip->isVisible())
        m_strip->showMessage(text, 6000);
    else if (m_tray)
        m_tray->showMessage(QStringLiteral("Pointshot"), text, appIcon(), 5000);
    else {
        showStrip();
        m_strip->showMessage(text, 6000);
    }
}

} // namespace ps
