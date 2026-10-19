#include "TempFiles.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QStandardPaths>

namespace ps {

QString TempFiles::directory()
{
    static const QString dir = [] {
        const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        // RuntimeLocation на Linux — $XDG_RUNTIME_DIR; на других ОС может совпасть с домашним
        // каталогом или временным — тогда отдельная папка на процесс.
        if (!runtime.isEmpty() && runtime != QDir::homePath() && !qEnvironmentVariableIsEmpty("XDG_RUNTIME_DIR"))
            return runtime + QStringLiteral("/pointshot");
        return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
             + QStringLiteral("/pointshot-%1").arg(QCoreApplication::applicationPid());
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

} // namespace ps
