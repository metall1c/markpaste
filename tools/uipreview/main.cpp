// Renders the program windows on a test set without a display (QT_QPA_PLATFORM=offscreen)
// and saves PNGs. Used to check the layout without capturing the real screen.
//   markpaste-uipreview [--dark] [--lang en|ru] [directory]
//   markpaste-uipreview --live [seconds [file.png]]: show the strip (layer-shell) and the shot
//     window in a real session; with a file, capture the screen via spectacle (KDE) after 2.5 s.
//     The capture is of the whole screen, with other windows
//   markpaste-uipreview --live-dialogs [seconds [prefix]]: the settings, wizard and preview windows
//     one after another; with a prefix, each is captured separately (spectacle -a, active window only):
//     prefix-1.png, prefix-2.png, prefix-3.png

#include "app/AppIcon.h"
#include "app/SettingsDialog.h"
#include "core/CaptureSetModel.h"
#include "render/ShotRenderer.h"
#include "ui/AboutDialog.h"
#include "ui/CaptureOverlay.h"
#include "ui/Controls.h"
#include "ui/PreviewDialog.h"
#include "ui/SetupWizard.h"
#include "ui/ShotEditor.h"
#include "ui/StripWindow.h"

#include <QApplication>
#include <QBuffer>
#include <QPalette>
#include <QDir>
#include <QIcon>
#include <QMenu>
#include <QMouseEvent>
#include <QProcess>
#include <QTimer>
#include <QToolButton>
#include <QPainter>
#include <QTranslator>
#include <QTest>
#include <QtMath>

#include <cmath>

using namespace mp;

// Demo content language: --lang en|ru (Russian by default, as before)
static bool g_english = false;

static QString L(const char *en, const char *ru)
{
    return QString::fromUtf8(g_english ? en : ru);
}

static QByteArray toPng(const QImage &img)
{
    QByteArray png;
    QBuffer b(&png);
    b.open(QIODevice::WriteOnly);
    img.save(&b, "PNG");
    return png;
}

static QFont pixelFont(int px, bool bold = false)
{
    QFont f;
    f.setPixelSize(px);
    f.setBold(bold);
    return f;
}

// A product page of an imaginary shop, 900×520. The demo marks point at its elements:
// the Buy button, the price, the card number (blurred), reviews, an ad banner, the cart icon.
static QImage shopPage()
{
    QImage img(900, 520, QImage::Format_RGB32);
    img.fill(QColor(246, 247, 249));
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    // Site header
    p.fillRect(0, 0, 900, 48, QColor(36, 41, 51));
    p.setPen(Qt::white);
    p.setFont(pixelFont(20, true));
    p.drawText(QRect(16, 0, 600, 48), Qt::AlignVCenter, L("Shop: product card", "Магазин — карточка товара"));
    // Toolbar with icons on the right; the cart icon is dark on dark: the point mark is about it
    p.fillRect(0, 48, 900, 40, QColor(52, 58, 70));
    p.setPen(QPen(QColor(64, 70, 84), 3));
    p.setBrush(Qt::NoBrush);
    p.drawRect(848, 58, 24, 16);
    p.drawEllipse(QPointF(853, 79), 2.5, 2.5);
    p.drawEllipse(QPointF(867, 79), 2.5, 2.5);
    p.setPen(QColor(200, 205, 215));
    p.setFont(pixelFont(15));
    p.drawText(QRect(24, 48, 700, 40), Qt::AlignVCenter,
               L("Catalog    Deals    Delivery    Help", "Каталог    Акции    Доставка    Помощь"));
    // Buy button
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x3B, 0x82, 0xF6));
    p.drawRoundedRect(QRectF(40, 110, 260, 54), 8, 8);
    p.setPen(Qt::white);
    p.setFont(pixelFont(20, true));
    p.drawText(QRect(40, 110, 260, 54), Qt::AlignCenter, L("Buy", "Купить"));
    // Price and the card number that gets blurred
    p.setPen(QColor(40, 40, 40));
    p.setFont(pixelFont(17));
    p.drawText(QRect(44, 188, 150, 30), Qt::AlignVCenter, L("Price: $24.90", "Цена: 1 990 ₽"));
    p.drawText(QRect(204, 188, 196, 30), Qt::AlignVCenter, QStringLiteral("4111 1111 1111 1111"));
    // Product photo placeholder
    QLinearGradient g(40, 240, 420, 500);
    g.setColorAt(0, QColor(255, 196, 120));
    g.setColorAt(1, QColor(240, 120, 90));
    p.setPen(Qt::NoPen);
    p.setBrush(g);
    p.drawRoundedRect(QRectF(40, 240, 380, 255), 12, 12);
    p.setBrush(QColor(255, 255, 255, 170));
    p.drawEllipse(QPointF(230, 367), 66, 66);
    // Description lines and reviews
    p.setPen(QColor(60, 60, 60));
    p.setFont(pixelFont(19, true));
    p.drawText(QRect(450, 230, 420, 30), Qt::AlignVCenter, L("Reviews (12)  ★ 4.5", "Отзывы (12)  ★ 4,5"));
    p.setBrush(QColor(214, 218, 226));
    for (int i = 0; i < 4; ++i)
        p.drawRoundedRect(QRectF(450, 275 + i * 22, 380 - i * 40, 10), 5, 5);
    // Ad banner (circled with the freehand mark)
    p.setBrush(QColor(255, 236, 153));
    p.drawRoundedRect(QRectF(610, 405, 190, 60), 8, 8);
    p.setPen(QColor(150, 90, 0));
    p.setFont(pixelFont(17, true));
    p.drawText(QRect(610, 405, 190, 60), Qt::AlignCenter, L("SALE −50%", "РАСПРОДАЖА −50%"));
    return img;
}

