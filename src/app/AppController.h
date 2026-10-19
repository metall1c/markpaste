#pragma once

#include "Settings.h"
#include "core/CaptureSetModel.h"
#include "paste/PasteOrchestrator.h"
#include "platform/PlatformFactory.h"

#include <QObject>
#include <QPointer>

class QSystemTrayIcon;
class QMimeData;

namespace mp {

class CaptureSession;
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
    // Команды от CLI и второго экземпляра: capture, paste, copy, show, clear, quit
    void handleCommand(const QString &command);
    // Команда первого запуска: без явной команды и с пустым набором — только трей
    void handleStartup(const QString &command);

public slots:
    void capture();
    void paste(bool fromHotkey = false);
    void copySet();
    void clearSet();
    void showStrip();
    void showSettings();

private:
    void bindHotkeys();
    void onCaptured(const QImage &image, qreal dpr);
    void openEditor(const QUuid &shotId);
    void hideOwnWindows();
    void restoreOwnWindows();
    void setupTray();
    void notify(const QString &text);
    QMimeData *dragData(const QList<QUuid> &shots);
    void updatePasteCaption();

    Settings m_settings;
    Platform m_platform;
    CaptureSetModel m_model;
    PasteOrchestrator *m_paste = nullptr;
    StripWindow *m_strip = nullptr;
    ShotEditor *m_editor = nullptr;
    QPointer<CaptureSession> m_capture;
    QSystemTrayIcon *m_tray = nullptr;
    QHash<Action, QString> m_boundTriggers;   // фактические сочетания от системы
    bool m_capturing = false;
    bool m_editorWasVisible = false;
    bool m_stripWasVisible = false;
};

} // namespace mp
