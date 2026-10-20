#pragma once

#include <QDialog>

namespace mp {

// Что показать в окне «О программе». Версию и адреса задаёт сборка (CMake), сведения о
// системе — платформа; окно о них не знает, поэтому их передают снаружи.
struct AboutInfo {
    QString version;
    QString homepage;
    QString environment;   // «Wayland · KDE Plasma 6.7»
    QString capture;
    QString hotkeys;
    QString clipboard;
    QString injector;
};

// «О программе»: версия, авторы, лицензия, исходный код, сведения о системе.
// «Скопировать сведения» кладёт в буфер версию и систему — для сообщения об ошибке.
class AboutDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AboutDialog(const AboutInfo &info, QWidget *parent = nullptr);

    // Текст для «Скопировать сведения» (и для тестов)
    QString details() const;

private:
    AboutInfo m_info;
};

} // namespace mp
