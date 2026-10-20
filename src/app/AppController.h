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

// Контроллер: связывает модель, платформу и окна (ТЗ разд. 8, модуль «Контроллер»).
class AppController : public QObject
{
    Q_OBJECT
public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() override;

    void start();
    // Команды от CLI и второго экземпляра: capture, capture-to-strip, paste, copy, show, clear, quit
    void handleCommand(const QString &command);
    // Команда первого запуска: без явной команды и с пустым набором — только трей
    void handleStartup(const QString &command);

public slots:
    void capture();
    // Снимок сразу в ленту: выделил область — снимок в наборе, редактор не открывается (ADR 0022)
    void captureToStrip();
    void paste(bool fromHotkey = false);
    void copySet();
    void clearSet();
    void showStrip();
    void showSettings();
    void showSetupWizard();   // APP-4: при первом запуске и из меню трея
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
    QTimer *m_memoryTimer = nullptr;   // набор пишется на диск не на каждую правку, а через секунду тишины
    QSystemTrayIcon *m_tray = nullptr;
    QHash<Action, QString> m_boundTriggers;   // фактические сочетания от системы
    QUrl m_trayClickUrl;   // что открыть по клику на последнее уведомление в трее
    bool m_capturing = false;
    // Набор вставлен или скопирован и с тех пор не менялся: «Начинать новый набор при следующем
    // захвате» очищает только такой (ТЗ разд. 4, шаг 8)
    bool m_setDelivered = false;
    bool m_editorWasVisible = false;
    bool m_stripWasVisible = false;
};

} // namespace mp
