#include "LogFile.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QStandardPaths>

#include <algorithm>

namespace mp {

namespace {

constexpr qint64 kMaxBytes = 1024 * 1024;
constexpr int kKeep = 3;

QtMessageHandler g_previous = nullptr;
QMutex g_mutex;
QFile *g_file = nullptr;

QString rotatedName(const QString &path, int n)
{
    // markpaste.log → markpaste.1.log
    const int dot = path.lastIndexOf(u'.');
    return path.left(dot) + QStringLiteral(".%1").arg(n) + path.mid(dot);
}

void handler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    if (g_previous)
        g_previous(type, ctx, msg);

    static const char *const kLevel[] = {"debug", "warning", "critical", "fatal", "info"};
    const QByteArray line = QStringLiteral("%1 %2 %3: %4\n")
                                .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs),
                                     QLatin1String(kLevel[std::clamp(int(type), 0, 4)]),
                                     QLatin1String(ctx.category ? ctx.category : "default"), msg)
                                .toUtf8();
    QMutexLocker lock(&g_mutex);
    if (!g_file)
        return;
    if (g_file->size() + line.size() > kMaxBytes) {
        const QString path = g_file->fileName();
        g_file->close();
        LogFile::rotate(path, 0, kKeep);
        if (!g_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            return;
    }
    g_file->write(line);
    g_file->flush();   // журнал нужен как раз когда программа упала
}

} // namespace

QString LogFile::directory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/logs");
}

QString LogFile::currentPath()
{
    return directory() + QStringLiteral("/markpaste.log");
}

void LogFile::rotate(const QString &path, qint64 maxBytes, int keep)
{
    if (!QFile::exists(path) || QFileInfo(path).size() <= maxBytes)
        return;
    QFile::remove(rotatedName(path, keep));
    for (int i = keep - 1; i >= 1; --i)
        QFile::rename(rotatedName(path, i), rotatedName(path, i + 1));
    QFile::rename(path, rotatedName(path, 1));
}

void LogFile::install()
{
    const QString dir = directory();
    if (!QDir().mkpath(dir))
        return;
    QFile::setPermissions(dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    rotate(currentPath(), kMaxBytes, kKeep);
    auto *file = new QFile(currentPath());   // живёт до конца процесса: сообщения бывают и в деструкторах
    if (!file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        delete file;
        return;
    }
    file->setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    {
        QMutexLocker lock(&g_mutex);
        g_file = file;
    }
    g_previous = qInstallMessageHandler(handler);
}

} // namespace mp
