#pragma once

#include <QHash>
#include <QObject>
#include <QSocketNotifier>

struct wl_display;
struct wl_registry;
struct wl_seat;
struct wl_interface;

class QThread;

namespace mp::lx {

struct WaylandGlobal {
    uint32_t name = 0;
    uint32_t version = 0;
};

// Our own connection to the compositor, separate from Qt's connection.
//
// The data-control protocols (and in the future virtual-keyboard) work without surfaces,
// and through Qt they cannot be reached without QtWaylandClient's private API. A separate
// connection is the same approach as wl-clipboard's.
//
// The connection lives in ITS OWN thread, not in the main one. The reason is a deadlock:
// when our own program pastes from a clipboard owned by our data-control source,
// Qt reads the data blockingly in the main thread, and the compositor asks our
// connection for it. If the main thread also dispatches its events, nobody can answer:
// the paste hangs until Qt's timeout and comes out empty. See docs/decisions/0002-*.md.
//
// All objects that use the connection (protocol listeners) live in its
// thread; their methods are called only via QMetaObject::invokeMethod.
class WaylandConnection : public QObject
{
    Q_OBJECT
public:
    // nullptr if the session is not Wayland or the compositor is unavailable.
    // Thread-safe; the first access creates the thread and connects (blocking).
    static WaylandConnection *instance();

    wl_display *display() const { return m_display; }
    wl_registry *registry() const { return m_registry; }
    wl_seat *seat() const { return m_seat; }

    // The list of globals: thread-safe (a snapshot after the first roundtrip).
    bool hasGlobal(const char *interface) const;
    WaylandGlobal global(const char *interface) const;
    QStringList globalNames() const;

    // Everything else only from the connection thread.
    void *bind(const ::wl_interface *iface, uint32_t wantedVersion);
    void flush();
    bool roundtrip();

signals:
    void disconnected();

private:
    WaylandConnection();
    ~WaylandConnection() override;
    bool init();
    void onReadable();

public:
    // wl_registry_listener handlers (called from libwayland)
    static void handleGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface,
                             uint32_t version);
    static void handleGlobalRemove(void *data, wl_registry *registry, uint32_t name);

private:
    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    wl_seat *m_seat = nullptr;
    QHash<QByteArray, WaylandGlobal> m_globals;
    QHash<QByteArray, WaylandGlobal> m_initialGlobals;   // immutable snapshot for other threads
    QSocketNotifier *m_notifier = nullptr;
    QThread *m_thread = nullptr;
};

} // namespace mp::lx
