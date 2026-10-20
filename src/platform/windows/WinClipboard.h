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
class WinClipboard : public QtClipboard
{
    Q_OBJECT
public:
    explicit WinClipboard(QObject *parent = nullptr)
        : QtClipboard(true, parent)
    {
    }
    QString name() const override { return QStringLiteral("Windows clipboard (OLE)"); }
    QFuture<ClipboardPayload> snapshot() override;

    // Формат QMimeData для зарегистрированного формата буфера Windows с этим именем
    static QString windowsFormat(const QString &name);

protected:
    void addPlatformFormats(QMimeData *md, const ClipboardPayload &payload, ClipboardFlags flags) const override;
};

} // namespace mp::win
