#include "SettingsDialog.h"

#include "LogFile.h"
#include "platform/Autostart.h"
#include "ui/Controls.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDesktopServices>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QButtonGroup>
#include <QSpinBox>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace mp {

namespace {
void setButtonColor(ActionButton *b, const QColor &c)
{
    b->setText(c.name().toUpper());
    b->setSwatch(c);
}

// Кнопка внутри окна настроек: второстепенная, с фокусом с клавиатуры
ActionButton *dialogButton(const QString &text)
{
    auto *b = new ActionButton(text);
    b->setFocusPolicy(Qt::StrongFocus);
    return b;
}

// Страница: поля формы и отступ сверху от вкладок
QFormLayout *formPage(QWidget *page)
{
    auto *form = new QFormLayout(page);
    form->setContentsMargins(2, 14, 2, 2);
    form->setVerticalSpacing(10);
    return form;
}

QLabel *noteLabel(const QString &text)
{
    auto *l = new QLabel(text);
    l->setWordWrap(true);
    l->setFont(smallerFont(l->font()));
    setMutedText(l);
    return l;
}
} // namespace

SettingsDialog::SettingsDialog(const Settings &settings, const Platform &platform,
                               const QHash<Action, QString> &boundTriggers, QWidget *parent)
    : QDialog(parent), m_settings(settings), m_accentColor(settings.accent)
{
    setWindowTitle(tr("Markpaste settings"));
    useAccentSelection(this);

    // Вкладки — «пилюли» над страницами (docs/decisions/0019-ui-style.md)
    auto *pages = new QStackedWidget;
    auto *tabRow = new QHBoxLayout;
    tabRow->setSpacing(6);
    auto *tabGroup = new QButtonGroup(this);
    auto addPage = [pages, tabRow, tabGroup](QWidget *page, const QString &title) {
        auto *tab = new PillButton(title);
        const int index = pages->addWidget(page);
        tabGroup->addButton(tab, index);
        tabRow->addWidget(tab);
        tab->setChecked(index == 0);
        QObject::connect(tab, &QPushButton::clicked, pages, [pages, index] { pages->setCurrentIndex(index); });
    };

    // --- Клавиши и захват ---
    auto *keys = new QWidget;
    auto *kf = formPage(keys);
    m_captureKey = new QLineEdit(settings.captureHotkey);
    m_pasteKey = new QLineEdit(settings.pasteHotkey);
    // Сочетания хранит система (портал GlobalShortcuts): своё значение она учитывает только при
    // первой привязке, поэтому показываем фактическое и меняем его через систему.
    m_systemKeys = platform.hotkeys && platform.hotkeys->systemAssignsTriggers();
    if (m_systemKeys) {
        for (auto [edit, action] : {std::pair{m_captureKey, Action::Capture}, std::pair{m_pasteKey, Action::PasteSet}}) {
            const QString t = boundTriggers.value(action);
            edit->setText(t.isEmpty() ? tr("not assigned") : t);
            edit->setReadOnly(true);
        }
    }
    kf->addRow(tr("Capture:"), m_captureKey);
    kf->addRow(tr("Paste the set:"), m_pasteKey);
    auto *keysNote = noteLabel(
        m_systemKeys
            ? tr("Shortcuts are assigned by the desktop. Change them with the button below "
                 "or in the system shortcut settings.")
            : tr("Format: Primary+Alt+V, Print. Primary is Ctrl on Windows and Linux."));
    kf->addRow(keysNote);
    if (platform.hotkeys && platform.hotkeys->canOpenSystemSettings()) {
        auto *sys = dialogButton(tr("Change shortcuts…"));
        IHotkeys *hk = platform.hotkeys.get();
        connect(sys, &QPushButton::clicked, this, [hk] { hk->openSystemSettings(); });
        auto *row = new QHBoxLayout;
        row->addWidget(sys);
        row->addStretch();
        kf->addRow(row);
    }
    m_quick = new QCheckBox(tr("Confirm the area on mouse release (otherwise — Enter)"));
    m_quick->setChecked(settings.quickCapture);
    m_clearOnNext = new QCheckBox(tr("Start a new set on the next capture"));
    m_clearOnNext->setChecked(settings.clearOnNextCapture);
    kf->addRow(m_quick);
    kf->addRow(m_clearOnNext);
    addPage(keys, tr("Capture"));

    // --- Вставка ---
    auto *paste = new QWidget;
    auto *pf = formPage(paste);
    m_mode = new QComboBox;
    m_mode->addItem(tr("Sequential: shots one by one, then text"), int(PasteMode::Sequential));
    m_mode->addItem(tr("Files + text in one clipboard entry"), int(PasteMode::FilesAndText));
    m_mode->addItem(tr("One combined image + text"), int(PasteMode::Combined));
    m_mode->setCurrentIndex(m_mode->findData(int(settings.pasteMode)));
    pf->addRow(tr("Mode:"), m_mode);
    m_delay = new QSpinBox;
    m_delay->setRange(100, 3000);
    m_delay->setSingleStep(50);
    m_delay->setSuffix(tr(" ms"));
    m_delay->setValue(settings.imageDelayMs);
    pf->addRow(tr("Delay between images:"), m_delay);
    m_restore = new QCheckBox(tr("Restore the clipboard after pasting"));
    m_restore->setChecked(settings.restoreClipboard);
    m_clearAfter = new QCheckBox(tr("Clear the set after pasting"));
    m_clearAfter->setChecked(settings.clearAfterPaste);
    pf->addRow(m_restore);
    pf->addRow(m_clearAfter);
    addPage(paste, tr("Paste"));

    // --- Сообщение ---
    auto *msg = new QWidget;
    auto *mf = formPage(msg);
    m_language = new QComboBox;
    m_language->addItem(tr("Like the system"), QStringLiteral("auto"));
    m_language->addItem(QStringLiteral("Русский"), QStringLiteral("ru"));
    m_language->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_language->setCurrentIndex(std::max(0, m_language->findData(settings.templateLanguage)));
    mf->addRow(tr("Message language:"), m_language);
    m_header = new QLineEdit(settings.shotHeader);
    m_header->setPlaceholderText(tr("default: Shot {letter}"));
    m_line = new QLineEdit(settings.markLine);
    m_line->setPlaceholderText(QStringLiteral("{id} {text}"));
    m_intro = new QLineEdit(settings.intro);
    m_intro->setPlaceholderText(tr("e.g. Fix these points:"));
    m_outro = new QLineEdit(settings.outro);
    mf->addRow(tr("Shot header:"), m_header);
    mf->addRow(tr("Mark line:"), m_line);
    mf->addRow(tr("Intro:"), m_intro);
    mf->addRow(tr("Outro:"), m_outro);
    m_textFirst = new QCheckBox(tr("Text before the images"));
    m_textFirst->setChecked(settings.textBeforeImages);
    mf->addRow(m_textFirst);
    m_captions = new QCheckBox(tr("Also write comments on the image, next to the numbers"));
    m_captions->setToolTip(tr("For chats where text gets separated from images. The message text stays as is."));
    m_captions->setChecked(settings.captionsOnImage);
    mf->addRow(m_captions);
    addPage(msg, tr("Message"));

    // --- Вид ---
    auto *look = new QWidget;
    auto *lf = formPage(look);
    m_accent = dialogButton(QString());
    setButtonColor(m_accent, m_accentColor);
    connect(m_accent, &QPushButton::clicked, this, [this] {
        const QColor c = QColorDialog::getColor(m_accentColor, this, tr("Mark color"));
        if (c.isValid()) {
            m_accentColor = c;
            setButtonColor(m_accent, c);
        }
    });
    auto *accentRow = new QHBoxLayout;
    accentRow->addWidget(m_accent);
    accentRow->addStretch();
    // QFormLayout ставит подпись к верху поля: у высокой кнопки цвета — центруем сами
    auto *accentLabel = new QLabel(tr("Mark color:"));
    accentLabel->setMinimumHeight(36);
    accentLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    lf->addRow(accentLabel, accentRow);
    lf->addRow(noteLabel(tr("The same color marks the selected tool, the main buttons and the shot letters.")));
    m_halo = new QCheckBox(tr("Thin dark edge around mark lines"));
    m_halo->setToolTip(tr("Keeps a mark visible on a background of its own color and on light shots"));
    m_halo->setChecked(settings.markHalo);
    lf->addRow(m_halo);
    m_badgeSize = new QSpinBox;
    m_badgeSize->setRange(16, 48);
    m_badgeSize->setSuffix(tr(" px"));
    m_badgeSize->setValue(settings.badgeSize);
    lf->addRow(tr("Number badge size:"), m_badgeSize);
    m_uiLanguage = new QComboBox;
    m_uiLanguage->addItem(tr("Like the system"), QStringLiteral("auto"));
    m_uiLanguage->addItem(QStringLiteral("Русский"), QStringLiteral("ru"));
    m_uiLanguage->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_uiLanguage->setCurrentIndex(std::max(0, m_uiLanguage->findData(settings.uiLanguage)));
    lf->addRow(tr("Interface language:"), m_uiLanguage);
    m_hideEmpty = new QCheckBox(tr("Hide the strip to the tray when the set is empty"));
    m_hideEmpty->setChecked(settings.hideStripWhenEmpty);
    lf->addRow(m_hideEmpty);
    addPage(look, tr("Appearance"));

    // --- Изображения (PST-7) ---
    auto *images = new QWidget;
    auto *imf = formPage(images);
    m_maxSide = new QSpinBox;
    m_maxSide->setRange(0, 16384);
    m_maxSide->setSingleStep(256);
    m_maxSide->setSuffix(tr(" px"));
    m_maxSide->setSpecialValueText(tr("no limit"));
    m_maxSide->setValue(settings.maxLongSide);
    imf->addRow(tr("Shrink the long side to:"), m_maxSide);
    m_jpegThreshold = new QSpinBox;
    m_jpegThreshold->setRange(0, 100);
    m_jpegThreshold->setSuffix(tr(" MB"));
    m_jpegThreshold->setSpecialValueText(tr("never, always PNG"));
    m_jpegThreshold->setValue(settings.jpegThresholdMb);
    imf->addRow(tr("Switch to JPEG when PNG is larger than:"), m_jpegThreshold);
    m_jpegQuality = new QSpinBox;
    m_jpegQuality->setRange(30, 100);
    m_jpegQuality->setSuffix(tr(" %"));
    m_jpegQuality->setValue(settings.jpegQuality);
    imf->addRow(tr("JPEG quality:"), m_jpegQuality);
    connect(m_jpegThreshold, &QSpinBox::valueChanged, m_jpegQuality, [this](int v) { m_jpegQuality->setEnabled(v > 0); });
    m_jpegQuality->setEnabled(settings.jpegThresholdMb > 0);
    imf->addRow(noteLabel(tr("Applies to pasting, copying and dragging into a chat. "
                             "\"Save to folder\" always writes full-size PNG.")));
    addPage(images, tr("Images"));

    // --- Система: что доступно здесь ---
    auto *sys = new QWidget;
    auto *sl = new QVBoxLayout(sys);
    sl->setContentsMargins(2, 14, 2, 2);
    sl->setSpacing(10);
    if (isAutostartSupported()) {
        m_autostart = new QCheckBox(tr("Start Markpaste when I log in"));
        m_autostart->setChecked(isAutostartEnabled(QStringLiteral(MARKPASTE_APP_ID)));
        sl->addWidget(m_autostart);
        sl->addSpacing(8);
    }
    m_keepSet = new QCheckBox(tr("Keep the set after restart"));
    m_keepSet->setToolTip(tr("Shots and marks are stored in %1 (only you can read it)").arg(sessionDirectory()));
    m_keepSet->setChecked(settings.keepSetOnRestart);
    sl->addWidget(m_keepSet);
    m_checkUpdates = new QCheckBox(tr("Check for a new version once a day"));
    m_checkUpdates->setToolTip(tr("The only network request of the program. Updates are installed by the package manager."));
    m_checkUpdates->setChecked(settings.checkUpdates);
    sl->addWidget(m_checkUpdates);
    auto *logs = dialogButton(tr("Open the log folder"));
    connect(logs, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(LogFile::directory()));
    });
    sl->addWidget(logs, 0, Qt::AlignLeft);
    sl->addSpacing(8);
    QString info = QStringLiteral("<b>%1</b><br>").arg(platform.environment.toHtmlEscaped());
    auto line = [](const QString &what, const QString &how) {
        return QStringLiteral("%1: %2<br>").arg(what.toHtmlEscaped(), how.isEmpty() ? QStringLiteral("—") : how.toHtmlEscaped());
    };
    info += line(tr("Screen capture"), platform.capture ? platform.capture->name() : QString());
    info += line(tr("Hotkeys"), platform.hotkeys ? platform.hotkeys->name() : QString());
    info += line(tr("Clipboard"), platform.clipboard ? platform.clipboard->name() : QString());
    info += line(tr("Ctrl+V emulation"), platform.injector ? platform.injector->name() : QString());
    auto *infoCard = new Card;
    auto *infoLayout = new QVBoxLayout(infoCard);
    infoLayout->setContentsMargins(14, 12, 14, 12);
    infoLayout->setSpacing(8);
    auto *infoLabel = new QLabel(info);
    infoLabel->setWordWrap(true);
    infoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    infoLayout->addWidget(infoLabel);
    for (const QString &n : platform.notes) {
        auto *note = new StatusLine;
        note->setState(StatusLine::State::Warning, n);
        infoLayout->addWidget(note);
    }
    sl->addWidget(infoCard);
    sl->addStretch();
    addPage(sys, tr("System"));

    // «Отмена» и главная «Сохранить»; Enter — сохранить, Esc — отмена
    auto *cancel = dialogButton(tr("Cancel"));
    auto *save = new ActionButton(tr("Save"), ActionButton::Kind::Primary);
    save->setFocusPolicy(Qt::StrongFocus);
    save->setDefault(true);
    connect(save, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    buttons->addStretch();
    buttons->addWidget(cancel);
    buttons->addWidget(save);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(4);
    tabRow->addStretch();
    layout->addLayout(tabRow);
    layout->addWidget(pages, 1);
    layout->addSpacing(8);
    layout->addLayout(buttons);
    resize(640, 460);
}

