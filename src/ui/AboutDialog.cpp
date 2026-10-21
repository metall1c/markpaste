#include "AboutDialog.h"

#include "Controls.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QSysInfo>
#include <QVBoxLayout>

namespace mp {

namespace {

// Ссылка с цветом в самой разметке: QLabel берёт цвет ссылок из палитры программы
// в момент разбора HTML, а не из палитры окна
QString anchor(const QString &href, const QString &text)
{
    return QStringLiteral("<a href=\"%1\" style=\"color:%2\">%3</a>")
        .arg(href.toHtmlEscaped(), UiColors::from(QGuiApplication::palette()).link.name(), text.toHtmlEscaped());
}

QLabel *richLabel(const QString &html)
{
    auto *l = new QLabel(html);
    l->setWordWrap(true);
    l->setTextFormat(Qt::RichText);
    l->setTextInteractionFlags(Qt::TextBrowserInteraction);
    l->setOpenExternalLinks(true);
    return l;
}

// Карточка с заголовком и строками «название — значение»
Card *infoCard(const QString &title, const QList<std::pair<QString, QString>> &rows)
{
    auto *card = new Card;
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(6);
    auto *head = new QLabel(title);
    QFont f = head->font();
    f.setWeight(QFont::DemiBold);
    head->setFont(f);
    layout->addWidget(head);
    for (const auto &[name, value] : rows) {
        auto *row = new QHBoxLayout;
        row->setSpacing(12);
        auto *n = new QLabel(name);
        n->setMinimumWidth(130);
        n->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        setMutedText(n);
        row->addWidget(n, 0, Qt::AlignTop);
        QLabel *v = richLabel(value);
        v->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        row->addWidget(v, 1, Qt::AlignTop);
        layout->addLayout(row);
    }
    return card;
}

QString orDash(const QString &s)
{
    return s.isEmpty() ? QStringLiteral("—") : s.toHtmlEscaped();
}

} // namespace

AboutDialog::AboutDialog(const AboutInfo &info, QWidget *parent)
    : QDialog(parent), m_info(info)
{
    setWindowTitle(tr("About Markpaste"));
    useAccentSelection(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(12);

    // Значок, название, версия, назначение
    auto *top = new QHBoxLayout;
    top->setSpacing(16);
    auto *icon = new QLabel;
    icon->setPixmap(QGuiApplication::windowIcon().pixmap(QSize(64, 64), devicePixelRatioF()));
    icon->setFixedSize(64, 64);
    top->addWidget(icon, 0, Qt::AlignTop);
    auto *titles = new QVBoxLayout;
    titles->setSpacing(2);
    auto *name = new QLabel(QStringLiteral("Markpaste"));
    QFont nameFont = name->font();
    nameFont.setWeight(QFont::DemiBold);
    if (nameFont.pointSizeF() > 0)
        nameFont.setPointSizeF(nameFont.pointSizeF() * 1.6);
    name->setFont(nameFont);
    titles->addWidget(name);
    auto *version = new QLabel(tr("Version %1").arg(info.version));
    setMutedText(version);
    titles->addWidget(version);
    titles->addSpacing(4);
    auto *tagline = new QLabel(tr("Screenshots with numbered marks and comments — into any chat in one action. "
                                  "For Linux and Windows."));
    tagline->setWordWrap(true);
    titles->addWidget(tagline);
    top->addLayout(titles, 1);
    layout->addLayout(top);

    const QString link = anchor(info.homepage, QString(info.homepage).remove(QStringLiteral("https://")));
    layout->addWidget(infoCard(tr("Authors"), {
        {tr("Author"), QStringLiteral("Andrey Che")},
        {tr("Email"), anchor(QStringLiteral("mailto:metallicvrn@gmail.com"), QStringLiteral("metallicvrn@gmail.com"))},
        {tr("Co-author"), tr("Claude Opus 5.5 (Anthropic) — the code and documentation were written together "
                             "with the AI assistant in Claude Code")},
    }));
    layout->addWidget(infoCard(tr("Program"), {
        {tr("License"), anchor(QStringLiteral("https://www.gnu.org/licenses/gpl-3.0.html"), QStringLiteral("GPL-3.0-or-later"))},
        {tr("Source code"), link},
        {tr("Built with"), tr("Qt %1 (running on %2)").arg(QStringLiteral(QT_VERSION_STR), QString::fromLatin1(qVersion()))},
        {tr("Icons"), tr("Own set; a few outlines follow %1 (ISC)").arg(anchor(QStringLiteral("https://lucide.dev"), QStringLiteral("Lucide")))},
    }));
    layout->addWidget(infoCard(tr("System"), {
        {tr("Environment"), orDash(info.environment)},
        {tr("Screen capture"), orDash(info.capture)},
        {tr("Hotkeys"), orDash(info.hotkeys)},
        {tr("Clipboard"), orDash(info.clipboard)},
        {tr("Ctrl+V emulation"), orDash(info.injector)},
    }));

    auto *copy = new ActionButton(tr("Copy details"));
    copy->setGlyph(Icon::Copy);
    copy->setFocusPolicy(Qt::StrongFocus);
    copy->setToolTip(tr("Version and system — for a bug report"));
    connect(copy, &QPushButton::clicked, this, [this] { QGuiApplication::clipboard()->setText(details()); });
    auto *close = new ActionButton(tr("Close"), ActionButton::Kind::Primary);
    close->setFocusPolicy(Qt::StrongFocus);
    close->setDefault(true);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(copy);
    buttons->addStretch();
    buttons->addWidget(close);
    layout->addStretch();   // лишняя высота — над кнопками, а не между заголовком и карточками
    layout->addSpacing(4);
    layout->addLayout(buttons);

    // Высота — по переносу строк при этой ширине: sizeHint считает переносы с запасом
    constexpr int kWidth = 560;
    resize(kWidth, layout->totalHeightForWidth(kWidth));
}

QString AboutDialog::details() const
{
    // Без перевода: такой текст уходит в сообщение об ошибке разработчику
    QStringList lines{
        QStringLiteral("Markpaste %1").arg(m_info.version),
        QStringLiteral("Qt %1 (built with %2)").arg(QString::fromLatin1(qVersion()), QStringLiteral(QT_VERSION_STR)),
        QStringLiteral("OS: %1 (%2)").arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture()),
        QStringLiteral("Environment: %1").arg(m_info.environment),
        QStringLiteral("Capture: %1").arg(m_info.capture),
        QStringLiteral("Hotkeys: %1").arg(m_info.hotkeys),
        QStringLiteral("Clipboard: %1").arg(m_info.clipboard),
        QStringLiteral("Ctrl+V emulation: %1").arg(m_info.injector),
    };
    return lines.join(u'\n');
}

} // namespace mp
