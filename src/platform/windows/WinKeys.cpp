#include "WinKeys.h"

#include <QKeySequence>

#include <windows.h>

namespace mp::win {

static unsigned vkForQtKey(int key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return unsigned('A' + (key - Qt::Key_A));   // VK букв и цифр совпадают с ASCII
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return unsigned('0' + (key - Qt::Key_0));
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
        return unsigned(VK_F1 + (key - Qt::Key_F1));
    switch (key) {
    case Qt::Key_Print: return VK_SNAPSHOT;
    case Qt::Key_SysReq: return VK_SNAPSHOT;
    case Qt::Key_Pause: return VK_PAUSE;
    case Qt::Key_ScrollLock: return VK_SCROLL;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Home: return VK_HOME;
    case Qt::Key_End: return VK_END;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    case Qt::Key_Left: return VK_LEFT;
    case Qt::Key_Up: return VK_UP;
    case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Down: return VK_DOWN;
    case Qt::Key_Escape: return VK_ESCAPE;
    case Qt::Key_Return:
    case Qt::Key_Enter: return VK_RETURN;
    case Qt::Key_Tab: return VK_TAB;
    case Qt::Key_Backspace: return VK_BACK;
    case Qt::Key_Space: return VK_SPACE;
    default:
        break;
    }
    // Остальные печатные символы — по текущей раскладке (`, -, =, [, …)
    if (key > 0x20 && key < 0x7f) {
        const SHORT scan = VkKeyScanW(wchar_t(key));
        if (scan != -1 && (scan >> 8) == 0)   // символ без Shift и AltGr — иначе это другое сочетание
            return unsigned(scan & 0xff);
    }
    return 0;
}

static QString normalized(const QString &portable)
{
    QString s = portable.trimmed();
    s.replace(QStringLiteral("Primary"), QStringLiteral("Ctrl"), Qt::CaseInsensitive);
    s.replace(QStringLiteral("Super"), QStringLiteral("Meta"), Qt::CaseInsensitive);
    s.replace(QStringLiteral("Win+"), QStringLiteral("Meta+"), Qt::CaseInsensitive);
    return s;
}

WinTrigger parseTrigger(const QString &portable)
{
    const QKeySequence seq = QKeySequence::fromString(normalized(portable), QKeySequence::PortableText);
    if (seq.isEmpty())
        return {};
    const QKeyCombination combo = seq[0];
    WinTrigger t;
    t.vk = vkForQtKey(combo.key());
    const Qt::KeyboardModifiers m = combo.keyboardModifiers();
    if (m & Qt::ShiftModifier)
        t.modifiers |= MOD_SHIFT;
    if (m & Qt::ControlModifier)
        t.modifiers |= MOD_CONTROL;
    if (m & Qt::AltModifier)
        t.modifiers |= MOD_ALT;
    if (m & Qt::MetaModifier)
        t.modifiers |= MOD_WIN;
    return t;
}

QString displayTrigger(const QString &portable)
{
    QString s = portable.trimmed();
    s.replace(QStringLiteral("Primary"), QStringLiteral("Ctrl"), Qt::CaseInsensitive);
    s.replace(QStringLiteral("Meta"), QStringLiteral("Win"), Qt::CaseInsensitive);
    s.replace(QStringLiteral("Super"), QStringLiteral("Win"), Qt::CaseInsensitive);
    return s;
}

bool isKeyDown(unsigned vk)
{
    return GetAsyncKeyState(int(vk)) & 0x8000;
}

bool modifiersDown(unsigned modifiers)
{
    return ((modifiers & MOD_SHIFT) && isKeyDown(VK_SHIFT)) || ((modifiers & MOD_CONTROL) && isKeyDown(VK_CONTROL))
        || ((modifiers & MOD_ALT) && isKeyDown(VK_MENU))
        || ((modifiers & MOD_WIN) && (isKeyDown(VK_LWIN) || isKeyDown(VK_RWIN)));
}

bool modifiersDownAll(unsigned modifiers)
{
    for (unsigned m : {unsigned(MOD_SHIFT), unsigned(MOD_CONTROL), unsigned(MOD_ALT), unsigned(MOD_WIN)})
        if ((modifiers & m) && !modifiersDown(m))
            return false;
    return true;
}

bool anyModifierDown()
{
    return modifiersDown(MOD_SHIFT | MOD_CONTROL | MOD_ALT | MOD_WIN);
}

} // namespace mp::win
