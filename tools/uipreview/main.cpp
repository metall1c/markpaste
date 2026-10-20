// Рисует окна программы на тестовом наборе без дисплея (QT_QPA_PLATFORM=offscreen)
// и сохраняет PNG. Нужна, чтобы проверять вёрстку, не снимая настоящий экран.
//   markpaste-uipreview [--dark] [каталог]
//   markpaste-uipreview --live [секунды [файл.png]] — показать ленту (layer-shell) и окно
//     снимка в настоящей сессии; с файлом — через 2,5 с снять экран через spectacle (KDE)
//     Снимок — всего экрана, с чужими окнами
//   markpaste-uipreview --live-dialogs [секунды [префикс]] — окна настроек, мастера и предпросмотра
//     по очереди; с префиксом — каждое снимается отдельно (spectacle -a, только активное окно):
//     префикс-1.png, префикс-2.png, префикс-3.png

#include "app/AppIcon.h"
#include "app/SettingsDialog.h"
#include "core/CaptureSetModel.h"
#include "render/ShotRenderer.h"
#include "ui/AboutDialog.h"
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
#include <QProcess>
#include <QTimer>
#include <QToolButton>
#include <QPainter>
#include <QTranslator>
#include <QTest>
#include <QtMath>

#include <cmath>

using namespace mp;

static QByteArray fakeScreen(int w, int h, const QColor &bg, const QString &title)
{
    QImage img(w, h, QImage::Format_ARGB32);
    img.fill(bg);
    QPainter p(&img);
    p.fillRect(0, 0, w, 48, QColor(40, 44, 52));
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setPixelSize(20);
    p.setFont(f);
    p.drawText(QRect(16, 0, w, 48), Qt::AlignVCenter, title);
    p.fillRect(40, 90, 260, 54, QColor(0x3B, 0x82, 0xF6));
    p.drawText(QRect(40, 90, 260, 54), Qt::AlignCenter, QStringLiteral("Купить"));
    p.setPen(QColor(60, 60, 60));
    p.drawText(QRect(40, 170, w - 80, 30), Qt::AlignLeft, QStringLiteral("Цена: 1 990 ₽   Пароль: hunter2"));
    p.end();
    QByteArray png;
    QBuffer b(&png);
    b.open(QIODevice::WriteOnly);
    img.save(&b, "PNG");
    return png;
}

