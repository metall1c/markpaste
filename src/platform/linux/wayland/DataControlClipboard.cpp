#include "DataControlClipboard.h"

#include "WaylandConnection.h"
#include "platform/linux/FdIo.h"

#include "ext-data-control-v1-client-protocol.h"

#include <QHash>
#include <QLoggingCategory>
#include <QMutex>
#include <QThread>
#include <QtConcurrent>

#include <wayland-client.h>

#include <fcntl.h>
#include <unistd.h>

#include <memory>

Q_DECLARE_LOGGING_CATEGORY(lcWayland)

namespace mp::lx {

// Всё, что касается протокола. Живёт и вызывается только в потоке WaylandConnection.
class DataControlDevice : public QObject
{
public:
    explicit DataControlDevice(WaylandConnection *conn);
    ~DataControlDevice() override;

    void write(ClipboardPayload payload, ClipboardFlags flags, std::shared_ptr<QPromise<Status>> promise);
    void snapshot(std::shared_ptr<QPromise<ClipboardPayload>> promise);

    // Читаются из главного потока
    bool ownsSelection() const;
    QStringList selectionMimes() const;

    // Обработчики событий протокола
    void onDataOffer(ext_data_control_offer_v1 *offer);
    void onOfferMime(ext_data_control_offer_v1 *offer, const char *mime);
    void onSelection(ext_data_control_offer_v1 *offer);
    void onPrimarySelection(ext_data_control_offer_v1 *offer);
    void onFinished();
    void onSend(ext_data_control_source_v1 *source, const char *mime, int fd);
    void onCancelled(ext_data_control_source_v1 *source);

private:
    QFuture<QByteArray> receive(const QString &mime, int timeoutMs);

    WaylandConnection *m_conn;
    ext_data_control_manager_v1 *m_manager = nullptr;
    ext_data_control_device_v1 *m_device = nullptr;

    ext_data_control_source_v1 *m_source = nullptr;   // наш текущий источник
    ClipboardPayload m_payload;                        // что он отдаёт

    QHash<ext_data_control_offer_v1 *, QStringList> m_offerMimes;   // ещё не ставшие selection
    ext_data_control_offer_v1 *m_selection = nullptr;

