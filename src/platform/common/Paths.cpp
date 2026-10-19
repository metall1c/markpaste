#include "platform/Paths.h"

#include <QDir>
#include <QStandardPaths>

namespace mp {

QString singleInstanceSocketName(const QString &appId)
{
#ifdef Q_OS_WIN
    return appId + QStringLiteral("-") + qEnvironmentVariable("USERNAME");
#else
    QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (dir.isEmpty())
        dir = QDir::tempPath();
    return dir + QStringLiteral("/%1.sock").arg(appId);
#endif
}

} // namespace mp
