#pragma once

#include <QHash>
#include <QObject>
#include <QSocketNotifier>

struct wl_display;
struct wl_registry;
struct wl_seat;
struct wl_interface;

class QThread;

namespace ps::lx {

struct WaylandGlobal {
    uint32_t name = 0;
    uint32_t version = 0;
};

// Собственное подключение к композитору, отдельное от подключения Qt.
//
// Протоколы data-control (и в будущем virtual-keyboard) работают без поверхностей,
// а через Qt к ним не добраться без приватного API QtWaylandClient. Отдельное
// подключение — тот же подход, что у wl-clipboard.
//
// Подключение живёт в СВОЁМ потоке, а не в главном. Причина — взаимная блокировка:
// когда наша же программа вставляет из буфера, которым владеет наш data-control-источник,
// Qt читает данные блокирующе в главном потоке, а композитор просит их у нашего
// подключения. Если его события тоже разбирает главный поток, ответить некому —
// вставка зависает до таймаута Qt и приходит пустой. См. docs/decisions/0002-*.md.
//
// Все объекты, которые пользуются подключением (слушатели протоколов), живут в его
// потоке; вызывать их методы — только через QMetaObject::invokeMethod.
class WaylandConnection : public QObject
{
    Q_OBJECT
public:
    // nullptr, если сессия не Wayland или композитор недоступен.
    // Потокобезопасно; первое обращение создаёт поток и подключается (блокирующе).
    static WaylandConnection *instance();

    wl_display *display() const { return m_display; }
    wl_registry *registry() const { return m_registry; }
    wl_seat *seat() const { return m_seat; }

    // Список глобальных объектов: потокобезопасно (снимок после первого roundtrip).
    bool hasGlobal(const char *interface) const;
    WaylandGlobal global(const char *interface) const;
    QStringList globalNames() const;

    // Остальное — только из потока подключения.
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
    // Обработчики wl_registry_listener (вызываются из libwayland)
    static void handleGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface,
                             uint32_t version);
    static void handleGlobalRemove(void *data, wl_registry *registry, uint32_t name);

private:
    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    wl_seat *m_seat = nullptr;
    QHash<QByteArray, WaylandGlobal> m_globals;
    QHash<QByteArray, WaylandGlobal> m_initialGlobals;   // неизменяемый снимок для других потоков
    QSocketNotifier *m_notifier = nullptr;
    QThread *m_thread = nullptr;
};

} // namespace ps::lx
