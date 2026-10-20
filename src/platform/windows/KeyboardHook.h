#pragma once

#include "WinKeys.h"

#include <QList>
#include <QObject>

#include <atomic>
#include <future>
#include <mutex>
#include <thread>

#include <windows.h>

namespace mp::win {

// Низкоуровневый хук клавиатуры WH_KEYBOARD_LL (ТЗ разд. 9) — для клавиш, которые
// RegisterHotKey не получает: Print Screen в Windows 11 перехватывают «Ножницы».
//
// Хук живёт в своём потоке со своим циклом сообщений. Windows ждёт ответа хука не дольше
// LowLevelHooksTimeout (около 1 с), а после нескольких опозданий молча снимает его. В главном
// потоке хук ждал бы запекания картинок и отрисовки — поэтому отдельный поток, который
// ничего не делает, кроме сравнения кода клавиши.
//
// Нажатия, которые эмулирует сама программа (LLKHF_INJECTED), пропускаются: иначе наш же
// Ctrl+V попал бы в перехват. Совпавшее сочетание проглатывается, в том числе автоповтор.
// Хук ничего не пишет в журнал.
class KeyboardHook : public QObject
{
    Q_OBJECT
public:
    struct Trigger {
        int id = 0;
        WinTrigger keys;
    };

    explicit KeyboardHook(QObject *parent = nullptr);
    ~KeyboardHook() override;

    // Пустой список снимает хук и останавливает поток
    void setTriggers(const QList<Trigger> &triggers);
    bool isRunning() const { return m_thread.joinable(); }

signals:
    // Из потока хука; подключение к объектам главного потока — через очередь
    void pressed(int id);
    void released(int id);

private:
    struct State {
        Trigger trigger;
        bool down = false;
    };
    void start();
    void stop();
    void run(std::promise<bool> *started);
    bool handle(unsigned vk, bool down);   // true — проглотить
    static LRESULT CALLBACK hookProc(int code, WPARAM wParam, LPARAM lParam);

    std::thread m_thread;
    std::atomic<DWORD> m_threadId{0};
    std::mutex m_mutex;
    QList<State> m_states;
};

} // namespace mp::win
