#include "AppController.h"
#include "AppIcon.h"
#include "LogFile.h"
#include "SingleInstance.h"
#include "platform/Process.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>


int main(int argc, char **argv)
{
    // Особенности процесса под ОС: SIGPIPE на Unix и т. п. (platform/Process.h)
    mp::prepareProcess();

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("markpaste"));
    QCoreApplication::setApplicationName(QStringLiteral("markpaste"));
    QCoreApplication::setApplicationVersion(QStringLiteral(MARKPASTE_VERSION));
    // Имя .desktop-файла = app id: по нему Wayland-композитор и порталы узнают программу.
    QGuiApplication::setDesktopFileName(QStringLiteral(MARKPASTE_APP_ID));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Markpaste"));
    QGuiApplication::setWindowIcon(mp::appIcon());
    QApplication::setQuitOnLastWindowClosed(false);   // работаем в фоне (APP-1)

    // Язык интерфейса — по системе или из настроек (APP-3, APP-5). Переводы Qt — для стандартных кнопок диалогов.
    const QString uiLanguage = QSettings().value(QStringLiteral("ui/language")).toString();
    const QLocale uiLocale = uiLanguage == u"ru"   ? QLocale(QLocale::Russian)
                           : uiLanguage == u"en" ? QLocale(QLocale::English)
                                                 : QLocale();
    QTranslator qtTranslator;
    if (qtTranslator.load(uiLocale, QStringLiteral("qtbase"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QCoreApplication::installTranslator(&qtTranslator);
    QTranslator appTranslator;
    if (appTranslator.load(uiLocale, QStringLiteral("markpaste"), QStringLiteral("_"), QStringLiteral(":/i18n")))
        QCoreApplication::installTranslator(&appTranslator);

    QCommandLineParser cli;
    cli.setApplicationDescription(QStringLiteral("Capture, mark and paste screenshots with comments into any chat"));
    cli.addHelpOption();
    cli.addVersionOption();
    const QList<std::pair<QString, QString>> commands = {
        {QStringLiteral("capture"), QStringLiteral("Start capture in the running instance")},
        {QStringLiteral("paste"), QStringLiteral("Paste the set into the active window")},
        {QStringLiteral("copy"), QStringLiteral("Copy the set (files + text) to the clipboard")},
        {QStringLiteral("show"), QStringLiteral("Show the set strip")},
        {QStringLiteral("settings"), QStringLiteral("Open settings")},
        {QStringLiteral("setup"), QStringLiteral("Open the setup check (capture, hotkeys, paste)")},
        {QStringLiteral("about"), QStringLiteral("Show version and authors")},
        {QStringLiteral("clear"), QStringLiteral("Clear the set")},
        {QStringLiteral("quit"), QStringLiteral("Quit the running instance")},
    };
    QList<QCommandLineOption> options;
    for (const auto &[name, help] : commands)
        options << QCommandLineOption(name, help);
    cli.addOptions(options);
    cli.process(app);

    QString command = QStringLiteral("show");
    for (const auto &[name, help] : commands)
        if (cli.isSet(name))
            command = name;

    // Второй экземпляр — только передаёт команду (APP-2, CAP-7)
    mp::SingleInstance instance(QStringLiteral(MARKPASTE_APP_ID));
    if (!instance.listen()) {
        if (instance.sendToRunning(command))
            return 0;
        qWarning("Markpaste: another instance does not answer and the socket cannot be created");
        return 1;
    }
    if (command == u"quit")
        return 0;
    // Журнал в файл — только у основного экземпляра: второй живёт миг и только передаёт команду
    mp::LogFile::install();
    qInfo("Markpaste %s started", MARKPASTE_VERSION);

    mp::AppController controller;
    QObject::connect(&instance, &mp::SingleInstance::commandReceived, &controller, &mp::AppController::handleCommand);
    controller.start();
    controller.handleStartup(command);
    return app.exec();
}
