#pragma once

#include <QByteArray>
#include <QFuture>
#include <QImage>
#include <QKeySequence>
#include <QList>
#include <QPromise>
#include <QRect>
#include <QString>
#include <QStringList>

namespace mp {

// Результат асинхронной платформенной операции. Ошибки возвращаем значением,
// а не исключением: на Wayland отказ пользователя в диалоге портала — обычный исход.
struct Status {
    bool ok = true;
    QString error;

    static Status success() { return {}; }
    static Status failure(QString message) { return {false, std::move(message)}; }
    explicit operator bool() const { return ok; }
};

template <typename T>
QFuture<T> readyFuture(T value)
{
    QPromise<T> promise;
    promise.start();
    promise.addResult(std::move(value));
    promise.finish();
    return promise.future();
}

// --- Захват ---

struct ScreenFrame {
    QString screenName;     // имя выхода: совпадает с QScreen::name()
    QRect logicalGeometry;  // положение монитора в логических координатах рабочего стола
    qreal scale = 1.0;      // физические пиксели / логические
    QImage image;           // физические пиксели
};

struct CaptureResult {
    Status status;
    QList<ScreenFrame> frames;
};

enum class CaptureCap {
    None = 0,
    WindowUnderCursor = 1 << 0,   // CAP-5
    Silent = 1 << 1,              // без диалога и вспышки
};
Q_DECLARE_FLAGS(CaptureCaps, CaptureCap)
Q_DECLARE_OPERATORS_FOR_FLAGS(CaptureCaps)

// --- Клавиши ---

enum class Action {
    Capture,
    PasteSet,
    NewShot,
    ClearSet,
};

struct BindResult {
    Status status;
    QString trigger;   // фактическое сочетание; на Wayland его подтверждает пользователь
};

struct ActivationContext {
    QString activationToken;   // xdg-activation, если платформа его дала
};

// --- Буфер обмена ---

struct ClipboardEntry {
    QString mimeType;
    QByteArray data;
};

// Одна запись буфера в нескольких форматах. Порядок — по убыванию предпочтения.
struct ClipboardPayload {
    QList<ClipboardEntry> entries;

    bool isEmpty() const { return entries.isEmpty(); }
    QStringList mimeTypes() const;
    const ClipboardEntry *find(const QString &mimeType) const;

    // Текст со всеми принятыми синонимами MIME-типа (text/plain;charset=utf-8, UTF8_STRING…).
    void addText(const QString &text);
    void addImage(const QByteArray &encoded, const QByteArray &mimeType = "image/png");
    static ClipboardPayload text(const QString &text);
    static ClipboardPayload image(const QByteArray &encoded, const QByteArray &mimeType = "image/png");
};

enum class ClipboardFlag {
    None = 0,
    ExcludeFromHistory = 1 << 0,   // не в историю и не в облако (NFR-4)
};
Q_DECLARE_FLAGS(ClipboardFlags, ClipboardFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(ClipboardFlags)

} // namespace mp
