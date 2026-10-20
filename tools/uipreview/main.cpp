// Рисует окна программы на тестовом наборе без дисплея (QT_QPA_PLATFORM=offscreen)
// и сохраняет PNG. Нужна, чтобы проверять вёрстку, не снимая настоящий экран.

#include "app/SettingsDialog.h"
#include "core/CaptureSetModel.h"
#include "render/ShotRenderer.h"
#include "ui/PreviewDialog.h"
#include "ui/SetupWizard.h"
#include "ui/ShotEditor.h"
#include "ui/StripWindow.h"

#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QIcon>
#include <QMenu>
#include <QTabWidget>
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
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    // offscreen не грузит тему KDE: просим Breeze явно, чтобы видеть те же иконки
    QIcon::setThemeName(QStringLiteral("breeze"));
    QTranslator tr;
    if (tr.load(QStringLiteral(":/i18n/markpaste_ru.qm")))
        QCoreApplication::installTranslator(&tr);
    const QString out = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("ui-preview");
    QDir().mkpath(out);

    CaptureSetModel model;
    Shot a;
    a.sourcePng = fakeScreen(900, 520, QColor(245, 245, 245), QStringLiteral("Магазин — карточка товара"));
    a.sourceSize = QSize(900, 520);
    const QUuid ida = model.addShot(a);
    model.addMark(ida, QRect(36, 86, 268, 62), QStringLiteral("Кнопку «Купить» ярче"));
    model.addMark(ida, QRect(36, 166, 160, 34), QStringLiteral("Цену крупнее"));
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
    model.addMark(idb, QRect(30, 80, 300, 80), QString());

    // Подписи комментариев на картинке (EDT-10)
    {
        RenderStyle captions;
        captions.captions = true;
        renderShot(model.set().shots[0], 0, captions).save(out + QStringLiteral("/captions.png"));
    }

    StripWindow strip(&model);
    strip.setCurrentShot(ida);
    strip.showMessage(QStringLiteral("Набор скопирован — нажмите Ctrl+V в чате"), 0);
    strip.adjustSize();
    strip.show();
    app.processEvents();
    strip.grab().save(out + QStringLiteral("/strip.png"));

    ShotEditor editor(&model);
    editor.showShot(ida);
    editor.resize(1000, 680);
    app.processEvents();
    editor.grab().save(out + QStringLiteral("/editor.png"));

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
        QTabWidget *tabs = dlg.findChild<QTabWidget *>();
        for (int i = 0; tabs && i < tabs->count(); ++i) {
            tabs->setCurrentIndex(i);
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
