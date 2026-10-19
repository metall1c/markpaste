#pragma once

#include "platform/Interfaces.h"

namespace mp::lx {

// Эмуляция Ctrl+V на X11 через расширение XTEST (ТЗ разд. 9). Без диалога и без сессии.
// V нажимается по коду клавиши, на которой в раскладке есть «v» (ТЗ разд. 10): при русской
// раскладке Qt и GTK на X11 сами сопоставляют Ctrl+М с Ctrl+V по латинскому варианту клавиши.
class X11InputInjector : public IInputInjector
{
    Q_OBJECT
public:
    // nullptr, если Qt работает не через xcb или у сервера нет XTEST
    static X11InputInjector *create(QObject *parent = nullptr);

    bool isAvailable() const override { return true; }
    QFuture<Status> begin() override { return readyFuture(Status::success()); }
    void end() override {}
    // На X11 состояние клавиатуры видно (QueryKeymap): ждём, пока отпустят модификаторы
    QFuture<Status> waitModifiersReleased(std::chrono::milliseconds timeout) override;
    QFuture<Status> sendPaste() override;
    bool canDetectForegroundChange() const override { return false; }
    QString name() const override { return QStringLiteral("X11 XTEST"); }

private:
    using IInputInjector::IInputInjector;
    void key(uint8_t keycode, bool pressed);

    uint8_t m_ctrl = 0;
    uint8_t m_v = 0;
};

} // namespace mp::lx
