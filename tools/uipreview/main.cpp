// Рисует окна программы на тестовом наборе без дисплея (QT_QPA_PLATFORM=offscreen)
// и сохраняет PNG. Нужна, чтобы проверять вёрстку, не снимая настоящий экран.

#include "core/CaptureSetModel.h"
#include "ui/ShotEditor.h"
#include "ui/StripWindow.h"

#include <QApplication>
#include <QBuffer>
#include <QDir>
#include <QIcon>
#include <QPainter>
#include <QTranslator>

using namespace ps;

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
    if (tr.load(QStringLiteral(":/i18n/pointshot_ru.qm")))
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
    Shot b;
    b.sourcePng = fakeScreen(700, 400, QColor(230, 240, 255), QStringLiteral("Корзина"));
    b.sourceSize = QSize(700, 400);
    const QUuid idb = model.addShot(b);
    model.addMark(idb, QRect(30, 80, 300, 80), QString());

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
    return 0;
}
