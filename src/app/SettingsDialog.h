#pragma once

#include "Settings.h"
#include "platform/PlatformFactory.h"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QPushButton;
class QSpinBox;

namespace mp {

// Настройки (APP-3). Минимальный набор для 1.0: клавиши, вставка, сообщение, вид.
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    // boundTriggers — фактические сочетания от системы; показываются вместо своих, если
    // сочетания назначает система (IHotkeys::systemAssignsTriggers)
    SettingsDialog(const Settings &settings, const Platform &platform,
                   const QHash<Action, QString> &boundTriggers = {}, QWidget *parent = nullptr);
    Settings settings() const;
    // Автозапуск хранится не в настройках, а файлом в ~/.config/autostart: его видит и система
    bool autostart() const;

private:
    Settings m_settings;
    QLineEdit *m_captureKey;
    QLineEdit *m_pasteKey;
    bool m_systemKeys = false;
    QCheckBox *m_quick;
    QCheckBox *m_clearOnNext;
    QComboBox *m_mode;
    QCheckBox *m_restore;
    QCheckBox *m_clearAfter;
    QSpinBox *m_delay;
    QComboBox *m_language;
    QLineEdit *m_header;
    QLineEdit *m_line;
    QLineEdit *m_intro;
    QLineEdit *m_outro;
    QCheckBox *m_textFirst;
    QPushButton *m_accent;
    QColor m_accentColor;
    QCheckBox *m_autostart = nullptr;
    QCheckBox *m_hideEmpty;
    QCheckBox *m_keepSet;
    QCheckBox *m_checkUpdates;
};

} // namespace mp
