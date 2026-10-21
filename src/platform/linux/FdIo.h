#pragma once

#include <QByteArray>
#include <QFuture>
#include <QObject>

namespace mp::lx {

// Blocking write of the whole buffer with an overall timeout (poll), then close(fd).
// Call from a worker thread. Returns false if the receiver did not read everything.
bool writeAllAndClose(int fd, const QByteArray &data, int timeoutMs);

// Asynchronous read until EOF in the context's thread (QSocketNotifier). fd is closed.
// Data only if it arrived completely: on timeout, read error or over maxBytes
// the result is empty. maxBytes protects against an endless stream.
QFuture<QByteArray> readAllAsync(int fd, int timeoutMs, QObject *context,
                                 qint64 maxBytes = 512LL * 1024 * 1024);

} // namespace mp::lx
