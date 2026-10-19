#include "TestUtil.h"
#include "render/ShotRenderer.h"

#include <gtest/gtest.h>

using namespace mp;
using namespace mp::test;

TEST(Renderer, BadgeContrastAtLeast45)
{
    const QColor defaultAccent(0xFF, 0x5A, 0x1F);
    EXPECT_LT(contrastRatio(defaultAccent, Qt::white), 4.5);   // поэтому и затемняем
    for (const QColor &accent : {defaultAccent, QColor(Qt::yellow), QColor(Qt::cyan), QColor(Qt::white),
                                 QColor(0x22, 0x66, 0xFF)}) {
        EXPECT_GE(contrastRatio(badgeFillColor(accent), Qt::white), 4.5) << accent.name().toStdString();
    }
    // Тёмный акцент не трогаем
    EXPECT_EQ(badgeFillColor(QColor(0x10, 0x10, 0x80)), QColor(0x10, 0x10, 0x80));
}

TEST(Renderer, PixelateLeavesNoSourcePixels)
{
    const QImage original = noiseImage(100, 60);
    QImage img = original;
    const QRect area(10, 5, 50, 37);   // не кратно блоку
    const int block = 12;
    pixelate(img, area, block);

    for (int by = area.top(); by <= area.bottom(); by += block)
        for (int bx = area.left(); bx <= area.right(); bx += block) {
            const QRgb first = img.pixel(bx, by);
            for (int y = by; y < std::min(by + block, area.bottom() + 1); ++y)
                for (int x = bx; x < std::min(bx + block, area.right() + 1); ++x)
                    ASSERT_EQ(img.pixel(x, y), first) << x << "," << y;
        }
    // Снаружи ничего не изменилось
    EXPECT_EQ(img.pixel(0, 0), original.pixel(0, 0));
    EXPECT_EQ(img.pixel(60, 42), original.pixel(60, 42));
    EXPECT_EQ(img.pixel(99, 59), original.pixel(99, 59));
}

TEST(Renderer, RenderAppliesCrop)
{
    Shot s = makeShot(200, 100);
    s.crop = QRect(50, 20, 80, 40);
    const QImage out = renderShot(s, 0);
    EXPECT_EQ(out.size(), QSize(80, 40));
}

TEST(Renderer, RenderDrawsFrameInAccent)
{
    Shot s = makeShot(200, 100);
    s.marks.append(Mark{QUuid::createUuid(), QRect(100, 40, 80, 50), {}});
    RenderStyle style;
    const QImage out = renderShot(s, 0, style);
    // Середина правой стороны рамки: 3 px внутрь от края метки, центр пера — 1.5 px
    const QColor c = out.pixelColor(100 + 80 - 2, 65);
    EXPECT_EQ(c.rgb(), style.accent.rgb());
}

TEST(Renderer, RedactionUsesScaledBlock)
{
    Shot s = makeShot(120, 120, 2.0);
    s.redactions.append(Redaction{QUuid::createUuid(), QRect(0, 0, 48, 48)});
    const QImage out = renderShot(s, 0);
    // При масштабе 200 % блок — 24 физических пикселя
    EXPECT_EQ(out.pixel(0, 0), out.pixel(23, 23));
    EXPECT_NE(out.pixel(0, 0), out.pixel(24, 0));
}

TEST(Renderer, EncodeLimitsLongSide)
{
    const QImage big = noiseImage(5000, 100);
    EncodeOptions opt;
    opt.jpegThresholdBytes = 0;   // только масштаб
    const EncodedImage e = encodeForPaste(big, opt);
    EXPECT_EQ(e.size.width(), 4096);
    EXPECT_EQ(e.mimeType, QByteArray("image/png"));
}

TEST(Renderer, EncodeSwitchesToJpegWhenLarge)
{
    const QImage img = noiseImage(400, 400);   // шум жмётся плохо: PNG ~ 480 КБ
    EncodeOptions opt;
    opt.jpegThresholdBytes = 100 * 1024;
    const EncodedImage e = encodeForPaste(img, opt);
    EXPECT_EQ(e.mimeType, QByteArray("image/jpeg"));
    EXPECT_EQ(e.extension, QStringLiteral("jpg"));
}

TEST(Renderer, ComposeVertical)
{
    const QImage a = noiseImage(100, 50);
    const QImage b = noiseImage(60, 30);
    const QImage out = composeVertical({a, b}, {QStringLiteral("A"), QStringLiteral("B")});
    EXPECT_EQ(out.width(), 100 + 32);
    EXPECT_EQ(out.height(), 16 + (28 + 50 + 16) + (28 + 30 + 16));
}

TEST(Renderer, RenderDrawsArrowAndFreehand)
{
    Shot s = makeShot(200, 100);
    s.marks.append(Mark{QUuid::createUuid(), QRect(), {}, MarkShape::Arrow, {{20, 80}, {180, 80}}});
    s.marks.append(Mark{QUuid::createUuid(), QRect(), {}, MarkShape::Freehand, {{20, 20}, {100, 20}, {180, 20}}});
    RenderStyle style;
    const QImage out = renderShot(s, 0, style);
    EXPECT_EQ(out.pixelColor(100, 80).rgb(), style.accent.rgb());   // древко
    EXPECT_EQ(out.pixelColor(176, 80).rgb(), style.accent.rgb());   // наконечник у острия
    EXPECT_EQ(out.pixelColor(167, 85).rgb(), style.accent.rgb());   // наконечник шире древка
    EXPECT_EQ(out.pixelColor(100, 20).rgb(), style.accent.rgb());   // линия карандаша
    // Вдали от линий — исходные пиксели
    const QImage src = noiseImage(200, 100);
    EXPECT_EQ(out.pixel(100, 50), src.pixel(100, 50));
}

// Для глазной проверки: MARKPASTE_RENDER_SAMPLE=путь.png сохраняет пример всех форм.
TEST(Renderer, SampleForVisualCheck)
{
    const QByteArray path = qgetenv("MARKPASTE_RENDER_SAMPLE");
    if (path.isEmpty())
        GTEST_SKIP();
    QImage bg(600, 300, QImage::Format_ARGB32);
    bg.fill(QColor(0xF4, 0xF4, 0xF4));
    Shot s;
    s.sourcePng = toPng(bg);
    s.sourceSize = bg.size();
    s.devicePixelRatio = 1.0;
    s.marks.append(Mark{QUuid::createUuid(), QRect(), {}, MarkShape::Arrow, {{40, 250}, {200, 120}}});
    s.marks.append(Mark{QUuid::createUuid(), QRect(240, 40, 140, 90), {}});
    QList<QPoint> loop;
    for (int a = 0; a <= 380; a += 8)
        loop.append(QPoint(500 + int(70 * std::cos(a * M_PI / 180)), 200 + int(50 * std::sin(a * M_PI / 180))));
    s.marks.append(Mark{QUuid::createUuid(), QRect(), {}, MarkShape::Freehand, loop});
    s.marks.append(Mark{QUuid::createUuid(), QRect(), {}, MarkShape::Arrow, {{300, 280}, {320, 270}}});
    EXPECT_TRUE(renderShot(s, 0).save(QString::fromLocal8Bit(path)));
}
