#pragma once

#include "platform/PlatformFactory.h"

#include <QDialog>
#include <QHash>
#include <QPointer>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace mp {

class StatusLine;

// First-start wizard (APP-4): one window for all checks and permissions (spec section 9).
//
// The checks are real, not based on the environment name:
// - capture: a shot of all monitors via ICapture (on Wayland the permission dialog
//   appears here, and the token is kept until the first real shot);
// - hotkeys: the user presses the shortcut, the controller passes the activation here
//   (hotkeyActivated) instead of starting a capture;
// - paste: the full mode 1 path: clipboard → Ctrl+V emulation → the text must appear
//   in the wizard's own field. The clipboard is restored afterwards.
class SetupWizard : public QDialog
{
    Q_OBJECT
public:
    SetupWizard(Platform *platform, const QString &appId, QWidget *parent = nullptr);

    // Actual shortcuts from the system (on Wayland the user confirms them)
    void setTriggers(const QHash<Action, QString> &triggers);
    void hotkeyActivated(Action action);

    // How long to wait for the text to appear in the field after Ctrl+V. Tests set 0.
    void setPasteWaitMs(int ms) { m_pasteWaitMs = ms; }

    enum class Result { Pending, Ok, Warning, Failed };
    Result captureResult() const { return m_captureResult; }
    Result hotkeyResult() const { return m_hotkeyResult; }
    Result pasteResult() const { return m_pasteResult; }

public slots:
    void checkCapture();
    void checkPaste();

signals:
    void openSettingsRequested();
    // The desktop gave up a key (GNOME Print): binding it needs a new portal session, i.e. a restart
    void restartRequested();

private:
    void setStatus(StatusLine *label, Result result, const QString &text);
    void updateHotkeys();
    void finishPaste(const Status &status, const QString &expected);
    void updateDesktopKeys();   // "Print is taken by the GNOME screenshot tool" + the button
    void releaseDesktopKeys();

    Platform *m_platform;
    QString m_appId;
    QHash<Action, QString> m_triggers;
    QHash<Action, bool> m_pressed;
    int m_pasteWaitMs = 600;
    bool m_pasteRunning = false;

    StatusLine *m_captureStatus;
    QPushButton *m_captureButton;
    StatusLine *m_hotkeyStatus;
    StatusLine *m_desktopKeyStatus;
    QPushButton *m_desktopKeyButton;
    QStringList m_heldKeys;                    // bound keys the desktop takes first
    QHash<QString, QString> m_keyHolders;      // key → who holds it ("" = nobody); gsettings once per key
    StatusLine *m_pasteStatus;
    QPushButton *m_pasteButton;
    QLineEdit *m_pasteField;
    QCheckBox *m_autostart = nullptr;

    Result m_captureResult = Result::Pending;
    Result m_hotkeyResult = Result::Pending;
    Result m_pasteResult = Result::Pending;
};

} // namespace mp