// A cart page, 700×400: the order row and the Checkout button
static QImage cartPage()
{
    QImage img(700, 400, QImage::Format_RGB32);
    img.fill(QColor(232, 240, 254));
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(0, 0, 700, 48, QColor(36, 41, 51));
    p.setPen(Qt::white);
    p.setFont(pixelFont(20, true));
    p.drawText(QRect(16, 0, 600, 48), Qt::AlignVCenter, L("Cart", "Корзина"));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x22, 0xA0, 0x6B));
    p.drawRoundedRect(QRectF(50, 95, 240, 50), 8, 8);
    p.setPen(Qt::white);
    p.setFont(pixelFont(18, true));
    p.drawText(QRect(50, 95, 240, 50), Qt::AlignCenter, L("Checkout", "Оформить заказ"));
    p.setPen(QColor(50, 50, 50));
    p.setFont(pixelFont(17));
    p.drawText(QRect(50, 190, 600, 30), Qt::AlignVCenter, L("1 × Desk lamp    $24.90", "1 × Настольная лампа    1 990 ₽"));
    p.drawText(QRect(50, 230, 600, 30), Qt::AlignVCenter, L("Delivery: tomorrow, free", "Доставка: завтра, бесплатно"));
    return img;
}

// A desktop with a browser window showing the shop page, for the area selection picture.
// The page lies at kPageOrigin, so the selection matches shot A exactly.
static const QPoint kPageOrigin(330, 170);
static QImage desktopImage()
{
    QImage img(1600, 900, QImage::Format_RGB32);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    QLinearGradient wall(0, 0, 1600, 900);
    wall.setColorAt(0, QColor(28, 58, 94));
    wall.setColorAt(1, QColor(84, 46, 102));
    p.fillRect(img.rect(), wall);
    // Browser window: tabs, address bar, page with a side column
    const QRect win(250, 70, 1080, 720);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(222, 225, 230));
    p.drawRoundedRect(win, 10, 10);
    p.setBrush(Qt::white);
    p.drawRoundedRect(QRect(win.left() + 12, win.top() + 8, 220, 30), 6, 6);
    p.setPen(QColor(60, 60, 60));
    p.setFont(pixelFont(14));
    p.drawText(QRect(win.left() + 24, win.top() + 8, 200, 30), Qt::AlignVCenter, L("Desk lamp: Shop", "Настольная лампа — Магазин"));
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(246, 247, 249));
    p.drawRoundedRect(QRect(win.left() + 12, win.top() + 48, win.width() - 24, 30), 15, 15);
    p.setPen(QColor(110, 110, 110));
    p.drawText(QRect(win.left() + 30, win.top() + 48, 600, 30), Qt::AlignVCenter, QStringLiteral("shop.example/lamp"));
    p.fillRect(QRect(win.left(), win.top() + 90, win.width(), win.height() - 90), QColor(246, 247, 249));
    p.drawImage(kPageOrigin, shopPage());
    // Taskbar
    p.fillRect(QRect(0, 852, 1600, 48), QColor(20, 22, 28, 235));
    return img;
}

