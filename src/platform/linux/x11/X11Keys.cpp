#include "X11Keys.h"

#include <QGuiApplication>
#include <QKeySequence>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

#include <cstdlib>
#include <memory>

namespace mp::lx {

xcb_connection_t *x11Connection()
{
    auto *x11 = qApp ? qApp->nativeInterface<QNativeInterface::QX11Application>() : nullptr;
    return x11 ? x11->connection() : nullptr;
}

uint32_t x11RootWindow()
{
    xcb_connection_t *c = x11Connection();
    if (!c)
        return 0;
    return xcb_setup_roots_iterator(xcb_get_setup(c)).data->root;
}

static uint32_t keysymForQtKey(int key)
{
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return uint32_t(key - Qt::Key_A + 'a');   // keysym латинских букв — строчные
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return uint32_t(key);
    if (key >= Qt::Key_F1 && key <= Qt::Key_F35)
        return 0xffbe + uint32_t(key - Qt::Key_F1);   // XK_F1
    switch (key) {
    case Qt::Key_Print: return 0xff61;
    case Qt::Key_SysReq: return 0xff15;
    case Qt::Key_Pause: return 0xff13;
    case Qt::Key_ScrollLock: return 0xff14;
    case Qt::Key_Insert: return 0xff63;
    case Qt::Key_Delete: return 0xffff;
    case Qt::Key_Home: return 0xff50;
    case Qt::Key_End: return 0xff57;
    case Qt::Key_PageUp: return 0xff55;
    case Qt::Key_PageDown: return 0xff56;
    case Qt::Key_Left: return 0xff51;
    case Qt::Key_Up: return 0xff52;
    case Qt::Key_Right: return 0xff53;
    case Qt::Key_Down: return 0xff54;
    case Qt::Key_Escape: return 0xff1b;
    case Qt::Key_Return: return 0xff0d;
    case Qt::Key_Enter: return 0xff8d;
    case Qt::Key_Tab: return 0xff09;
    case Qt::Key_Backspace: return 0xff08;
    case Qt::Key_Space: return 0x20;
    default:
        // Остальные печатные символы Latin-1: keysym совпадает с кодом символа
        return key > 0x20 && key < 0x100 ? uint32_t(key) : 0;
    }
}

X11Trigger parseTrigger(const QString &portable)
{
    QString s = portable.trimmed();
    s.replace(QStringLiteral("Primary"), QStringLiteral("Ctrl"), Qt::CaseInsensitive);
    s.replace(QStringLiteral("Super"), QStringLiteral("Meta"), Qt::CaseInsensitive);
    const QKeySequence seq = QKeySequence::fromString(s, QKeySequence::PortableText);
    if (seq.isEmpty())
        return {};
    const QKeyCombination combo = seq[0];
    X11Trigger t;
    t.keysym = keysymForQtKey(combo.key());
    const Qt::KeyboardModifiers m = combo.keyboardModifiers();
    if (m & Qt::ShiftModifier)
        t.modifiers |= XCB_MOD_MASK_SHIFT;
    if (m & Qt::ControlModifier)
        t.modifiers |= XCB_MOD_MASK_CONTROL;
    if (m & Qt::AltModifier)
        t.modifiers |= XCB_MOD_MASK_1;
    if (m & Qt::MetaModifier)
        t.modifiers |= XCB_MOD_MASK_4;
    return t;
}

QList<uint8_t> keycodesFor(uint32_t keysym)
{
    QList<uint8_t> out;
    xcb_connection_t *c = x11Connection();
    if (!c)
        return out;
    std::unique_ptr<xcb_key_symbols_t, decltype(&xcb_key_symbols_free)> syms(xcb_key_symbols_alloc(c),
                                                                             xcb_key_symbols_free);
    std::unique_ptr<xcb_keycode_t, decltype(&free)> codes(xcb_key_symbols_get_keycode(syms.get(), keysym), free);
    for (xcb_keycode_t *k = codes.get(); k && *k != XCB_NO_SYMBOL; ++k)
        if (!out.contains(*k))
            out << *k;
    return out;
}

static std::unique_ptr<xcb_query_keymap_reply_t, decltype(&free)> queryKeymap()
{
    xcb_connection_t *c = x11Connection();
    return {c ? xcb_query_keymap_reply(c, xcb_query_keymap(c), nullptr) : nullptr, free};
}

bool isKeyDown(uint8_t keycode)
{
    const auto km = queryKeymap();
    return km && (km->keys[keycode / 8] & (1 << (keycode % 8)));
}

bool anyModifierDown()
{
    xcb_connection_t *c = x11Connection();
    const auto km = queryKeymap();
    if (!c || !km)
        return false;
    std::unique_ptr<xcb_get_modifier_mapping_reply_t, decltype(&free)> mm(
        xcb_get_modifier_mapping_reply(c, xcb_get_modifier_mapping(c), nullptr), free);
    if (!mm)
        return false;
    const xcb_keycode_t *codes = xcb_get_modifier_mapping_keycodes(mm.get());
    const int n = xcb_get_modifier_mapping_keycodes_length(mm.get());
    // Индексы 0–7: Shift, Lock, Control, Mod1…Mod5. Lock (Caps) и Mod2 (NumLock) — фиксируемые, не считаем
    for (int i = 0; i < n; ++i) {
        const int modIndex = i / mm->keycodes_per_modifier;
        if (modIndex == 1 || modIndex == 4 || codes[i] == 0)
            continue;
        if (km->keys[codes[i] / 8] & (1 << (codes[i] % 8)))
            return true;
    }
    return false;
}

} // namespace mp::lx
