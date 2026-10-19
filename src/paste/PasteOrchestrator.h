#pragma once

#include "core/CaptureSet.h"
#include "core/MessageBuilder.h"
#include "platform/Interfaces.h"
#include "render/ShotRenderer.h"

#include <QObject>
#include <QPointer>

namespace mp {

enum class PasteMode {
    Sequential = 1,     // режим 1: картинки по одной, потом текст, Ctrl+V за пользователя
    FilesAndText = 2,   // режим 2: одна запись буфера — файлы, текст и первая картинка
    Combined = 4,       // режим 4: склейка в одну картинку + текст (последовательно)
};

// Задержки профиля приложения (ТЗ разд. 10). Значения — стартовые, уточняются на прототипе.
struct PasteTiming {
    int settleMs = 80;            // запись в буфер → Ctrl+V
    int afterImageMs = 400;       // после Ctrl+V картинки
    int afterTextMs = 400;        // после последнего Ctrl+V — до закрытия сессии ввода
    int restoreAfterMs = 1000;    // потом — восстановление буфера (PST-8)
    int releaseTimeoutMs = 1500;  // сколько ждать отпускания горячей клавиши
    int afterReleaseMs = 50;
};

struct PasteOptions {
    PasteMode mode = PasteMode::Sequential;
    MessageTemplate messageTemplate;
    RenderStyle style;
    EncodeOptions encode;
    PasteTiming timing;
    bool restoreClipboard = true;
    bool waitHotkeyRelease = false;   // вставку запустила горячая клавиша — ждём её отпускания
};

// Вставка набора (PST-1…PST-9). Работает только через IClipboard и IInputInjector,
// ничего не знает об ОС. Никогда не нажимает Enter (PST-9).
//
// Последовательная вставка — конечный автомат на future и таймерах в главном потоке:
// запечь всё (пул потоков) → сохранить буфер → открыть сессию ввода → дождаться
// отпускания клавиш → [записать картинку → пауза → Ctrl+V → пауза] × N → текст →
// пауза → закрыть сессию → пауза → вернуть буфер.
class PasteOrchestrator : public QObject
{
    Q_OBJECT
public:
    PasteOrchestrator(IClipboard *clipboard, IInputInjector *injector, QObject *parent = nullptr);

    bool isRunning() const { return m_running; }
    // Какие режимы возможны на этой платформе прямо сейчас.
    bool canSequential() const;

    void start(const CaptureSet &set, const PasteOptions &options);
    void cancel();

    // Сообщить, что горячая клавиша отпущена (IHotkeys::released).
    void hotkeyReleased();

    // Готовые элементы для режимов — публичны для перетаскивания и тестов.
    struct Item {
        ClipboardPayload payload;
        QString what;   // для прогресса и логов: "A", "B", "text"
    };
    static QList<Item> bakeSequence(const CaptureSet &set, const PasteOptions &options);
    static ClipboardPayload filesAndTextPayload(const CaptureSet &set, const PasteOptions &options, QStringList *files);

signals:
    void progress(int done, int total);
    void finished(mp::Status status, mp::PasteMode mode);

private:
    void runSequence(QList<Item> items);
    void step();
    void finish(const Status &status);
    void after(int ms, std::function<void()> fn);

    QPointer<IClipboard> m_clipboard;
    QPointer<IInputInjector> m_injector;
    PasteOptions m_options;
    QList<Item> m_items;
    int m_index = 0;
    ClipboardPayload m_saved;
    bool m_running = false;
    bool m_cancelled = false;
    bool m_sessionOpen = false;
    bool m_released = false;
    std::function<void()> m_onReleased;
    quint64 m_generation = 0;   // защищает от отложенных колбэков прошлого запуска
};

} // namespace mp
