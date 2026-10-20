#include "PreviewDialog.h"

#include <QClipboard>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGuiApplication>
#include <QImage>
#include <QLabel>
#include <QLocale>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace mp {

static constexpr int kPreviewWidth = 520;

PreviewDialog::PreviewDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Message preview"));
    resize(kPreviewWidth + 80, 640);

    auto *layout = new QVBoxLayout(this);
    m_summary = new QLabel;
    m_summary->setWordWrap(true);
    layout->addWidget(m_summary);

    auto *host = new QWidget;
    m_items = new QVBoxLayout(host);
    m_items->setContentsMargins(0, 0, 0, 0);
    m_items->setSpacing(10);
    auto *scroll = new QScrollArea;
    scroll->setWidget(host);
    scroll->setWidgetResizable(true);
    layout->addWidget(scroll, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    m_copyText = buttons->addButton(tr("Copy text"), QDialogButtonBox::ActionRole);
    auto *save = buttons->addButton(tr("Save to folder…"), QDialogButtonBox::ActionRole);
    connect(m_copyText, &QPushButton::clicked, this, [this] { QGuiApplication::clipboard()->setText(m_text); });
    connect(save, &QPushButton::clicked, this, &PreviewDialog::saveToFolderRequested);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
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
        opt.mode = PasteMode::Sequential;   // картинки и текст те же, отличается только способ доставки
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
        caption->setForegroundRole(QPalette::PlaceholderText);
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
                                 .arg(QLocale().formattedDataSize(image->data.size(), 1, QLocale::DataSizeTraditionalFormat), format));
            auto *pic = new QLabel;
            pic->setFrameShape(QFrame::StyledPanel);
            QPixmap px = QPixmap::fromImage(img);
            if (px.width() > kPreviewWidth * devicePixelRatioF())
                px = px.scaledToWidth(int(kPreviewWidth * devicePixelRatioF()), Qt::SmoothTransformation);
            px.setDevicePixelRatio(devicePixelRatioF());
            pic->setPixmap(px);
            pic->setAlignment(Qt::AlignLeft);
            m_items->addWidget(pic);
        } else {
            for (const ClipboardEntry &e : item.payload.entries)
                if (e.mimeType.startsWith(QStringLiteral("text/plain"))) {
                    m_text = QString::fromUtf8(e.data);
                    break;
                }
            caption->setText(tr("%1. Text").arg(step));
            auto *text = new QPlainTextEdit(m_text);
            text->setReadOnly(true);
            text->setMinimumHeight(120);
            m_items->addWidget(text);
        }
    }
    m_items->addStretch();

    m_summary->setText(items.isEmpty() ? tr("The set is empty")
                                       : tr("This goes to the chat: %n image(s) (%1)%2", nullptr, m_imageCount)
                                             .arg(QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat),
                                                  m_text.isEmpty() ? tr(", no text") : tr(" and text")));
    m_copyText->setEnabled(!m_text.isEmpty());
    m_ready = true;
}

} // namespace mp