bool SettingsDialog::autostart() const
{
    return m_autostart && m_autostart->isChecked();
}

Settings SettingsDialog::settings() const
{
    Settings s = m_settings;
    if (!m_systemKeys) {
        s.captureHotkey = m_captureKey->text().trimmed();
        s.pasteHotkey = m_pasteKey->text().trimmed();
    }
    s.quickCapture = m_quick->isChecked();
    s.clearOnNextCapture = m_clearOnNext->isChecked();
    s.pasteMode = PasteMode(m_mode->currentData().toInt());
    s.imageDelayMs = m_delay->value();
    s.restoreClipboard = m_restore->isChecked();
    s.clearAfterPaste = m_clearAfter->isChecked();
    s.templateLanguage = m_language->currentData().toString();
    s.shotHeader = m_header->text();
    s.markLine = m_line->text();
    s.intro = m_intro->text();
    s.outro = m_outro->text();
    s.textBeforeImages = m_textFirst->isChecked();
    s.accent = m_accentColor;
    s.hideStripWhenEmpty = m_hideEmpty->isChecked();
    s.captionsOnImage = m_captions->isChecked();
    s.markHalo = m_halo->isChecked();
    s.badgeSize = m_badgeSize->value();
    s.uiLanguage = m_uiLanguage->currentData().toString();
    s.maxLongSide = m_maxSide->value();
    s.jpegThresholdMb = m_jpegThreshold->value();
    s.jpegQuality = m_jpegQuality->value();
    s.keepSetOnRestart = m_keepSet->isChecked();
    s.checkUpdates = m_checkUpdates->isChecked();
    return s;
}

} // namespace mp
