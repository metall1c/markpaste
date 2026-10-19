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
    SettingsDialog(const Settings &settings, const Platform &platform, QWidget *parent = nullptr);
    Settings settings() const;

private:
    Settings m_settings;
    QLineEdit *m_captureKey;
    QLineEdit *m_pasteKey;
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
};

} // namespace mp
