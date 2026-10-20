#include "SettingsDialog.h"

#include "platform/Autostart.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace mp {

namespace {
void setButtonColor(QPushButton *b, const QColor &c)
{
    b->setText(c.name().toUpper());
    b->setStyleSheet(QStringLiteral("QPushButton { background: %1; color: white; font-weight: bold; }").arg(c.name()));
}
} // namespace

SettingsDialog::SettingsDialog(const Settings &settings, const Platform &platform,
                               const QHash<Action, QString> &boundTriggers, QWidget *parent)
    : QDialog(parent), m_settings(settings), m_accentColor(settings.accent)
{
    setWindowTitle(tr("Markpaste settings"));
    auto *tabs = new QTabWidget;

    // --- Клавиши и захват ---
    auto *keys = new QWidget;
    auto *kf = new QFormLayout(keys);
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
    auto *keysNote = new QLabel(
        m_systemKeys
            ? tr("Shortcuts are assigned by the desktop. Change them with the button below "
                 "or in the system shortcut settings.")
            : tr("Format: Primary+Alt+V, Print. Primary is Ctrl on Windows and Linux."));
    keysNote->setWordWrap(true);
    keysNote->setForegroundRole(QPalette::PlaceholderText);
    kf->addRow(keysNote);
    if (platform.hotkeys && platform.hotkeys->canOpenSystemSettings()) {
        auto *sys = new QPushButton(tr("Change shortcuts…"));
        IHotkeys *hk = platform.hotkeys.get();
        connect(sys, &QPushButton::clicked, this, [hk] { hk->openSystemSettings(); });
        kf->addRow(sys);
    }
    m_quick = new QCheckBox(tr("Confirm the area on mouse release (otherwise — Enter)"));
    m_quick->setChecked(settings.quickCapture);
    m_clearOnNext = new QCheckBox(tr("Start a new set on the next capture"));
    m_clearOnNext->setChecked(settings.clearOnNextCapture);
    kf->addRow(m_quick);
    kf->addRow(m_clearOnNext);
    tabs->addTab(keys, tr("Capture"));

    // --- Вставка ---
    auto *paste = new QWidget;
    auto *pf = new QFormLayout(paste);
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
    tabs->addTab(paste, tr("Paste"));

    // --- Сообщение ---
    auto *msg = new QWidget;
    auto *mf = new QFormLayout(msg);
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
    tabs->addTab(msg, tr("Message"));

    // --- Вид ---
    auto *look = new QWidget;
    auto *lf = new QFormLayout(look);
    m_accent = new QPushButton;
    setButtonColor(m_accent, m_accentColor);
    connect(m_accent, &QPushButton::clicked, this, [this] {
        const QColor c = QColorDialog::getColor(m_accentColor, this, tr("Mark color"));
        if (c.isValid()) {
            m_accentColor = c;
            setButtonColor(m_accent, c);
        }
    });
    lf->addRow(tr("Mark color:"), m_accent);
    tabs->addTab(look, tr("Appearance"));

    // --- Система: что доступно здесь ---
    auto *sys = new QWidget;
    auto *sl = new QVBoxLayout(sys);
    if (isAutostartSupported()) {
        m_autostart = new QCheckBox(tr("Start Markpaste when I log in"));
        m_autostart->setChecked(isAutostartEnabled(QStringLiteral(MARKPASTE_APP_ID)));
        sl->addWidget(m_autostart);
        sl->addSpacing(8);
    }
    QString info = QStringLiteral("<b>%1</b><br>").arg(platform.environment.toHtmlEscaped());
    auto line = [](const QString &what, const QString &how) {
        return QStringLiteral("%1: %2<br>").arg(what.toHtmlEscaped(), how.isEmpty() ? QStringLiteral("—") : how.toHtmlEscaped());
    };
    info += line(tr("Capture"), platform.capture ? platform.capture->name() : QString());
    info += line(tr("Hotkeys"), platform.hotkeys ? platform.hotkeys->name() : QString());
    info += line(tr("Clipboard"), platform.clipboard ? platform.clipboard->name() : QString());
    info += line(tr("Ctrl+V emulation"), platform.injector ? platform.injector->name() : QString());
    for (const QString &n : platform.notes)
        info += QStringLiteral("<br>⚠ %1").arg(n.toHtmlEscaped());
    auto *infoLabel = new QLabel(info);
    infoLabel->setWordWrap(true);
    infoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    sl->addWidget(infoLabel);
    sl->addStretch();
    tabs->addTab(sys, tr("System"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(tabs);
    layout->addWidget(buttons);
    resize(560, 420);
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
    return s;
}

} // namespace mp
