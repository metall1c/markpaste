#include "FdIo.h"

#include <QElapsedTimer>
#include <QPromise>
#include <QSocketNotifier>
#include <QTimer>

#include <cerrno>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <memory>

namespace mp::lx {

bool writeAllAndClose(int fd, const QByteArray &data, int timeoutMs)
{
    // Non-blocking mode + poll: this way the overall timeout holds even if the receiver hangs.
    ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK);
    QElapsedTimer timer;
    timer.start();
    const char *p = data.constData();
    qint64 left = data.size();
    bool ok = true;
    while (left > 0) {
        const ssize_t n = ::write(fd, p, size_t(left));
        if (n > 0) {
            p += n;
            left -= n;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && errno == EAGAIN) {
            const qint64 remaining = timeoutMs - timer.elapsed();
            if (remaining <= 0) {
                ok = false;
                break;
            }
            pollfd pfd{fd, POLLOUT, 0};
            ::poll(&pfd, 1, int(remaining));
            continue;
        }
        ok = false;   // EPIPE: the receiver closed its end
        break;
    }
    ::close(fd);
    return ok;
}

QFuture<QByteArray> readAllAsync(int fd, int timeoutMs, QObject *context, qint64 maxBytes)
{
    ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL) | O_NONBLOCK);

    struct State {
        QPromise<QByteArray> promise;
        QByteArray data;
        int fd = -1;
        bool done = false;
        QSocketNotifier *notifier = nullptr;
        QTimer *timer = nullptr;
    };
    auto st = std::make_shared<State>();
    st->fd = fd;
    st->promise.start();

    // complete = false: a timeout or maxBytes exceeded; do not hand out a fragment. Otherwise after a paste
    // a truncated image or text would be restored to the clipboard (docs/problems.md).
    auto finish = [st](bool complete) {
        if (st->done)
            return;
        st->done = true;
        st->notifier->setEnabled(false);
        st->notifier->deleteLater();
        st->timer->stop();
        st->timer->deleteLater();
        ::close(st->fd);
        st->promise.addResult(complete ? st->data : QByteArray());
        st->promise.finish();
    };

    st->notifier = new QSocketNotifier(fd, QSocketNotifier::Read, context);
    st->timer = new QTimer(context);
    st->timer->setSingleShot(true);
    QObject::connect(st->timer, &QTimer::timeout, st->notifier, [finish] { finish(false); });
    QObject::connect(st->notifier, &QSocketNotifier::activated, st->notifier, [st, finish, maxBytes] {
        char buf[65536];
        for (;;) {
            const ssize_t n = ::read(st->fd, buf, sizeof buf);
            if (n > 0) {
                st->data.append(buf, n);
                if (st->data.size() > maxBytes)
                    return finish(false);
                continue;
            }
            if (n < 0 && errno == EINTR)
                continue;
            if (n < 0 && errno == EAGAIN)
                return;   // wait for the next chunk
            return finish(n == 0);   // EOF means everything was read; a read error means a fragment
        }
    });
    if (timeoutMs > 0)
        st->timer->start(timeoutMs);
    return st->promise.future();
}

} // namespace mp::lx
