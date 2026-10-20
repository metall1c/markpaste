#pragma once

#include "platform/Interfaces.h"

#include <windows.h>

namespace mp::win {

// Эмуляция Ctrl+V через SendInput (ТЗ разд. 9, 10). Без диалога и без сессии.
//
// V нажимается по виртуальному коду VK_V (ТЗ разд. 10): программы Windows сверяют сочетание
// по VK, и при русской раскладке Ctrl+V тоже вставляет.
//
// Окно, активное в начале вставки, запоминаем: если пользователь переключился на другое,
// вставка останавливается, а не сыплет картинки куда попало (ТЗ разд. 10, шаг 6).
//
// UIPI: SendInput молча не доходит до окон, запущенных от администратора, если сама
// программа запущена без повышения, и ошибку не возвращает. Поэтому уровень целостности
// окна проверяем заранее и сообщаем об этом сразу.
class WinInputInjector : public IInputInjector
{
    Q_OBJECT
public:
    explicit WinInputInjector(QObject *parent = nullptr);

    bool isAvailable() const override { return true; }
    QFuture<Status> begin() override;
    void end() override;
    QFuture<Status> waitModifiersReleased(std::chrono::milliseconds timeout) override;
    QFuture<Status> sendPaste() override;
    QFuture<Status> sendModifierMask() override;
    bool canDetectForegroundChange() const override { return true; }
    QString name() const override { return QStringLiteral("SendInput"); }

    // Окно запущено с более высоким уровнем целостности, чем программа (UIPI)
    static bool isElevatedAbove(HWND window);
    static QString processName(HWND window);   // для журнала и профилей задержек

private:
    HWND m_target = nullptr;
};

} // namespace mp::win