int main(int argc, char **argv)
{
    const bool liveDialogs = argc > 1 && qstrcmp(argv[1], "--live-dialogs") == 0;
    const bool live = liveDialogs || (argc > 1 && qstrcmp(argv[1], "--live") == 0);
    if (!live)
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    // offscreen does not load the KDE theme: ask for Breeze explicitly to see the same icons
    QIcon::setThemeName(QStringLiteral("breeze"));
    QString out = QStringLiteral("ui-preview");
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == u"--live" || arg == u"--live-dialogs") {
            continue;
        } else if (!live && arg == u"--lang" && i + 1 < argc) {
            g_english = qstrcmp(argv[++i], "en") == 0;
        } else if (live) {
            break;   // seconds to show
        } else if (arg == u"--dark") {
            // Breeze Dark colors (Plasma 6): offscreen does not take the system theme
            QPalette pal;
            pal.setColor(QPalette::Window, QColor(0x20, 0x23, 0x26));
            pal.setColor(QPalette::WindowText, QColor(0xfc, 0xfc, 0xfc));
            pal.setColor(QPalette::Base, QColor(0x14, 0x16, 0x18));
            pal.setColor(QPalette::AlternateBase, QColor(0x1d, 0x1f, 0x22));
            pal.setColor(QPalette::Text, QColor(0xfc, 0xfc, 0xfc));
            pal.setColor(QPalette::Button, QColor(0x29, 0x2c, 0x30));
            pal.setColor(QPalette::ButtonText, QColor(0xfc, 0xfc, 0xfc));
            pal.setColor(QPalette::Highlight, QColor(0x3d, 0xae, 0xe9));
            pal.setColor(QPalette::HighlightedText, QColor(0xfc, 0xfc, 0xfc));
            pal.setColor(QPalette::ToolTipBase, QColor(0x29, 0x2c, 0x30));
            pal.setColor(QPalette::ToolTipText, QColor(0xfc, 0xfc, 0xfc));
            pal.setColor(QPalette::PlaceholderText, QColor(0xa1, 0xa9, 0xb1));
            pal.setColor(QPalette::Mid, QColor(0x3a, 0x3e, 0x42));
            QApplication::setPalette(pal);
        } else {
            out = arg;
        }
    }
    QTranslator tr;
    if (!g_english && tr.load(QStringLiteral(":/i18n/markpaste_ru.qm")))
        QCoreApplication::installTranslator(&tr);
    const QLocale demoLocale(g_english ? QLocale::English : QLocale::Russian);
    setUiLocale(demoLocale);
    if (!live)
        QDir().mkpath(out);
    QGuiApplication::setWindowIcon(appIcon());

    CaptureSetModel model;
    Shot a;
    a.sourcePng = toPng(shopPage());
    a.sourceSize = QSize(900, 520);
    const QUuid ida = model.addShot(a);
    model.addMark(ida, QRect(36, 106, 268, 62), L("Make the Buy button brighter", "Кнопку «Купить» ярче"));
    model.addMark(ida, QRect(38, 186, 158, 34), L("Larger price", "Цену крупнее"),
                  MarkPen{QColor(), 0, LineStyle::Solid, FrameShape::Rounded});
    model.addRedaction(ida, QRect(200, 184, 200, 38));
    model.addLinearMark(ida, MarkShape::Arrow, {{650, 370}, {560, 262}}, L("Reviews should be higher", "Отзывы — выше"),
                        MarkPen{QColor(0x1E, 0x88, 0xE5), 5, LineStyle::Solid});
    QList<QPoint> loop;
    for (int deg = 0; deg <= 370; deg += 10)
        loop.append(QPoint(705 + int(118 * std::cos(qDegreesToRadians(double(deg)))),
                           435 + int(46 * std::sin(qDegreesToRadians(double(deg))))));
    model.addLinearMark(ida, MarkShape::Freehand, loop, L("Remove this block", "Убрать этот блок"),
                        MarkPen{QColor(0x43, 0xA0, 0x47), 3, LineStyle::Dash});
    model.addLinearMark(ida, MarkShape::Point, {{860, 68}}, L("The cart icon is invisible in the dark theme", "Иконка корзины не видна на тёмной теме"),
                        MarkPen{QColor(0x8E, 0x24, 0xAA), 3, LineStyle::Solid});
    Shot b;
    b.sourcePng = toPng(cartPage());
    b.sourceSize = QSize(700, 400);
    const QUuid idb = model.addShot(b);
    model.addMark(idb, QRect(30, 80, 280, 80), QString(), MarkPen{QColor(), 0, LineStyle::Solid, FrameShape::Ellipse});

    if (live) {
        const int seconds = argc > 2 ? QString::fromLocal8Bit(argv[2]).toInt() : 6;
        std::unique_ptr<StripWindow> strip;
        std::unique_ptr<ShotEditor> editor;
        std::unique_ptr<SettingsDialog> settings;
        std::unique_ptr<SetupWizard> wizard;
        std::unique_ptr<PreviewDialog> preview;
        Platform livePlatform;
        livePlatform.environment = L("Wayland · KDE Plasma (preview)", "Wayland · KDE Plasma (предпросмотр)");
        livePlatform.notes << L("A sample platform note", "Пример примечания платформы");
        if (liveDialogs) {
            settings = std::make_unique<SettingsDialog>(Settings{}, livePlatform);
            settings->findChildren<PillButton *>().value(3)->click();   // "Appearance": checkbox, list, color
            wizard = std::make_unique<SetupWizard>(&livePlatform, QStringLiteral("su._525.MarkpastePreview"));
            preview = std::make_unique<PreviewDialog>();
            PasteOptions opt;
            opt.messageTemplate = MessageTemplate::defaultFor(demoLocale);
            preview->showSet(model.set(), opt);
            // One after another: the shown window is active, and spectacle -a captures only it
            const QString prefix = argc > 3 ? QString::fromLocal8Bit(argv[3]) : QString();
            const QList<QWidget *> windows{settings.get(), wizard.get(), preview.get()};
            for (int i = 0; i < windows.size(); ++i) {
                QTimer::singleShot(i * 3000, &app, [windows, i] {
                    if (i > 0)
                        windows[i - 1]->hide();
                    windows[i]->show();
                    windows[i]->raise();
                    windows[i]->activateWindow();
                });
                if (!prefix.isEmpty())
                    QTimer::singleShot(i * 3000 + 2000, &app, [prefix, i] {
                        QProcess::startDetached(QStringLiteral("spectacle"),
                                                {QStringLiteral("-b"), QStringLiteral("-n"), QStringLiteral("-a"),
                                                 QStringLiteral("-o"), QStringLiteral("%1-%2.png").arg(prefix).arg(i + 1)});
                    });
            }
            QTimer::singleShot(int(windows.size()) * 3000 + 500, &app, &QApplication::quit);
            return app.exec();
        } else {
            strip = std::make_unique<StripWindow>(&model);
            strip->setCurrentShot(idb);
            strip->setPasteMode(true, QStringLiteral("Alt+V"));
            strip->present();
            editor = std::make_unique<ShotEditor>(&model);
            editor->showShot(idb, true);
            editor->findChild<ShotCanvas *>()->editMark(model.set().shots[1].marks[0].id);
        }
        if (argc > 3) {
            const QString file = QString::fromLocal8Bit(argv[3]);
            QTimer::singleShot(2500, &app, [file] {
                QProcess::startDetached(QStringLiteral("spectacle"), {QStringLiteral("-b"), QStringLiteral("-n"),
                                                                      QStringLiteral("-f"), QStringLiteral("-o"), file});
            });
        }
        QTimer::singleShot(seconds * 1000, &app, &QApplication::quit);
        return app.exec();
    }

    // Area selection over a frozen desktop: the drag is in progress, the magnifier and size are visible
    {
        ScreenFrame frame;
        frame.screenName = QStringLiteral("preview");
        frame.logicalGeometry = QRect(0, 0, 1600, 900);
        frame.image = desktopImage();
        CaptureSession session({frame}, false);
        OverlayWidget overlay(&session, frame, false);
        overlay.resize(1600, 900);
        overlay.show();
        app.processEvents();
        const QPointF from = kPageOrigin;
        const QPointF to = kPageOrigin + QPoint(900, 520);
        QMouseEvent press(QEvent::MouseButtonPress, from, from, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&overlay, &press);
        QMouseEvent move(QEvent::MouseMove, to, to, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(&overlay, &move);
        app.processEvents();
        overlay.grab().save(out + QStringLiteral("/overlay.png"));
        overlay.hide();
    }

    // Comment captions on the image (EDT-10)
    {
        RenderStyle captions;
        captions.captions = true;
        renderShot(model.set().shots[0], 0, captions).save(out + QStringLiteral("/captions.png"));
    }

    // The strip is a transparent window with rounded corners, as on layer-shell and Windows
    StripWindow strip(&model);
    strip.setAttribute(Qt::WA_TranslucentBackground);
    strip.setCurrentShot(ida);
    strip.setPasteMode(true, QStringLiteral("Alt+V"));
    strip.adjustSize();
    strip.show();
    app.processEvents();
    strip.grab().save(out + QStringLiteral("/strip-set.png"));
    strip.showMessage(L("Set copied: press Ctrl+V in the chat", "Набор скопирован — нажмите Ctrl+V в чате"), 0);
    strip.adjustSize();
    strip.show();
    app.processEvents();
    strip.grab().save(out + QStringLiteral("/strip.png"));

    CaptureSetModel emptyModel;
    StripWindow emptyStrip(&emptyModel);
    emptyStrip.setAttribute(Qt::WA_TranslucentBackground);
    emptyStrip.setCaptureHotkey(QStringLiteral("Print"));
    emptyStrip.adjustSize();
    emptyStrip.show();
    app.processEvents();
    emptyStrip.grab().save(out + QStringLiteral("/strip-empty.png"));

    ShotEditor editor(&model);
    editor.showShot(ida);
    editor.resize(1000, 680);
    app.processEvents();
    editor.grab().save(out + QStringLiteral("/editor.png"));

    // Shot B: a mark without a comment, its field open, the card selected
    editor.showShot(idb, true);
    editor.resize(1000, 600);
    app.processEvents();
    editor.findChild<ShotCanvas *>()->editMark(model.set().shots[1].marks[0].id);
    app.processEvents();
    editor.grab().save(out + QStringLiteral("/editor-comment.png"));
    editor.showShot(ida);
    editor.resize(1000, 680);
    app.processEvents();

    // Pen drop-down panels: color and width
    int n = 0;
    for (QMenu *menu : editor.findChildren<QMenu *>()) {
        menu->popup(QPoint(0, 0));
        app.processEvents();
        menu->grab().save(out + QStringLiteral("/pen-menu-%1.png").arg(++n));
        menu->close();
    }

    // The first-start wizard with a platform without implementations: all warnings are visible
    Platform platform;
    platform.environment = L("Wayland · KDE Plasma 6.7 (preview)", "Wayland · KDE Plasma 6.7 (предпросмотр)");
    platform.notes << L("A sample platform note", "Пример примечания платформы");
    SetupWizard wizard(&platform, QStringLiteral("su._525.MarkpastePreview"));
    wizard.show();
    app.processEvents();
    wizard.grab().save(out + QStringLiteral("/setup-wizard.png"));

    // About
    {
        AboutInfo info{QStringLiteral(MARKPASTE_VERSION), QStringLiteral(MARKPASTE_HOMEPAGE_URL),
                       platform.environment, QStringLiteral("KWin ScreenShot2"), QStringLiteral("XDG GlobalShortcuts"),
                       QStringLiteral("Wayland data-control"), QStringLiteral("RemoteDesktop portal")};
        AboutDialog about(info);
        about.show();
        app.processEvents();
        about.grab().save(out + QStringLiteral("/about.png"));
    }

    // Message preview (PST-6)
    PreviewDialog preview;
    PasteOptions opt;
    opt.messageTemplate = MessageTemplate::defaultFor(demoLocale);
    preview.showSet(model.set(), opt);
    if (!QTest::qWaitFor([&] { return preview.isReady(); }, 10000))
        return 1;
    preview.show();
    app.processEvents();
    preview.grab().save(out + QStringLiteral("/preview.png"));

    // Settings window: each tab separately
    {
        SettingsDialog dlg(Settings{}, platform);
        dlg.show();
        app.processEvents();
        // Tabs are "pill" buttons above the pages
        const QList<PillButton *> tabs = dlg.findChildren<PillButton *>();
        for (int i = 0; i < tabs.size(); ++i) {
            tabs[i]->click();
            app.processEvents();
            dlg.grab().save(out + QStringLiteral("/settings-%1.png").arg(i + 1));
        }
    }

    // Strip menu: preview, save to folder, settings
    for (QToolButton *b : strip.findChildren<QToolButton *>())
        if (b->menu()) {
            b->menu()->popup(QPoint(0, 0));
            app.processEvents();
            b->menu()->grab().save(out + QStringLiteral("/strip-menu.png"));
            b->menu()->close();
        }
    return 0;
}
