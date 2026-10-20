// Замеры NFR-1 и NFR-2 без настоящего экрана 4K: память набора из 26 снимков 4K, время
// вставки 5 снимков (запекание + задержки профиля), время до первого кадра оверлея на 2×4K.
// Платформа — заглушки: буфер и Ctrl+V мгновенные, поэтому время вставки — это наше
// запекание плюс паузы профиля, без задержек самого чата.
//
//   markpaste-bench [--shots 26] [--paste 5]       (QT_QPA_PLATFORM=offscreen ставится сам)

#include "../../tests/FakePlatform.h"
#include "core/CaptureSetModel.h"
#include "paste/PasteOrchestrator.h"
#include "platform/Memory.h"
#include "ui/CaptureOverlay.h"
#include "ui/ShotEditor.h"
#include "ui/StripWindow.h"

#include <QApplication>
#include <QBuffer>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QFile>
#include <QPainter>
#include <QRandomGenerator>
#include <QTest>
#include <QtConcurrent>

#include <cstdio>

using namespace mp;

namespace {

// VmRSS и VmHWM из /proc/self/status, МБ
std::pair<double, double> memoryMb()
{
    QFile f(QStringLiteral("/proc/self/status"));
    double rss = 0, hwm = 0;
    if (f.open(QIODevice::ReadOnly))
        for (const QByteArray &line : f.readAll().split('\n')) {
            const QList<QByteArray> parts = line.simplified().split(' ');
            if (parts.size() >= 2 && parts[0] == "VmRSS:")
                rss = parts[1].toDouble() / 1024;
            if (parts.size() >= 2 && parts[0] == "VmHWM:")
                hwm = parts[1].toDouble() / 1024;
        }
    return {rss, hwm};
}

void report(const char *what)
{
    const auto [rss, hwm] = memoryMb();
    std::printf("%-44s RSS %7.1f MB   peak %7.1f MB\n", what, rss, hwm);
    std::fflush(stdout);
}

// Похоже на снимок экрана, а не на шум: панели, текст, кнопки, градиент и фото-вставка.
// Шум сжимается плохо и завысил бы память, однотонный фон — занизил бы.
QImage screenLike(int w, int h, int seed)
{
    QImage img(w, h, QImage::Format_ARGB32);
    QRandomGenerator rng(seed);
    img.fill(QColor(0xF4, 0xF5, 0xF7));
    QPainter p(&img);
    p.fillRect(0, 0, w, 96, QColor(0x2B, 0x2F, 0x36));
    p.fillRect(0, 96, 420, h - 96, QColor(0xE8, 0xEA, 0xED));
    QFont f = p.font();
    f.setPixelSize(28);
    p.setFont(f);
    for (int y = 140; y < h - 40; y += 44) {
        p.setPen(QColor(0x30, 0x30, 0x30));
        QString line;
        const int words = 4 + rng.bounded(14);
        for (int i = 0; i < words; ++i)
            line += QString(QChar(u'а' + rng.bounded(32))).repeated(2 + rng.bounded(8)) + u' ';
        p.drawText(460, y, line);
        if (y < h / 2)
            p.drawText(30, y, line.left(18));
    }
    for (int i = 0; i < 8; ++i)
        p.fillRect(QRect(460 + i * 300, h - 220, 260, 70), QColor::fromHsv(rng.bounded(360), 160, 220));
    // «Фотография» — плохо сжимаемая область, как картинка на странице
    QImage photo(900, 600, QImage::Format_RGB32);
    for (int y = 0; y < photo.height(); ++y)
        for (int x = 0; x < photo.width(); ++x)
            photo.setPixel(x, y, qRgb((x + rng.bounded(40)) % 256, (y + rng.bounded(40)) % 256, (x + y) % 256));
    p.drawImage(w - 1000, 200, photo);
    p.end();
    return img;
}

QByteArray toPng(const QImage &img)
{
    QByteArray png;
    QBuffer buf(&png);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "PNG", 50);   // как AppController::onCaptured
    return png;
}

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    QCommandLineParser cli;
    cli.addHelpOption();
    QCommandLineOption shotsOpt(QStringLiteral("shots"), QStringLiteral("Shots in the set"), QStringLiteral("n"),
                                QStringLiteral("26"));
    QCommandLineOption pasteOpt(QStringLiteral("paste"), QStringLiteral("Shots to paste"), QStringLiteral("n"),
                                QStringLiteral("5"));
    cli.addOptions({shotsOpt, pasteOpt});
    cli.process(app);
    const int shots = std::clamp(cli.value(shotsOpt).toInt(), 1, kMaxShots);
    const int pasteCount = std::clamp(cli.value(pasteOpt).toInt(), 1, shots);
    constexpr int W = 3840, H = 2160;

    report("start (QApplication)");

    // --- NFR-1: оверлей на двух мониторах 4K ---
    {
        QList<ScreenFrame> frames;
        for (int i = 0; i < 2; ++i)
            frames << ScreenFrame{QStringLiteral("bench-%1").arg(i), QRect(i * W, 0, W, H), 1.0, screenLike(W, H, i)};
        // Время до оверлея здесь не мерим: без настоящего композитора окно не рисуется на экран,
        // цифры — из журнала программы на живой сессии (overlay shown … ms). Здесь — память.
        auto *session = new CaptureSession(frames, true);
        session->start();
        QApplication::processEvents();
        report("with overlay 2x4K open");
        delete session;
        QApplication::processEvents();
    }

    // --- NFR-2: набор из N снимков 4K ---
    CaptureSetModel model;
    StripWindow strip(&model);
    strip.show();
    ShotEditor editor(&model);
    QElapsedTimer encode;
    encode.start();
    qint64 pngBytes = 0;
    for (int i = 0; i < shots; ++i) {
        Shot s;
        const QImage img = screenLike(W, H, 100 + i);
        s.sourcePng = toPng(img);
        s.sourceSize = img.size();
        pngBytes += s.sourcePng.size();
        const QUuid id = model.addShot(s);
        model.addMark(id, QRect(500, 300, 800, 200), QStringLiteral("Комментарий %1").arg(i + 1));
        model.addLinearMark(id, MarkShape::Arrow, {{2000, 1500}, {1400, 900}});
        if (i == 0) {
            editor.showShot(id);
            editor.resize(1600, 1000);
        }
        QApplication::processEvents();
    }
    std::printf("%d shots 4K: PNG in memory %.1f MB (%.1f MB per shot), encode %lld ms per shot\n", shots,
                pngBytes / 1048576.0, pngBytes / 1048576.0 / shots, encode.elapsed() / shots);
    report("set + strip + editor (one shot open)");

    // --- NFR-1: вставка N снимков ---
    test::FakeClipboard clipboard;
    test::FakeInjector injector(&clipboard);
    PasteOrchestrator paste(&clipboard, &injector);
    CaptureSet subset;
    subset.shots = model.set().shots.mid(0, pasteCount);
    PasteOptions opt;
    opt.messageTemplate = MessageTemplate::defaultFor(QLocale(QLocale::Russian));
    releaseFreedMemory();
    // Пик RSS — заново, чтобы видеть пик самой вставки (Linux: запись «5» в clear_refs)
    if (QFile refs(QStringLiteral("/proc/self/clear_refs")); refs.open(QIODevice::WriteOnly))
        refs.write("5");
    report("before paste (peak reset)");
    QElapsedTimer t;
    bool done = false;
    QObject::connect(&paste, &PasteOrchestrator::finished, [&](const Status &s, PasteMode) {
        done = true;
        if (!s.ok)
            std::printf("paste failed: %s\n", qPrintable(s.error));
    });
    t.start();
    paste.start(subset, opt);
    if (!QTest::qWaitFor([&] { return done; }, 60000))
        std::printf("paste timed out\n");
    const qint64 total = t.elapsed();
    const PasteTiming &tm = opt.timing;
    report("after paste");
    QElapsedTimer bake;
    bake.start();
    const auto items = PasteOrchestrator::bakeSequence(subset, opt);
    const qint64 bakeMs = bake.elapsed();
    std::printf("paste %d shots + text: %lld ms total (bake %lld ms for %lld items, the rest — profile delays;"
                " restore of the clipboard after %d ms is included)\n",
                pasteCount, total, bakeMs, qlonglong(items.size()), tm.restoreAfterMs);
    std::printf("  until the last Ctrl+V: about %lld ms\n", total - tm.afterTextMs - tm.restoreAfterMs);
    QTest::qWait(1500);
    report("after paste + 1.5 s idle");
    releaseFreedMemory();   // так делает программа через 2 с после вставки
    report("after releaseFreedMemory()");
    return 0;
}
