#pragma once

#include <QDialog>

namespace mp {

// What the About window shows. The version and URLs come from the build (CMake), the system
// details from the platform; the window knows about neither, so they are passed in from outside.
struct AboutInfo {
    QString version;
    QString homepage;
    QString environment;   // «Wayland · KDE Plasma 6.7»
    QString capture;
    QString hotkeys;
    QString clipboard;
    QString injector;
};

// "About": version, authors, license, source code, system details.
// "Copy details" puts the version and system into the clipboard, for a bug report.
class AboutDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AboutDialog(const AboutInfo &info, QWidget *parent = nullptr);

    // Text for "Copy details" (and for tests)
    QString details() const;

private:
    AboutInfo m_info;
};

} // namespace mp
