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
    // Середина правой стороны рамки: 2 px внутрь от края метки, центр пера — 1 px
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
    EXPECT_EQ(out.pixelColor(171, 83).rgb(), style.accent.rgb());   // наконечник шире древка
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

TEST(Renderer, MarkPenColorWidthAndDash)
{
    Shot s = makeShot(200, 100);
    // Сплошная синяя рамка толщиной 6: середина правой стороны — в 3 px от края
    s.marks.append(Mark{QUuid::createUuid(), QRect(100, 10, 80, 80), {}, MarkShape::Rect, {},
                        MarkPen{QColor(Qt::blue), 6, LineStyle::Solid}});
    // Пунктирная линия карандаша по y = 50 от x = 10 до 90: есть и цветные, и нецветные пиксели
    s.marks.append(Mark{QUuid::createUuid(), QRect(), {}, MarkShape::Freehand, {{10, 50}, {90, 50}},
                        MarkPen{QColor(Qt::green), 4, LineStyle::Dash}});
    const QImage out = renderShot(s, 0);
    EXPECT_EQ(out.pixelColor(100 + 80 - 3, 50).rgb(), QColor(Qt::blue).rgb());
    int colored = 0, other = 0;
    for (int x = 30; x < 90; ++x)
        (out.pixelColor(x, 50).rgb() == QColor(Qt::green).rgb() ? colored : other)++;
    EXPECT_GT(colored, 20);
    EXPECT_GT(other, 10);
}

TEST(Renderer, DefaultPenFollowsStyle)
{
    Mark rect{QUuid(), QRect(0, 0, 10, 10), {}};
    Mark arrow{QUuid(), QRect(), {}, MarkShape::Arrow, {{0, 0}, {50, 0}}};
    RenderStyle style;
    style.accent = QColor(Qt::cyan);
    EXPECT_EQ(markColor(rect, style), QColor(Qt::cyan));
    EXPECT_EQ(markWidth(rect, style), style.frameWidth);
    EXPECT_EQ(markWidth(arrow, style), style.arrowWidth);
    arrow.pen.width = 9;
    EXPECT_EQ(markWidth(arrow, style), 9);
}

TEST(Renderer, PointIsBadgeWithRingCenteredOnSpot)
{
    QImage bg(200, 100, QImage::Format_ARGB32);
    bg.fill(Qt::black);
    Shot s;
    s.sourcePng = toPng(bg);
    s.sourceSize = bg.size();
    s.marks.append(Mark{QUuid::createUuid(), QRect(), {}, MarkShape::Point, {{100, 50}}});
    RenderStyle style;
    const QImage out = renderShot(s, 0, style);
    // Центр — фон значка; кольцо цвета метки за краем значка (14 px) плюс зазор и обводка
    EXPECT_EQ(out.pixelColor(100, 50 + 7).rgb(), badgeFillColor(style.accent).rgb());
    const int ringY = 50 + int(style.badgeDiameter / 2 + style.outlineWidth + style.frameWidth / 2 + style.frameWidth);
    EXPECT_EQ(out.pixelColor(100, ringY).rgb(), style.accent.rgb());
    EXPECT_EQ(out.pixelColor(100, 50 + 30).rgb(), QColor(Qt::black).rgb());
}

TEST(Renderer, CaptionsOnImageOnlyWhenEnabled)
{
    QImage bg(400, 200, QImage::Format_ARGB32);
    bg.fill(Qt::black);
    Shot s;
    s.sourcePng = toPng(bg);
    s.sourceSize = bg.size();
    s.marks.append(Mark{QUuid::createUuid(), QRect(20, 20, 100, 60), QStringLiteral("Кнопку ярче")});
    s.marks.append(Mark{QUuid::createUuid(), QRect(), QStringLiteral("у края"), MarkShape::Point, {{395, 190}}});
    RenderStyle style;
    auto whiteRight = [](const QImage &img, int x0, int x1, int y) {
        int n = 0;
        for (int x = x0; x < x1; ++x)
            n += img.pixelColor(x, y).rgb() == QColor(Qt::white).rgb();
        return n;
    };
    const QImage plain = renderShot(s, 0, style);
    EXPECT_LT(whiteRight(plain, 60, 120, 12), 5);   // справа от значка A1, над рамкой — фон
    style.captions = true;
    const QImage captioned = renderShot(s, 0, style);
    EXPECT_GT(whiteRight(captioned, 60, 120, 12), 30);   // белая плашка с подписью
    // У края подпись не вылезает за снимок, а уходит влево от значка
    EXPECT_GT(whiteRight(captioned, 300, 360, 180), 20);
}
