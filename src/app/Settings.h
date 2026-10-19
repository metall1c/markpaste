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

    // Вид и картинки
    QColor accent = QColor(0xFF, 0x5A, 0x1F);
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

} // namespace mp
