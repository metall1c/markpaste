#pragma once

#include "Settings.h"
#include "core/CaptureSetModel.h"
#include "paste/PasteOrchestrator.h"
#include "platform/PlatformFactory.h"

#include <QObject>
#include <QPointer>
#include <QUrl>

class QSystemTrayIcon;
class QTimer;
class QMimeData;

namespace mp {

class AboutDialog;
class CaptureSession;
class PreviewDialog;
class SetupWizard;
class UpdateChecker;
class ShotEditor;
class StripWindow;

// Controller: ties together the model, the platform and the windows (spec section 8, "Controller" module).
class AppController : public QObject
{
    Q_OBJECT
public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    void start();
    // Commands from the CLI and a second instance: capture, capture-to-strip, paste, copy, show, clear, quit
    void handleCommand(const QString &command);
    // First-start command: without an explicit command and with an empty set, only the tray
    void handleStartup(const QString &command);

public slots:
    void capture();
    // Capture straight to the strip: select an area and the shot is in the set, no editor (ADR 0022)
    void captureToStrip();
    void paste(bool fromHotkey = false);
    void copySet();
    void clearSet();
    void showStrip();
    void showSettings();
    void showSetupWizard();   // APP-4: on first start and from the tray menu
    void showPreview();       // PST-6
    void showAbout();
    void saveSetToFolder();   // APP-6

private:
    void bindHotkeys();
    void startCapture(bool toStrip);
    void onCaptured(const QImage &image, qreal dpr, bool toStrip);
    void openEditor(const QUuid &shotId, bool fresh = false);
    void hideOwnWindows();
    void restoreOwnWindows();
    void setupTray();
    void notify(const QString &text);
    QMimeData *dragData(const QList<QUuid> &shots);
    void updatePasteCaption();
    void restoreSession();
    void saveSession();
    void scheduleMemoryRelease();

    Settings m_settings;
    Platform m_platform;
    CaptureSetModel m_model;
    PasteOrchestrator *m_paste = nullptr;
    StripWindow *m_strip = nullptr;
    ShotEditor *m_editor = nullptr;
    QPointer<CaptureSession> m_capture;
    QPointer<SetupWizard> m_wizard;
    QPointer<PreviewDialog> m_preview;
    QPointer<AboutDialog> m_about;
    UpdateChecker *m_updates = nullptr;
    QTimer *m_saveTimer = nullptr;
    QTimer *m_memoryTimer = nullptr;   // the set is written to disk not on every edit but after a second of quiet
    QSystemTrayIcon *m_tray = nullptr;
    QHash<Action, QString> m_boundTriggers;   // actual shortcuts reported by the system
    QUrl m_trayClickUrl;   // what to open when the last tray notification is clicked
    bool m_capturing = false;
    // The set was pasted or copied and has not changed since: "Start a new set on the next
    // capture" clears only such a set (spec section 4, step 8)
    bool m_setDelivered = false;
    bool m_editorWasVisible = false;
    bool m_stripWasVisible = false;
};

} // namespace mp
