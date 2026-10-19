#pragma once

#include <QByteArray>
#include <QFuture>
#include <QObject>

namespace ps::lx {

// Блокирующая запись всего буфера с общим таймаутом (poll), потом close(fd).
// Вызывать из рабочего потока. Возвращает false, если получатель не дочитал.
bool writeAllAndClose(int fd, const QByteArray &data, int timeoutMs);

// Асинхронное чтение до EOF в главном потоке (QSocketNotifier). fd закрывается.
// По таймауту возвращает то, что успело прийти. maxBytes защищает от бесконечного потока.
QFuture<QByteArray> readAllAsync(int fd, int timeoutMs, QObject *context,
                                 qint64 maxBytes = 512LL * 1024 * 1024);

} // namespace ps::lx
