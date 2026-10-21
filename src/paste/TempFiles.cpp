#include "TempFiles.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>

namespace mp {

QString TempFiles::directory()
{
    static const QString dir = [] {
        const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        // RuntimeLocation on Linux is $XDG_RUNTIME_DIR; on other OSes it may match the home
        // or the temp directory, then a separate folder per process is used.
        if (!runtime.isEmpty() && runtime != QDir::homePath() && !qEnvironmentVariableIsEmpty("XDG_RUNTIME_DIR"))
            return runtime + QStringLiteral("/markpaste");
        return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
             + QStringLiteral("/markpaste-%1").arg(QCoreApplication::applicationPid());
    }();
    return dir;
}

QString TempFiles::write(const QString &fileName, const QByteArray &data)
{
    const QString dir = directory();
    if (!QDir().mkpath(dir))
        return {};
    QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    const QString path = dir + u'/' + fileName;
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit())
        return {};
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return path;
}

void TempFiles::clear()
{
    QDir(directory()).removeRecursively();
}

} // namespace mp
