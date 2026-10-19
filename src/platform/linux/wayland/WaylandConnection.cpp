#include "WaylandConnection.h"

#include <QLoggingCategory>
#include <QThread>

#include <wayland-client.h>

#include <algorithm>
#include <cerrno>

Q_LOGGING_CATEGORY(lcWayland, "pointshot.wayland")

namespace ps::lx {

static const wl_registry_listener kRegistryListener = {
    &WaylandConnection::handleGlobal,
    &WaylandConnection::handleGlobalRemove,
};

WaylandConnection *WaylandConnection::instance()
{
    static WaylandConnection *conn = [] {
        if (qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") && qEnvironmentVariable("XDG_SESSION_TYPE") != u"wayland")
            return static_cast<WaylandConnection *>(nullptr);

        auto *thread = new QThread;
        thread->setObjectName(QStringLiteral("pointshot-wayland"));
        auto *c = new WaylandConnection;
        c->m_thread = thread;
        c->moveToThread(thread);
        thread->start();

        bool ok = false;
        QMetaObject::invokeMethod(c, [c, &ok] { ok = c->init(); }, Qt::BlockingQueuedConnection);
        if (!ok) {
            thread->quit();
            thread->wait();
            delete thread;
            // Объект уже в остановленном потоке: удаляем напрямую, обработчиков у него нет.
            delete c;
            return static_cast<WaylandConnection *>(nullptr);
        }
        // Поток живёт до конца процесса: композитор может прислать запрос данных в любой момент.
        return c;
    }();
    return conn;
}

WaylandConnection::WaylandConnection() = default;

WaylandConnection::~WaylandConnection()
{
    delete m_notifier;
    if (m_seat)
        wl_seat_destroy(m_seat);
    if (m_registry)
        wl_registry_destroy(m_registry);
    if (m_display)
        wl_display_disconnect(m_display);
}

bool WaylandConnection::init()
{
    m_display = wl_display_connect(nullptr);
    if (!m_display) {
        qCWarning(lcWayland) << "wl_display_connect failed";
        return false;
    }
    m_registry = wl_display_get_registry(m_display);
    wl_registry_add_listener(m_registry, &kRegistryListener, this);
    if (wl_display_roundtrip(m_display) < 0)
        return false;
    m_initialGlobals = m_globals;

    // Первое сиденье. Несколько сидений на десктопе — редкость, поддержим при необходимости.
    if (m_globals.contains("wl_seat"))
        m_seat = static_cast<wl_seat *>(bind(&wl_seat_interface, 1));

    m_notifier = new QSocketNotifier(wl_display_get_fd(m_display), QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &WaylandConnection::onReadable);
    qCInfo(lcWayland) << "own Wayland connection ready," << m_globals.size() << "globals";
    return true;
}

void WaylandConnection::onReadable()
{
    // Стандартная схема libwayland для внешнего цикла событий.
    if (wl_display_prepare_read(m_display) == 0) {
        if (wl_display_read_events(m_display) < 0 && errno != EAGAIN) {
            qCWarning(lcWayland) << "connection to compositor lost";
            m_notifier->setEnabled(false);
            emit disconnected();
            return;
        }
    }
    wl_display_dispatch_pending(m_display);
    wl_display_flush(m_display);
}

void WaylandConnection::handleGlobal(void *data, wl_registry *, uint32_t name, const char *interface,
                                     uint32_t version)
{
    auto *self = static_cast<WaylandConnection *>(data);
    self->m_globals.insert(QByteArray(interface), WaylandGlobal{name, version});
}

void WaylandConnection::handleGlobalRemove(void *data, wl_registry *, uint32_t name)
{
    auto *self = static_cast<WaylandConnection *>(data);
    for (auto it = self->m_globals.begin(); it != self->m_globals.end(); ++it) {
        if (it->name == name) {
            self->m_globals.erase(it);
            return;
        }
    }
}

bool WaylandConnection::hasGlobal(const char *interface) const
{
    return m_initialGlobals.contains(QByteArray(interface));
}

WaylandGlobal WaylandConnection::global(const char *interface) const
{
    return m_initialGlobals.value(QByteArray(interface));
}

QStringList WaylandConnection::globalNames() const
{
    QStringList names;
    for (auto it = m_initialGlobals.cbegin(); it != m_initialGlobals.cend(); ++it)
        names << QString::fromLatin1(it.key());
    names.sort();
    return names;
}

void *WaylandConnection::bind(const wl_interface *iface, uint32_t wantedVersion)
{
    Q_ASSERT(QThread::currentThread() == thread());
    const WaylandGlobal g = m_globals.value(QByteArray(iface->name));
    if (g.name == 0)
        return nullptr;
    return wl_registry_bind(m_registry, g.name, iface, std::min(wantedVersion, g.version));
}

void WaylandConnection::flush()
{
    wl_display_flush(m_display);
}

bool WaylandConnection::roundtrip()
{
    return wl_display_roundtrip(m_display) >= 0;
}

} // namespace ps::lx
