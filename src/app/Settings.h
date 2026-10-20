#pragma once

#include "core/MessageBuilder.h"
#include "paste/PasteOrchestrator.h"
#include "render/ShotRenderer.h"

#include <QString>

namespace mp {

// Настройки программы (APP-3, ТЗ разд. 11). Хранятся в QSettings (INI) в каталоге
// конфигурации пользователя. Сочетания — в переносимом виде (Primary+Alt+V).
struct Settings {
    // Клавиши
    QString captureHotkey = QStringLiteral("Print");
    QString pasteHotkey = QStringLiteral("Primary+Alt+V");   // не Ctrl+Shift+V (PST-2)

    // Захват
    bool quickCapture = true;          // CAP-4: подтверждение отпусканием мыши
    bool clearOnNextCapture = false;   // ТЗ разд. 4, шаг 8

    // Вставка
    PasteMode pasteMode = PasteMode::Sequential;
    bool restoreClipboard = true;
    bool clearAfterPaste = false;
    int imageDelayMs = 400;

    // Сообщение
    QString templateLanguage = QStringLiteral("auto");   // auto / ru / en — отдельно от интерфейса (NFR-10)
    QString shotHeader;    // пусто — из шаблона языка
    QString markLine;
    QString intro;
    QString outro;
    bool textBeforeImages = false;
    bool captionsOnImage = false;      // EDT-10: комментарии подписями на картинке

    // Вид и картинки
    QColor accent = QColor(0xFF, 0x5A, 0x1F);
    int badgeSize = 28;                // диаметр значка номера, логические px
    QString uiLanguage = QStringLiteral("auto");   // auto / ru / en — язык интерфейса (APP-5), после перезапуска
    bool hideStripWhenEmpty = true;
    bool keepSetOnRestart = false;     // NFR-5: набор на диске, переживает перезапуск
    bool checkUpdates = true;          // APP-7, NFR-4: единственный сетевой запрос    // пустой набор — лента прячется в трей (ADR 0004)
    MarkPen markPen;                   // последнее перо редактора; пустые поля — по умолчанию
    int maxLongSide = 4096;
    int jpegThresholdMb = 5;
    int jpegQuality = 85;

    static Settings load();
    void save() const;

    MessageTemplate messageTemplate() const;
    PasteOptions pasteOptions() const;
    RenderStyle renderStyle() const;
};

// Каталог сохранённого набора (NFR-5): <AppLocalDataLocation>/session, на Linux —
// ~/.local/share/markpaste/session, права 0700
QString sessionDirectory();

} // namespace mp
