#pragma once

#include "platform/common/QtClipboard.h"

namespace mp::win {

// Буфер обмена Windows через QClipboard (OLE). Qt сам раскладывает запись по форматам
// Windows: картинку — в CF_DIB и CF_DIBV5, список файлов — в CF_HDROP, текст — в
// CF_UNICODETEXT. Пишет из фона: фокус окну для этого не нужен.
//
// Формат «PNG» (его первым читают Chrome, Telegram, Office: в нём есть прозрачность и нет
// потерь) Qt сам не кладёт — добавляем байты PNG как есть. При чтении (снимок буфера перед
// вставкой) PNG и CF_DIB берём сами, если Qt картинку не разобрал.
//
// «Не в историю» (NFR-4, ТЗ разд. 10) — три зарегистрированных формата, которые читают
// журнал буфера Win+V, облачная синхронизация и программы-мониторы буфера:
// ExcludeClipboardContentFromMonitorProcessing, CanIncludeInClipboardHistory = 0,
// CanUploadToCloudClipboard = 0.
//
// Занятый буфер. Пока другая программа держит буфер открытым (rdpclip в сеансе RDP и менеджеры
// буфера открывают его после каждой смены), OleSetClipboard не пишет, а Qt только оставляет
// предупреждение в журнале. Без проверки «Скопировано» было бы неправдой, а Ctrl+V вставил бы
// прежнее содержимое буфера (docs/problems.md, 2026-10-20). Поэтому после записи проверяем, что
// буфер наш, и повторяем, пока он не освободится; перед снимком буфера ждём того же.
class WinClipboard : public QtClipboard
{
    Q_OBJECT
public:
    explicit WinClipboard(QObject *parent = nullptr);
    ~WinClipboard() override;
    QString name() const override { return QStringLiteral("Windows clipboard (OLE)"); }
    QFuture<Status> write(const ClipboardPayload &payload, ClipboardFlags flags) override;
    QFuture<ClipboardPayload> snapshot() override;

    // Формат QMimeData для зарегистрированного формата буфера Windows с этим именем
    static QString windowsFormat(const QString &name);

    // Сколько ждать, пока другая программа отпустит буфер (по умолчанию ~2 с)
    void setBusyWait(int attempts, int intervalMs)
    {
        m_busyAttempts = attempts;
        m_busyIntervalMs = intervalMs;
    }

protected:
    bool keepEntry(const QString &mimeType) const override;
    void addPlatformFormats(QMimeData *md, const ClipboardPayload &payload, ClipboardFlags flags) const override;

private:
    ClipboardPayload readNow();
    bool clipboardFree() const;

    void *m_probeWindow = nullptr;   // HWND: открывать буфер для проверки от своего окна, а не от NULL
    quint64 m_writeGeneration = 0;   // новая запись отменяет повторы прежней, ещё ждущей буфер
    int m_busyAttempts = 20;
    int m_busyIntervalMs = 100;
};

} // namespace mp::win