int main(int argc, char **argv)
{
    const bool liveDialogs = argc > 1 && qstrcmp(argv[1], "--live-dialogs") == 0;
    const bool live = liveDialogs || (argc > 1 && qstrcmp(argv[1], "--live") == 0);
    if (!live)
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    // offscreen не грузит тему KDE: просим Breeze явно, чтобы видеть те же иконки
    QIcon::setThemeName(QStringLiteral("breeze"));
    QTranslator tr;
    if (tr.load(QStringLiteral(":/i18n/markpaste_ru.qm")))
        QCoreApplication::installTranslator(&tr);
    QString out = QStringLiteral("ui-preview");
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == u"--live" || arg == u"--live-dialogs") {
            continue;
        } else if (live) {
            break;   // секунды показа
        } else if (arg == u"--dark") {
            // Цвета Breeze Dark (Plasma 6): offscreen не берёт системную тему
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
    if (!live)
        QDir().mkpath(out);
    QGuiApplication::setWindowIcon(appIcon());

    CaptureSetModel model;
    Shot a;
    a.sourcePng = fakeScreen(900, 520, QColor(245, 245, 245), QStringLiteral("Магазин — карточка товара"));
    a.sourceSize = QSize(900, 520);
    const QUuid ida = model.addShot(a);
    model.addMark(ida, QRect(36, 86, 268, 62), QStringLiteral("Кнопку «Купить» ярче"));
    model.addMark(ida, QRect(36, 166, 160, 34), QStringLiteral("Цену крупнее"),
                  MarkPen{QColor(), 0, LineStyle::Solid, FrameShape::Rounded});
    model.addRedaction(ida, QRect(200, 166, 200, 34));
    model.addLinearMark(ida, MarkShape::Arrow, {{620, 330}, {480, 230}}, QStringLiteral("Сюда — отзывы"),
                        MarkPen{QColor(0x1E, 0x88, 0xE5), 5, LineStyle::Solid});
    QList<QPoint> loop;
    for (int deg = 0; deg <= 370; deg += 10)
        loop.append(QPoint(700 + int(110 * std::cos(qDegreesToRadians(double(deg)))),
                           420 + int(45 * std::sin(qDegreesToRadians(double(deg))))));
    model.addLinearMark(ida, MarkShape::Freehand, loop, QStringLiteral("Убрать этот блок"),
                        MarkPen{QColor(0x43, 0xA0, 0x47), 3, LineStyle::Dash});
    model.addLinearMark(ida, MarkShape::Point, {{860, 140}}, QStringLiteral("Иконка корзины не видна на тёмной теме"),
                        MarkPen{QColor(0x8E, 0x24, 0xAA), 3, LineStyle::Solid});
    Shot b;
    b.sourcePng = fakeScreen(700, 400, QColor(230, 240, 255), QStringLiteral("Корзина"));
    b.sourceSize = QSize(700, 400);
    const QUuid idb = model.addShot(b);
    model.addMark(idb, QRect(30, 80, 300, 80), QString(), MarkPen{QColor(), 0, LineStyle::Solid, FrameShape::Ellipse});

    if (live) {
        const int seconds = argc > 2 ? QString::fromLocal8Bit(argv[2]).toInt() : 6;
        std::unique_ptr<StripWindow> strip;
        std::unique_ptr<ShotEditor> editor;
        std::unique_ptr<SettingsDialog> settings;
        std::unique_ptr<SetupWizard> wizard;
        std::unique_ptr<PreviewDialog> preview;
        Platform livePlatform;
        livePlatform.environment = QStringLiteral("Wayland · KDE Plasma (предпросмотр)");
        livePlatform.notes << QStringLiteral("Пример примечания платформы");
        if (liveDialogs) {
            settings = std::make_unique<SettingsDialog>(Settings{}, livePlatform);
            settings->findChildren<PillButton *>().value(3)->click();   // «Вид»: флажок, список, цвет
            wizard = std::make_unique<SetupWizard>(&livePlatform, QStringLiteral("su._525.MarkpastePreview"));
            preview = std::make_unique<PreviewDialog>();
            PasteOptions opt;
            opt.messageTemplate = MessageTemplate::defaultFor(QLocale(QLocale::Russian));
            preview->showSet(model.set(), opt);
            // По очереди: показанное окно активно, и spectacle -a снимает только его
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

    // Подписи комментариев на картинке (EDT-10)
    {
        RenderStyle captions;
        captions.captions = true;
        renderShot(model.set().shots[0], 0, captions).save(out + QStringLiteral("/captions.png"));
    }

    // Лента — прозрачное окно со скруглёнными углами, как на layer-shell и Windows
    StripWindow strip(&model);
    strip.setAttribute(Qt::WA_TranslucentBackground);
    strip.setCurrentShot(ida);
    strip.setPasteMode(true, QStringLiteral("Alt+V"));
    strip.showMessage(QStringLiteral("Набор скопирован — нажмите Ctrl+V в чате"), 0);
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

    // Снимок B: метка без комментария, её поле открыто, карточка выделена
    editor.showShot(idb, true);
    editor.resize(1000, 600);
    app.processEvents();
    editor.findChild<ShotCanvas *>()->editMark(model.set().shots[1].marks[0].id);
    app.processEvents();
    editor.grab().save(out + QStringLiteral("/editor-comment.png"));
    editor.showShot(ida);
    editor.resize(1000, 680);
    app.processEvents();

    // Выпадающие панели пера: цвет и толщина
    int n = 0;
    for (QMenu *menu : editor.findChildren<QMenu *>()) {
        menu->popup(QPoint(0, 0));
        app.processEvents();
        menu->grab().save(out + QStringLiteral("/pen-menu-%1.png").arg(++n));
        menu->close();
    }

    // Мастер первого запуска с платформой без реализаций: все предупреждения видны
    Platform platform;
    platform.environment = QStringLiteral("Wayland · KDE Plasma 6.7 (предпросмотр)");
    platform.notes << QStringLiteral("Пример примечания платформы");
    SetupWizard wizard(&platform, QStringLiteral("su._525.MarkpastePreview"));
    wizard.show();
    app.processEvents();
    wizard.grab().save(out + QStringLiteral("/setup-wizard.png"));

    // О программе
    {
        AboutInfo info{QStringLiteral(MARKPASTE_VERSION), QStringLiteral("https://gitlab.525.su/che/main/markpaste"),
                       platform.environment, QStringLiteral("KWin ScreenShot2"), QStringLiteral("XDG GlobalShortcuts"),
                       QStringLiteral("Wayland data-control"), QStringLiteral("RemoteDesktop portal")};
        AboutDialog about(info);
        about.show();
        app.processEvents();
        about.grab().save(out + QStringLiteral("/about.png"));
    }

    // Предпросмотр сообщения (PST-6)
    PreviewDialog preview;
    PasteOptions opt;
    opt.messageTemplate = MessageTemplate::defaultFor(QLocale(QLocale::Russian));
    preview.showSet(model.set(), opt);
    if (!QTest::qWaitFor([&] { return preview.isReady(); }, 10000))
        return 1;
    preview.show();
    app.processEvents();
    preview.grab().save(out + QStringLiteral("/preview.png"));

    // Окно настроек: каждая вкладка отдельно
    {
        SettingsDialog dlg(Settings{}, platform);
        dlg.show();
        app.processEvents();
        // Вкладки — кнопки-«пилюли» над страницами
        const QList<PillButton *> tabs = dlg.findChildren<PillButton *>();
        for (int i = 0; i < tabs.size(); ++i) {
            tabs[i]->click();
            app.processEvents();
            dlg.grab().save(out + QStringLiteral("/settings-%1.png").arg(i + 1));
        }
    }

    // Меню ленты: предпросмотр, сохранение в папку, настройки
    for (QToolButton *b : strip.findChildren<QToolButton *>())
        if (b->menu()) {
            b->menu()->popup(QPoint(0, 0));
            app.processEvents();
            b->menu()->grab().save(out + QStringLiteral("/strip-menu.png"));
            b->menu()->close();
        }
    return 0;
}