    mutable QMutex m_mutex;          // защищает два поля ниже
    QStringList m_selectionMimes;
    bool m_owns = false;
};

namespace {

DataControlDevice *self(void *data) { return static_cast<DataControlDevice *>(data); }

const ext_data_control_offer_v1_listener kOfferListener = {
    .offer = [](void *data, ext_data_control_offer_v1 *offer, const char *mime) { self(data)->onOfferMime(offer, mime); },
};

const ext_data_control_device_v1_listener kDeviceListener = {
    .data_offer = [](void *data, ext_data_control_device_v1 *, ext_data_control_offer_v1 *offer) {
        self(data)->onDataOffer(offer);
    },
    .selection = [](void *data, ext_data_control_device_v1 *, ext_data_control_offer_v1 *offer) {
        self(data)->onSelection(offer);
    },
    .finished = [](void *data, ext_data_control_device_v1 *) { self(data)->onFinished(); },
    .primary_selection = [](void *data, ext_data_control_device_v1 *, ext_data_control_offer_v1 *offer) {
        self(data)->onPrimarySelection(offer);
    },
};

const ext_data_control_source_v1_listener kSourceListener = {
    .send = [](void *data, ext_data_control_source_v1 *source, const char *mime, int32_t fd) {
        self(data)->onSend(source, mime, fd);
    },
    .cancelled = [](void *data, ext_data_control_source_v1 *source) { self(data)->onCancelled(source); },
};

// Что восстанавливаем после вставки: только текст и картинки (ТЗ разд. 10).
const QStringList kTextMimes = {QStringLiteral("text/plain;charset=utf-8"), QStringLiteral("text/plain"),
                                QStringLiteral("UTF8_STRING")};
const QStringList kImageMimes = {QStringLiteral("image/png"), QStringLiteral("image/jpeg"),
                                 QStringLiteral("image/bmp")};
// Сколько ждать данные чужого буфера при сохранении. Картинку владелец часто кодирует по
// запросу (браузер, редактор): 4K PNG — до секунды и больше. Не дождались — не восстанавливаем.
constexpr int kTextReadMs = 1000;
constexpr int kImageReadMs = 3000;

} // namespace

// --- DataControlDevice ---

DataControlDevice::DataControlDevice(WaylandConnection *conn)
    : m_conn(conn)
{
    m_manager = static_cast<ext_data_control_manager_v1 *>(conn->bind(&ext_data_control_manager_v1_interface, 1));
    m_device = ext_data_control_manager_v1_get_data_device(m_manager, conn->seat());
    ext_data_control_device_v1_add_listener(m_device, &kDeviceListener, this);
    // Первый roundtrip приносит текущее содержимое буфера.
    conn->roundtrip();
}

DataControlDevice::~DataControlDevice()
{
    if (m_source)
        ext_data_control_source_v1_destroy(m_source);
    for (auto *offer : m_offerMimes.keys())
        ext_data_control_offer_v1_destroy(offer);
    if (m_selection)
        ext_data_control_offer_v1_destroy(m_selection);
    if (m_device)
        ext_data_control_device_v1_destroy(m_device);
    if (m_manager)
        ext_data_control_manager_v1_destroy(m_manager);
    m_conn->flush();
}

bool DataControlDevice::ownsSelection() const
{
    QMutexLocker lock(&m_mutex);
    return m_owns;
}

QStringList DataControlDevice::selectionMimes() const
{
    QMutexLocker lock(&m_mutex);
    return m_selectionMimes;
}

void DataControlDevice::onDataOffer(ext_data_control_offer_v1 *offer)
{
    m_offerMimes.insert(offer, {});
    ext_data_control_offer_v1_add_listener(offer, &kOfferListener, this);
}

void DataControlDevice::onOfferMime(ext_data_control_offer_v1 *offer, const char *mime)
{
    auto it = m_offerMimes.find(offer);
    if (it != m_offerMimes.end())
        it->append(QString::fromUtf8(mime));
}

void DataControlDevice::onSelection(ext_data_control_offer_v1 *offer)
{
    if (m_selection)
        ext_data_control_offer_v1_destroy(m_selection);
    m_selection = offer;
    const QStringList mimes = offer ? m_offerMimes.take(offer) : QStringList();
    QMutexLocker lock(&m_mutex);
    m_selectionMimes = mimes;
}

void DataControlDevice::onPrimarySelection(ext_data_control_offer_v1 *offer)
{
    // Первичное выделение (средняя кнопка мыши) не трогаем.
    if (offer) {
        m_offerMimes.remove(offer);
        ext_data_control_offer_v1_destroy(offer);
    }
}

void DataControlDevice::onFinished()
{
    qCWarning(lcWayland) << "data-control device finished (seat removed?)";
    ext_data_control_device_v1_destroy(m_device);
    m_device = nullptr;
}

void DataControlDevice::onSend(ext_data_control_source_v1 *source, const char *mime, int fd)
{
    const ClipboardEntry *entry = source == m_source ? m_payload.find(QString::fromUtf8(mime)) : nullptr;
    if (!entry) {
        ::close(fd);
        return;
    }
    // Пишем в пуле потоков: получатель читает сколько хочет (картинка — мегабайты),
    // а поток Wayland должен оставаться свободным для следующих запросов.
    const QByteArray data = entry->data;
    (void)QtConcurrent::run([fd, data] { writeAllAndClose(fd, data, 5000); });
}

void DataControlDevice::onCancelled(ext_data_control_source_v1 *source)
{
    ext_data_control_source_v1_destroy(source);
    if (source == m_source) {
        m_source = nullptr;
        m_payload = {};
        QMutexLocker lock(&m_mutex);
        m_owns = false;
    }
    m_conn->flush();
}

void DataControlDevice::write(ClipboardPayload payload, ClipboardFlags flags,
                              std::shared_ptr<QPromise<Status>> promise)
{
    if (!m_device) {
        promise->addResult(Status::failure(QStringLiteral("data-control device is not available")));
        promise->finish();
        return;
    }
    if (flags & ClipboardFlag::ExcludeFromHistory) {
        // Klipper не сохраняет записи с этой подсказкой (ТЗ разд. 10).
        payload.entries.append({QStringLiteral("x-kde-passwordManagerHint"), QByteArrayLiteral("secret")});
    }

    auto *source = ext_data_control_manager_v1_create_data_source(m_manager);
    ext_data_control_source_v1_add_listener(source, &kSourceListener, this);
    for (const ClipboardEntry &e : payload.entries)
        ext_data_control_source_v1_offer(source, e.mimeType.toUtf8().constData());
    ext_data_control_device_v1_set_selection(m_device, source);

    // Прежнему источнику композитор пришлёт cancelled — там он и удалится.
    m_source = source;
    m_payload = std::move(payload);
    {
        QMutexLocker lock(&m_mutex);
        m_owns = true;
    }

    // Готово, когда композитор обработал set_selection: ждём ответ на wl_display.sync.
    auto *holder = new std::shared_ptr<QPromise<Status>>(std::move(promise));
    static const wl_callback_listener kDone = {
        .done = [](void *data, wl_callback *cb, uint32_t) {
            auto *h = static_cast<std::shared_ptr<QPromise<Status>> *>(data);
            (*h)->addResult(Status::success());
            (*h)->finish();
            delete h;
            wl_callback_destroy(cb);
        },
    };
    wl_callback_add_listener(wl_display_sync(m_conn->display()), &kDone, holder);
    m_conn->flush();
}

QFuture<QByteArray> DataControlDevice::receive(const QString &mime, int timeoutMs)
{
    int fds[2];
    if (::pipe2(fds, O_CLOEXEC) != 0)
        return readyFuture(QByteArray());
    ext_data_control_offer_v1_receive(m_selection, mime.toUtf8().constData(), fds[1]);
    m_conn->flush();
    ::close(fds[1]);
    return readAllAsync(fds[0], timeoutMs, this);
}

void DataControlDevice::snapshot(std::shared_ptr<QPromise<ClipboardPayload>> promise)
{
    auto finishWith = [promise](const ClipboardPayload &p) {
        promise->addResult(p);
        promise->finish();
    };
    if (m_source)   // буфер наш — данные уже есть
        return finishWith(m_payload);
    if (!m_selection)
        return finishWith({});

    const QStringList mimes = selectionMimes();
    QString textMime, imageMime;
    for (const QString &m : kTextMimes)
        if (mimes.contains(m)) {
            textMime = m;
            break;
        }
    for (const QString &m : kImageMimes)
        if (mimes.contains(m)) {
            imageMime = m;
            break;
        }

    auto result = std::make_shared<ClipboardPayload>();
    auto readImage = [this, imageMime, result, finishWith] {
        if (imageMime.isEmpty())
            return finishWith(*result);
        receive(imageMime, kImageReadMs).then(this, [imageMime, result, finishWith](const QByteArray &data) {
            if (!data.isEmpty())
                result->addImage(data, imageMime.toLatin1());
            finishWith(*result);
        });
    };
    if (textMime.isEmpty())
        return readImage();
    receive(textMime, kTextReadMs).then(this, [result, readImage](const QByteArray &data) {
        if (!data.isEmpty())
            result->addText(QString::fromUtf8(data));
        readImage();
    });
}

// --- DataControlClipboard (фасад) ---

DataControlClipboard *DataControlClipboard::create(QObject *parent)
{
    WaylandConnection *conn = WaylandConnection::instance();
    if (!conn || !conn->seat() || !conn->hasGlobal(ext_data_control_manager_v1_interface.name))
        return nullptr;
    DataControlDevice *device = nullptr;
    QMetaObject::invokeMethod(conn, [conn, &device] {
        device = new DataControlDevice(conn);
    }, Qt::BlockingQueuedConnection);
    return new DataControlClipboard(device, parent);
}

DataControlClipboard::DataControlClipboard(DataControlDevice *device, QObject *parent)
    : IClipboard(parent), m_device(device)
{
}

DataControlClipboard::~DataControlClipboard()
{
    // Удаляем в его потоке, синхронно: после нас никто не должен трогать устройство.
    DataControlDevice *device = m_device;
    QMetaObject::invokeMethod(device, [device] { delete device; }, Qt::BlockingQueuedConnection);
}

QFuture<Status> DataControlClipboard::write(const ClipboardPayload &payload, ClipboardFlags flags)
{
    auto promise = std::make_shared<QPromise<Status>>();
    promise->start();
    QFuture<Status> future = promise->future();
    DataControlDevice *device = m_device;
    QMetaObject::invokeMethod(device, [device, payload, flags, promise] { device->write(payload, flags, promise); },
                              Qt::QueuedConnection);
    return future;
}

QFuture<ClipboardPayload> DataControlClipboard::snapshot()
{
    auto promise = std::make_shared<QPromise<ClipboardPayload>>();
    promise->start();
    QFuture<ClipboardPayload> future = promise->future();
    DataControlDevice *device = m_device;
    QMetaObject::invokeMethod(device, [device, promise] { device->snapshot(promise); }, Qt::QueuedConnection);
    return future;
}

bool DataControlClipboard::ownsSelection() const
{
    return m_device->ownsSelection();
}

QStringList DataControlClipboard::currentMimeTypes() const
{
    return m_device->selectionMimes();
}

} // namespace mp::lx
