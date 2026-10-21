#include "platform/Process.h"

#include <QCoreApplication>
#include <QProcess>
#include <QStringList>

#include <csignal>

namespace mp {

void prepareProcess()
{
#ifdef SIGPIPE
    std::signal(SIGPIPE, SIG_IGN);
#endif
}

bool restartWhenExited(const QStringList &args)
{
#ifdef Q_OS_UNIX
    // A shell waits until this process is gone (the single-instance socket is free), then starts the program
    const QString script = QStringLiteral("while kill -0 \"$1\" 2>/dev/null; do sleep 0.1; done; shift; exec \"$@\"");
    QStringList shArgs{QStringLiteral("-c"), script, QStringLiteral("sh"),
                       QString::number(QCoreApplication::applicationPid()), QCoreApplication::applicationFilePath()};
    shArgs << args;
    return QProcess::startDetached(QStringLiteral("/bin/sh"), shArgs);
#else
    Q_UNUSED(args);
    return false;
#endif
}

} // namespace mp
