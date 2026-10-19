#include "AppController.h"
#include "AppIcon.h"
#include "SingleInstance.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>

#include <csignal>

int main(int argc, char **argv)
{
    // Запись в pipe получателя, который закрыл свой конец, не должна убивать процесс.
    std::signal(SIGPIPE, SIG_IGN);

    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("pointshot"));
    QCoreApplication::setApplicationName(QStringLiteral("pointshot"));
    QCoreApplication::setApplicationVersion(QStringLiteral(POINTSHOT_VERSION));
    // Имя .desktop-файла = app id: по нему Wayland-композитор и порталы узнают программу.
    QGuiApplication::setDesktopFileName(QStringLiteral(POINTSHOT_APP_ID));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Pointshot"));
    QGuiApplication::setWindowIcon(ps::appIcon());
    QApplication::setQuitOnLastWindowClosed(false);   // работаем в фоне (APP-1)

    // Язык интерфейса — по системе (APP-5). Переводы Qt — для стандартных кнопок диалогов.
    QTranslator qtTranslator;
    if (qtTranslator.load(QLocale(), QStringLiteral("qtbase"), QStringLiteral("_"),
                          QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QCoreApplication::installTranslator(&qtTranslator);
    QTranslator appTranslator;
    if (appTranslator.load(QLocale(), QStringLiteral("pointshot"), QStringLiteral("_"), QStringLiteral(":/i18n")))
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
    ps::SingleInstance instance(QStringLiteral(POINTSHOT_APP_ID));
    if (!instance.listen()) {
        if (instance.sendToRunning(command))
            return 0;
        qWarning("Pointshot: another instance does not answer and the socket cannot be created");
        return 1;
    }
    if (command == u"quit")
        return 0;

    ps::AppController controller;
    QObject::connect(&instance, &ps::SingleInstance::commandReceived, &controller, &ps::AppController::handleCommand);
    controller.start();
    controller.handleStartup(command);
    return app.exec();
}
