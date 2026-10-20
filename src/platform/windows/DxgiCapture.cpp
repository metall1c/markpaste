#include "DxgiCapture.h"

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QScreen>
#include <QTransform>
#include <QtConcurrent>

#include <d3d11.h>
#include <dxgi1_5.h>
#include <windows.h>

#include <cstring>
#include <utility>
#include <vector>

Q_LOGGING_CATEGORY(lcDxgi, "markpaste.win.capture")

namespace mp::win {

namespace {

// Владение COM-указателем без WRL: Release при выходе из области видимости
template <class T>
class Com
{
public:
    Com() = default;
    Com(const Com &) = delete;
    Com &operator=(const Com &) = delete;
    Com(Com &&other) noexcept : m_p(std::exchange(other.m_p, nullptr)) {}
    ~Com() { reset(); }
    void reset()
    {
        if (m_p)
            m_p->Release();
        m_p = nullptr;
    }
    T **put()
    {
        reset();
        return &m_p;
    }
    void **putVoid() { return reinterpret_cast<void **>(put()); }
    T *get() const { return m_p; }
    T *operator->() const { return m_p; }
    explicit operator bool() const { return m_p != nullptr; }

private:
    T *m_p = nullptr;
};

QString hr(HRESULT code)
{
    return QStringLiteral("0x%1").arg(quint32(code), 8, 16, QLatin1Char('0'));
}

// BGRA строки с шагом pitch → QImage. Альфа рабочего стола не определена — ставим 255.
QImage fromBgra(const uchar *data, int pitch, int width, int height)
{
    QImage img(width, height, QImage::Format_RGB32);
    if (img.isNull())
        return img;
    for (int y = 0; y < height; ++y) {
        auto *dst = reinterpret_cast<quint32 *>(img.scanLine(y));
        std::memcpy(dst, data + qsizetype(y) * pitch, size_t(width) * 4);
        for (int x = 0; x < width; ++x)
            dst[x] |= 0xff000000u;
    }
    return img;
}

bool looksBlack(const QImage &img)
{
    // Первый кадр дублирования на некоторых драйверах пустой: проверяем сетку точек
    for (int y = 0; y < img.height(); y += std::max(1, img.height() / 16))
        for (int x = 0; x < img.width(); x += std::max(1, img.width() / 16))
            if ((img.pixel(x, y) & 0x00ffffffu) != 0)
                return false;
    return true;
}

QImage dxgiGrab(IDXGIAdapter1 *adapter, IDXGIOutput *output, QString *error)
{
    Com<ID3D11Device> device;
    Com<ID3D11DeviceContext> context;
    HRESULT h = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr,
                                  0, D3D11_SDK_VERSION, device.put(), nullptr, context.put());
    if (FAILED(h)) {
        *error = QStringLiteral("D3D11CreateDevice: %1").arg(hr(h));
        return {};
    }

    Com<IDXGIOutputDuplication> dup;
    // Windows 10 1703+: формат задаём сами — HDR система переведёт в 8 бит
    Com<IDXGIOutput5> out5;
    if (SUCCEEDED(output->QueryInterface(__uuidof(IDXGIOutput5), out5.putVoid()))) {
        const DXGI_FORMAT formats[] = {DXGI_FORMAT_B8G8R8A8_UNORM};
        h = out5->DuplicateOutput1(device.get(), 0, UINT(std::size(formats)), formats, dup.put());
    }
    if (!dup) {
        Com<IDXGIOutput1> out1;
        h = output->QueryInterface(__uuidof(IDXGIOutput1), out1.putVoid());
        if (SUCCEEDED(h))
            h = out1->DuplicateOutput(device.get(), dup.put());
    }
    if (FAILED(h) || !dup) {
        // E_ACCESSDENIED — экран блокировки или окно UAC; DXGI_ERROR_UNSUPPORTED — драйвер не умеет
        *error = QStringLiteral("DuplicateOutput: %1").arg(hr(h));
        return {};
    }
    DXGI_OUTDUPL_DESC desc;
    dup->GetDesc(&desc);

    QImage image;
    for (int attempt = 0; attempt < 2; ++attempt) {
        DXGI_OUTDUPL_FRAME_INFO info;
        Com<IDXGIResource> resource;
        h = dup->AcquireNextFrame(attempt == 0 ? 500 : 100, &info, resource.put());
        if (h == DXGI_ERROR_WAIT_TIMEOUT)
            break;   // новых кадров нет — остаётся тот, что уже есть
        if (FAILED(h)) {
            *error = QStringLiteral("AcquireNextFrame: %1").arg(hr(h));
            return {};
        }
        Com<ID3D11Texture2D> frame;
        resource->QueryInterface(__uuidof(ID3D11Texture2D), frame.putVoid());
        D3D11_TEXTURE2D_DESC td{};
        if (frame)
            frame->GetDesc(&td);
        if (!frame || (td.Format != DXGI_FORMAT_B8G8R8A8_UNORM && td.Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)) {
            dup->ReleaseFrame();
            *error = QStringLiteral("unexpected frame format %1").arg(int(td.Format));
            return {};
        }
        D3D11_TEXTURE2D_DESC sd = td;
        sd.Usage = D3D11_USAGE_STAGING;
        sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        sd.BindFlags = 0;
        sd.MiscFlags = 0;
        sd.MipLevels = 1;
        sd.ArraySize = 1;
        Com<ID3D11Texture2D> staging;
        h = device->CreateTexture2D(&sd, nullptr, staging.put());
        if (FAILED(h)) {
            dup->ReleaseFrame();
            *error = QStringLiteral("CreateTexture2D: %1").arg(hr(h));
            return {};
        }
        context->CopyResource(staging.get(), frame.get());
        dup->ReleaseFrame();
        D3D11_MAPPED_SUBRESOURCE mapped;
        h = context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped);
        if (FAILED(h)) {
            *error = QStringLiteral("Map: %1").arg(hr(h));
            return {};
        }
        image = fromBgra(static_cast<const uchar *>(mapped.pData), int(mapped.RowPitch), int(td.Width), int(td.Height));
        context->Unmap(staging.get(), 0);
        if (info.LastPresentTime.QuadPart != 0 || !looksBlack(image))
            break;
    }
    if (image.isNull()) {
        *error = QStringLiteral("no frame");
        return {};
    }
    // Поверхность дублирования — в собственной ориентации панели, без поворота экрана
    switch (desc.Rotation) {
    case DXGI_MODE_ROTATION_ROTATE90: image = image.transformed(QTransform().rotate(90)); break;
    case DXGI_MODE_ROTATION_ROTATE180: image = image.transformed(QTransform().rotate(180)); break;
    case DXGI_MODE_ROTATION_ROTATE270: image = image.transformed(QTransform().rotate(270)); break;
    default: break;
    }
    return image;
}

