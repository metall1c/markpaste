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

class ActionButton;

// Settings (APP-3). The minimal set for 1.0: hotkeys, paste, message, appearance.
class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    // boundTriggers are the actual shortcuts from the system; shown instead of our own when
    // the system assigns shortcuts (IHotkeys::systemAssignsTriggers)
    SettingsDialog(const Settings &settings, const Platform &platform,
                   const QHash<Action, QString> &boundTriggers = {}, QWidget *parent = nullptr);
    Settings settings() const;
    // Autostart is not kept in settings but as a file in ~/.config/autostart: the system sees it too
    bool autostart() const;

private:
    Settings m_settings;
    QLineEdit *m_captureKey;
    QLineEdit *m_pasteKey;
    QLineEdit *m_toStripKey;
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
    ActionButton *m_accent;
    QColor m_accentColor;
    QCheckBox *m_autostart = nullptr;
    QCheckBox *m_hideEmpty;
    QCheckBox *m_captions;
    QCheckBox *m_halo;
    QSpinBox *m_badgeSize;
    QComboBox *m_uiLanguage;
    QSpinBox *m_maxSide;
    QSpinBox *m_jpegThreshold;
    QSpinBox *m_jpegQuality;
    QCheckBox *m_keepSet;
    QCheckBox *m_checkUpdates;
};

} // namespace mp
