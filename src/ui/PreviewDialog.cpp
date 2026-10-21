#include "PreviewDialog.h"

#include "Controls.h"

#include <QClipboard>
#include <QFrame>
#include <QGuiApplication>
#include <QImage>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace mp {

static constexpr int kPreviewWidth = 520;

namespace {
// An image as the chat will see it: rounded corners and a thin border
class PictureView : public QWidget
{
public:
    explicit PictureView(const QPixmap &pixmap) : m_pixmap(pixmap)
    {
        setFixedSize(m_pixmap.deviceIndependentSize().toSize());
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const UiColors c = UiColors::from(palette());
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath clip;
        clip.addRoundedRect(r, 8, 8);
        p.save();
        p.setClipPath(clip);
        p.drawPixmap(QPointF(0, 0), m_pixmap);
        p.restore();
        p.setPen(QPen(c.border, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r, 8, 8);
    }

private:
    QPixmap m_pixmap;
};
} // namespace

PreviewDialog::PreviewDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Message preview"));
    useAccentSelection(this);
    resize(kPreviewWidth + 100, 660);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 16);
    layout->setSpacing(10);
    m_summary = new QLabel;
    m_summary->setWordWrap(true);
    QFont summaryFont = m_summary->font();
    summaryFont.setWeight(QFont::DemiBold);
    m_summary->setFont(summaryFont);
    layout->addWidget(m_summary);

    auto *host = new QWidget;
    m_items = new QVBoxLayout(host);
    m_items->setContentsMargins(0, 0, 8, 0);
    m_items->setSpacing(6);
    auto *scroll = new QScrollArea;
    scroll->setWidget(host);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->viewport()->setAutoFillBackground(false);
    host->setAutoFillBackground(false);
    layout->addWidget(scroll, 1);

    auto button = [](const QString &text, ActionButton::Kind kind) {
        auto *b = new ActionButton(text, kind);
        b->setFocusPolicy(Qt::StrongFocus);
        return b;
    };
    auto *copyText = button(tr("Copy text"), ActionButton::Kind::Secondary);
    copyText->setGlyph(Icon::Copy);
    m_copyText = copyText;
    auto *save = button(tr("Save to folder…"), ActionButton::Kind::Secondary);
    auto *close = button(tr("Close"), ActionButton::Kind::Primary);
    close->setDefault(true);
    connect(m_copyText, &QPushButton::clicked, this, [this] { QGuiApplication::clipboard()->setText(m_text); });
    connect(save, &QPushButton::clicked, this, &PreviewDialog::saveToFolderRequested);
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    buttons->addWidget(m_copyText);
    buttons->addWidget(save);
    buttons->addStretch();
    buttons->addWidget(close);
    layout->addLayout(buttons);
}

void PreviewDialog::showSet(const CaptureSet &set, const PasteOptions &options)
{
    m_ready = false;
    m_text.clear();
    m_imageCount = 0;
    m_copyText->setEnabled(false);
    while (QLayoutItem *item = m_items->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    m_summary->setText(tr("Preparing…"));

    PasteOptions opt = options;
    if (opt.mode == PasteMode::FilesAndText)
        opt.mode = PasteMode::Sequential;   // the images and text are the same, only the delivery differs
    const quint64 gen = ++m_generation;
    QPointer<PreviewDialog> self(this);
    QtConcurrent::run([set, opt] { return PasteOrchestrator::bakeSequence(set, opt); })
        .then(this, [self, gen](const QList<PasteOrchestrator::Item> &items) {
            if (self && gen == self->m_generation)
                self->fill(items);
        });
}

void PreviewDialog::fill(const QList<PasteOrchestrator::Item> &items)
{
    qint64 bytes = 0;
    int step = 0;
    for (const PasteOrchestrator::Item &item : items) {
        ++step;
        const ClipboardEntry *image = nullptr;
        for (const ClipboardEntry &e : item.payload.entries)
            if (e.mimeType.startsWith(QStringLiteral("image/"))) {
                image = &e;
                break;
            }

        auto *caption = new QLabel;
        caption->setFont(smallerFont(caption->font()));
        setMutedText(caption);
        if (step > 1)
            m_items->addSpacing(8);
        m_items->addWidget(caption);

        if (image) {
            ++m_imageCount;
            bytes += image->data.size();
            const QImage img = QImage::fromData(image->data);
            const QString format = image->mimeType.mid(6).toUpper();   // image/png → PNG
            const QString name = item.what == u"combined" ? tr("combined image") : tr("shot %1").arg(item.what);
            caption->setText(tr("%1. Image — %2, %3×%4, %5 %6")
                                 .arg(step)
                                 .arg(name)
                                 .arg(img.width())
                                 .arg(img.height())
                                 .arg(uiLocale().formattedDataSize(image->data.size(), 1, QLocale::DataSizeTraditionalFormat), format));
            QPixmap px = QPixmap::fromImage(img);
            if (px.width() > kPreviewWidth * devicePixelRatioF())
                px = px.scaledToWidth(int(kPreviewWidth * devicePixelRatioF()), Qt::SmoothTransformation);
            px.setDevicePixelRatio(devicePixelRatioF());
            m_items->addWidget(new PictureView(px), 0, Qt::AlignLeft);
        } else {
            for (const ClipboardEntry &e : item.payload.entries)
                if (e.mimeType.startsWith(QStringLiteral("text/plain"))) {
                    m_text = QString::fromUtf8(e.data);
                    break;
                }
            caption->setText(tr("%1. Text").arg(step));
            // The text on a card, like a chat message
            auto *card = new Card;
            auto *cardLayout = new QVBoxLayout(card);
            cardLayout->setContentsMargins(10, 8, 10, 8);
            auto *text = new QPlainTextEdit(m_text);
            text->setReadOnly(true);
            text->setFrameShape(QFrame::NoFrame);
            text->setMinimumHeight(120);
            text->viewport()->setAutoFillBackground(false);
            text->setAutoFillBackground(false);
            cardLayout->addWidget(text);
            m_items->addWidget(card);
        }
    }
    m_items->addStretch();

    m_summary->setText(items.isEmpty() ? tr("The set is empty")
                                       : tr("This goes to the chat: %n image(s) (%1)%2", nullptr, m_imageCount)
                                             .arg(uiLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat),
                                                  m_text.isEmpty() ? tr(", no text") : tr(" and text")));
    m_copyText->setEnabled(!m_text.isEmpty());
    m_ready = true;
}

} // namespace mp