struct Output {
    Com<IDXGIAdapter1> adapter;
    Com<IDXGIOutput> output;
    QString deviceName;
    QRect desktop;   // физические пиксели
};

} // namespace

QImage DxgiCapture::gdiGrab(const QRect &r)
{
    if (r.isEmpty())
        return {};
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = r.width();
    bi.bmiHeader.biHeight = -r.height();   // сверху вниз
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void *bits = nullptr;
    HBITMAP bmp = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    QImage img;
    if (bmp && bits) {
        HGDIOBJ old = SelectObject(mem, bmp);
        // CAPTUREBLT — вместе с многослойными окнами (подсказки, меню)
        if (BitBlt(mem, 0, 0, r.width(), r.height(), screen, r.x(), r.y(), SRCCOPY | CAPTUREBLT)) {
            GdiFlush();
            img = fromBgra(static_cast<const uchar *>(bits), r.width() * 4, r.width(), r.height());
        }
        SelectObject(mem, old);
    }
    if (bmp)
        DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return img;
}

CaptureResult DxgiCapture::grab(const QList<ScreenInfo> &screens, bool useDxgi)
{
    std::vector<Output> outputs;
    Com<IDXGIFactory1> factory;
    if (useDxgi && SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), factory.putVoid()))) {
        for (UINT a = 0;; ++a) {
            Com<IDXGIAdapter1> adapter;
            if (factory->EnumAdapters1(a, adapter.put()) == DXGI_ERROR_NOT_FOUND)
                break;
            for (UINT o = 0;; ++o) {
                Output out;
                if (adapter->EnumOutputs(o, out.output.put()) == DXGI_ERROR_NOT_FOUND)
                    break;
                DXGI_OUTPUT_DESC d;
                if (FAILED(out.output->GetDesc(&d)) || !d.AttachedToDesktop)
                    continue;
                adapter->AddRef();
                *out.adapter.put() = adapter.get();
                out.deviceName = QString::fromWCharArray(d.DeviceName);
                const RECT &dc = d.DesktopCoordinates;
                out.desktop = QRect(QPoint(dc.left, dc.top), QPoint(dc.right - 1, dc.bottom - 1));
                outputs.push_back(std::move(out));
            }
        }
    }

    CaptureResult result;
    QStringList errors;
    for (const ScreenInfo &s : screens) {
        QElapsedTimer t;
        t.start();
        // Монитор ищем по имени устройства (\\.\DISPLAY1), а если Qt назвал его иначе — по левому
        // верхнему углу: Qt сохраняет его в физических координатах и при разном масштабе экранов
        const Output *match = nullptr;
        for (const Output &o : outputs)
            if (o.deviceName == s.name)
                match = &o;
        if (!match)
            for (const Output &o : outputs)
                if (o.desktop.topLeft() == s.logical.topLeft())
                    match = &o;

        ScreenFrame f;
        f.screenName = s.name;
        f.logicalGeometry = s.logical;
        f.scale = s.scale;
        QString error = useDxgi ? QStringLiteral("no DXGI output") : QStringLiteral("DXGI is off");
        const char *method = "DXGI";
        if (match)
            f.image = dxgiGrab(match->adapter.get(), match->output.get(), &error);
        if (f.image.isNull()) {
            method = "GDI";
            const QRect physical = match ? match->desktop
                                         : QRect(s.logical.topLeft(), (QSizeF(s.logical.size()) * s.scale).toSize());
            if (useDxgi)
                qCInfo(lcDxgi) << s.name << "DXGI failed:" << error << "— using GDI";
            f.image = gdiGrab(physical);
        }
        if (f.image.isNull()) {
            errors << QStringLiteral("%1: %2; GDI BitBlt failed").arg(s.name, error);
            continue;
        }
        qCInfo(lcDxgi) << s.name << method << f.image.size() << t.elapsed() << "ms";
        result.frames.append(f);
    }
    if (result.frames.isEmpty())
        result.status = Status::failure(errors.isEmpty() ? QStringLiteral("no screens") : errors.join(u'\n'));
    return result;
}

QFuture<CaptureResult> DxgiCapture::captureAllScreens()
{
    QList<ScreenInfo> screens;
    for (QScreen *s : QGuiApplication::screens())
        screens << ScreenInfo{s->name(), s->geometry(), s->devicePixelRatio()};
    return QtConcurrent::run([screens, dxgi = m_useDxgi] { return grab(screens, dxgi); });
}

} // namespace mp::win
