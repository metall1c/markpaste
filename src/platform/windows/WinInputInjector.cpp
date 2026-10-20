#include "WinInputInjector.h"

#include "WinKeys.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QTimer>

#include <functional>
#include <memory>

Q_LOGGING_CATEGORY(lcWinInput, "markpaste.win.input")

namespace mp::win {

namespace {

constexpr int kGapMs = 20;   // между событиями клавиш, как на X11 и Wayland
constexpr std::chrono::milliseconds kModifierWait{1500};

DWORD integrityLevel(HANDLE token)
{
    DWORD size = 0;
    GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &size);
    if (!size)
        return 0;
    auto buf = std::make_unique<char[]>(size);
    if (!GetTokenInformation(token, TokenIntegrityLevel, buf.get(), size, &size))
        return 0;
    const auto *label = reinterpret_cast<const TOKEN_MANDATORY_LABEL *>(buf.get());
    const DWORD count = *GetSidSubAuthorityCount(label->Label.Sid);
    return *GetSidSubAuthority(label->Label.Sid, count - 1);
}

DWORD processIntegrity(HANDLE process)
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(process, TOKEN_QUERY, &token))
        return 0;
    const DWORD level = integrityLevel(token);
    CloseHandle(token);
    return level;
}

INPUT keyInput(WORD vk, bool down)
{
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.wScan = WORD(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC));
    in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    return in;
}

} // namespace

WinInputInjector::WinInputInjector(QObject *parent)
    : IInputInjector(parent)
{
}

bool WinInputInjector::isElevatedAbove(HWND window)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    if (!pid)
        return false;
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return GetLastError() == ERROR_ACCESS_DENIED;   // не пускают даже спросить — защищённый процесс
    const DWORD theirs = processIntegrity(process);
    CloseHandle(process);
    const DWORD ours = processIntegrity(GetCurrentProcess());
    return theirs > ours;
}

QString WinInputInjector::processName(HWND window)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    HANDLE process = pid ? OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid) : nullptr;
    if (!process)
        return {};
    wchar_t path[MAX_PATH * 2];
    DWORD len = DWORD(std::size(path));
    QString name;
    if (QueryFullProcessImageNameW(process, 0, path, &len))
        name = QFileInfo(QString::fromWCharArray(path, int(len))).fileName();
    CloseHandle(process);
    return name;
}

QFuture<Status> WinInputInjector::begin()
{
    m_target = GetForegroundWindow();
    if (!m_target)
        return readyFuture(Status::failure(QStringLiteral("No active window to paste into")));
    qCInfo(lcWinInput) << "paste target:" << processName(m_target);
    if (isElevatedAbove(m_target))
        return readyFuture(Status::failure(
            QStringLiteral("The active window runs as administrator: Windows does not let Markpaste press keys in it. "
                           "Use \"Copy set\" and paste with Ctrl+V, or run Markpaste as administrator too.")));
    return readyFuture(Status::success());
}

void WinInputInjector::end()
{
    m_target = nullptr;
}

QFuture<Status> WinInputInjector::waitModifiersReleased(std::chrono::milliseconds timeout)
{
    auto promise = std::make_shared<QPromise<Status>>();
    promise->start();
    auto timer = std::make_shared<QElapsedTimer>();
    timer->start();
    auto poll = std::make_shared<std::function<void()>>();
    *poll = [this, promise, timer, poll, timeout] {
        if (!anyModifierDown() || timer->elapsed() >= timeout.count()) {
            promise->addResult(Status::success());
            promise->finish();
            *poll = nullptr;   // разрываем цикл ссылок
            return;
        }
        QTimer::singleShot(kGapMs, this, [poll] {
            if (*poll)
                (*poll)();
        });
    };
    (*poll)();
    return promise->future();
}

QFuture<Status> WinInputInjector::sendModifierMask()
{
    // Неназначенный код 0xE8: программы его не обрабатывают, а одиночным Alt нажатие уже не будет
    // (так же делает AutoHotkey). Ctrl здесь хуже: Ctrl+Alt — это AltGr в части раскладок
    INPUT in[2] = {};
    for (int i = 0; i < 2; ++i) {
        in[i].type = INPUT_KEYBOARD;
        in[i].ki.wVk = 0xE8;
        in[i].ki.dwFlags = i ? KEYEVENTF_KEYUP : 0;
    }
    if (SendInput(2, in, sizeof(INPUT)) != 2)
        return readyFuture(Status::failure(QStringLiteral("SendInput failed: error %1").arg(GetLastError())));
    return readyFuture(Status::success());
}

QFuture<Status> WinInputInjector::sendPaste()
{
    if (m_target && GetForegroundWindow() != m_target)
        return readyFuture(Status::failure(QStringLiteral("The active window changed: paste stopped")));

    auto promise = std::make_shared<QPromise<Status>>();
    promise->start();
    // Зажатые пользователем Shift/Alt/Win превратили бы Ctrl+V в другое сочетание
    waitModifiersReleased(kModifierWait).then(this, [this, promise](const Status &) {
        // Ожидание длится до 1,5 с — за это время пользователь мог перейти в другое окно
        if (m_target && GetForegroundWindow() != m_target) {
            promise->addResult(Status::failure(QStringLiteral("The active window changed: paste stopped")));
            promise->finish();
            return;
        }
        const INPUT steps[] = {
            keyInput(VK_CONTROL, true),
            keyInput('V', true),
            keyInput('V', false),
            keyInput(VK_CONTROL, false),
        };
        auto failed = std::make_shared<DWORD>(0);
        for (int i = 0; i < int(std::size(steps)); ++i)
            QTimer::singleShot(i * kGapMs, this, [in = steps[i], failed] {
                // После сбоя отпускания всё равно шлём (не оставить Ctrl зажатым), а нажатия — нет:
                // V без Ctrl напечатала бы в чат «v»
                if (*failed && !(in.ki.dwFlags & KEYEVENTF_KEYUP))
                    return;
                INPUT copy = in;
                if (SendInput(1, &copy, sizeof(INPUT)) != 1 && !*failed)
                    *failed = GetLastError() ? GetLastError() : 1;
            });
        QTimer::singleShot(int(std::size(steps)) * kGapMs, this, [promise, failed] {
            promise->addResult(*failed ? Status::failure(QStringLiteral("SendInput failed: error %1").arg(*failed))
                                       : Status::success());
            promise->finish();
        });
    });
    return promise->future();
}

} // namespace mp::win
