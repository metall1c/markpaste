#pragma once

#include "platform/PlatformTypes.h"

#include <QString>
#include <QStringList>

namespace mp::lx {

// GNOME's own shortcuts that take a key before a GlobalShortcuts portal shortcut. GNOME's Print opens
// its screenshot tool, and our "Print" binding never fires (spec section 9: "Print Screen is often taken
// by the desktop", problems.md 2026-10-21). The settings are read and changed with the gsettings tool.
//
// Releasing a key removes it from the GNOME setting and saves the old value in QSettings, so that
// restore() can give it back (the program settings have a button for it).
namespace GnomeKeybindings {

struct Holder {
    QString schema;
    QString key;
    QStringList accels;   // the whole current value
    QString description;  // for the user: "the GNOME screenshot tool"
};

bool isGnome();   // XDG_CURRENT_DESKTOP has GNOME (Ubuntu: "ubuntu:GNOME")

// GNOME settings that hold this key. trigger: "Print", "Ctrl+Print" (portal or portable form).
QList<Holder> holders(const QString &trigger);
Status release(const QString &trigger);
bool hasReleased();
Status restore();

// Pure helpers, public for tests.
// "Ctrl+Print", "Primary+Print", "<Control>Print", "<Primary><Shift>s" → "ctrl+print", "ctrl+shift+s"
QString canonical(const QString &triggerOrAccel);
// gsettings output "['Print', '<Shift>Print']" or "@as []" → the list; and back
QStringList parseStrv(const QString &gvariant);
QString formatStrv(const QStringList &list);

} // namespace GnomeKeybindings

} // namespace mp::lx
