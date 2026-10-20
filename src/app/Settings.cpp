#include "Settings.h"

#include <QSettings>
#include <QStandardPaths>

#include <algorithm>

namespace mp {

Settings Settings::load()
{
    Settings d;
    Settings s;
    QSettings q;
    s.captureHotkey = q.value(QStringLiteral("hotkeys/capture"), d.captureHotkey).toString();
    s.pasteHotkey = q.value(QStringLiteral("hotkeys/paste"), d.pasteHotkey).toString();
    s.quickCapture = q.value(QStringLiteral("capture/quick"), d.quickCapture).toBool();
    s.clearOnNextCapture = q.value(QStringLiteral("capture/clearOnNext"), d.clearOnNextCapture).toBool();
    const int mode = q.value(QStringLiteral("paste/mode"), int(d.pasteMode)).toInt();
    s.pasteMode = (mode == 2) ? PasteMode::FilesAndText : (mode == 4) ? PasteMode::Combined : PasteMode::Sequential;
    s.restoreClipboard = q.value(QStringLiteral("paste/restoreClipboard"), d.restoreClipboard).toBool();
    s.clearAfterPaste = q.value(QStringLiteral("paste/clearAfter"), d.clearAfterPaste).toBool();
    s.imageDelayMs = q.value(QStringLiteral("paste/imageDelayMs"), d.imageDelayMs).toInt();
    s.templateLanguage = q.value(QStringLiteral("message/language"), d.templateLanguage).toString();
    s.shotHeader = q.value(QStringLiteral("message/shotHeader")).toString();
    s.markLine = q.value(QStringLiteral("message/markLine")).toString();
    s.intro = q.value(QStringLiteral("message/intro")).toString();
    s.outro = q.value(QStringLiteral("message/outro")).toString();
    s.textBeforeImages = q.value(QStringLiteral("message/textBeforeImages"), d.textBeforeImages).toBool();
    s.accent = QColor(q.value(QStringLiteral("appearance/accent"), d.accent.name()).toString());
    if (!s.accent.isValid())
        s.accent = d.accent;
    s.keepSetOnRestart = q.value(QStringLiteral("session/keep"), d.keepSetOnRestart).toBool();
    s.checkUpdates = q.value(QStringLiteral("updates/check"), d.checkUpdates).toBool();
    s.hideStripWhenEmpty = q.value(QStringLiteral("strip/hideWhenEmpty"), d.hideStripWhenEmpty).toBool();
    s.markPen.color = QColor(q.value(QStringLiteral("editor/markColor")).toString());   // пусто — акцент
    s.markPen.width = std::clamp(q.value(QStringLiteral("editor/markWidth"), 0.0).toDouble(), 0.0, 20.0);
    const QString line = q.value(QStringLiteral("editor/markLine")).toString();
    s.markPen.line = line == u"dash" ? LineStyle::Dash : line == u"dot" ? LineStyle::Dot : LineStyle::Solid;
    s.maxLongSide = q.value(QStringLiteral("images/maxLongSide"), d.maxLongSide).toInt();
    s.jpegThresholdMb = q.value(QStringLiteral("images/jpegThresholdMb"), d.jpegThresholdMb).toInt();
    s.jpegQuality = q.value(QStringLiteral("images/jpegQuality"), d.jpegQuality).toInt();
    return s;
}

void Settings::save() const
{
    QSettings q;
    q.setValue(QStringLiteral("hotkeys/capture"), captureHotkey);
    q.setValue(QStringLiteral("hotkeys/paste"), pasteHotkey);
    q.setValue(QStringLiteral("capture/quick"), quickCapture);
    q.setValue(QStringLiteral("capture/clearOnNext"), clearOnNextCapture);
    q.setValue(QStringLiteral("paste/mode"), int(pasteMode));
    q.setValue(QStringLiteral("paste/restoreClipboard"), restoreClipboard);
    q.setValue(QStringLiteral("paste/clearAfter"), clearAfterPaste);
    q.setValue(QStringLiteral("paste/imageDelayMs"), imageDelayMs);
    q.setValue(QStringLiteral("message/language"), templateLanguage);
    q.setValue(QStringLiteral("message/shotHeader"), shotHeader);
    q.setValue(QStringLiteral("message/markLine"), markLine);
    q.setValue(QStringLiteral("message/intro"), intro);
    q.setValue(QStringLiteral("message/outro"), outro);
    q.setValue(QStringLiteral("message/textBeforeImages"), textBeforeImages);
    q.setValue(QStringLiteral("appearance/accent"), accent.name());
    q.setValue(QStringLiteral("strip/hideWhenEmpty"), hideStripWhenEmpty);
    q.setValue(QStringLiteral("session/keep"), keepSetOnRestart);
    q.setValue(QStringLiteral("updates/check"), checkUpdates);
    q.setValue(QStringLiteral("editor/markColor"), markPen.color.isValid() ? markPen.color.name() : QString());
    q.setValue(QStringLiteral("editor/markWidth"), markPen.width);
    q.setValue(QStringLiteral("editor/markLine"), markPen.line == LineStyle::Dash  ? QStringLiteral("dash")
                                                  : markPen.line == LineStyle::Dot ? QStringLiteral("dot")
                                                                                   : QStringLiteral("solid"));
    q.setValue(QStringLiteral("images/maxLongSide"), maxLongSide);
    q.setValue(QStringLiteral("images/jpegThresholdMb"), jpegThresholdMb);
    q.setValue(QStringLiteral("images/jpegQuality"), jpegQuality);
}

MessageTemplate Settings::messageTemplate() const
{
    QLocale locale;
    if (templateLanguage == u"ru")
        locale = QLocale(QLocale::Russian);
    else if (templateLanguage == u"en")
        locale = QLocale(QLocale::English);
    MessageTemplate t = MessageTemplate::defaultFor(locale);
    if (!shotHeader.isEmpty())
        t.shotHeader = shotHeader;
    if (!markLine.isEmpty())
        t.markLine = markLine;
    t.intro = intro;
    t.outro = outro;
    t.textBeforeImages = textBeforeImages;
    return t;
}

RenderStyle Settings::renderStyle() const
{
    RenderStyle st;
    st.accent = accent;
    return st;
}

PasteOptions Settings::pasteOptions() const
{
    PasteOptions o;
    o.mode = pasteMode;
    o.messageTemplate = messageTemplate();
    o.style = renderStyle();
    o.encode.maxLongSide = maxLongSide;
    o.encode.jpegThresholdBytes = qint64(jpegThresholdMb) * 1024 * 1024;
    o.encode.jpegQuality = jpegQuality;
    o.restoreClipboard = restoreClipboard;
    o.timing.afterImageMs = imageDelayMs;
    o.timing.afterTextMs = std::max(400, imageDelayMs);
    return o;
}

QString sessionDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/session");
}

} // namespace mp
