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

// Мастер первого запуска (APP-4): одно окно на все проверки и разрешения (ТЗ разд. 9).
//
// Проверки настоящие, а не по названию окружения:
// - захват — снимок всех мониторов через ICapture (на Wayland здесь же появляется диалог
//   разрешения, и токен сохраняется до первого настоящего снимка);
// - клавиши — пользователь нажимает сочетание, контроллер передаёт срабатывание сюда
//   (hotkeyActivated), а не запускает захват;
// - вставка — полный путь режима 1: буфер → эмуляция Ctrl+V → текст должен появиться
//   в поле самого мастера. Буфер потом восстанавливается.
class SetupWizard : public QDialog
{
    Q_OBJECT
public:
    SetupWizard(Platform *platform, const QString &appId, QWidget *parent = nullptr);

    // Фактические сочетания от системы (на Wayland их подтверждает пользователь)
    void setTriggers(const QHash<Action, QString> &triggers);
    void hotkeyActivated(Action action);

    // Сколько ждать появления текста в поле после Ctrl+V. Тесты ставят 0.
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

private:
    void setStatus(QLabel *label, Result result, const QString &text);
    void updateHotkeys();
    void finishPaste(const Status &status, const QString &expected);

    Platform *m_platform;
    QString m_appId;
    QHash<Action, QString> m_triggers;
    QHash<Action, bool> m_pressed;
    int m_pasteWaitMs = 600;
    bool m_pasteRunning = false;

    QLabel *m_captureStatus;
    QPushButton *m_captureButton;
    QLabel *m_hotkeyStatus;
    QLabel *m_pasteStatus;
    QPushButton *m_pasteButton;
    QLineEdit *m_pasteField;
    QCheckBox *m_autostart = nullptr;

    Result m_captureResult = Result::Pending;
    Result m_hotkeyResult = Result::Pending;
    Result m_pasteResult = Result::Pending;
};

} // namespace mp
