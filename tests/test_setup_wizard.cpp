#include "FakePlatform.h"
#include "ui/SetupWizard.h"

#include <QApplication>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>

using namespace mp;
using namespace mp::test;

namespace {

// Ctrl+V «доходит» до поля в фокусе: вставляет текст буфера, как сделал бы чат
class TypingInjector : public FakeInjector
{
public:
    using FakeInjector::FakeInjector;
    bool deliver = true;
    QFuture<Status> sendPaste() override
    {
        const QFuture<Status> r = FakeInjector::sendPaste();
        if (deliver)
            if (auto *edit = qobject_cast<QLineEdit *>(QApplication::focusWidget()))
                if (const ClipboardEntry *e = clipboard->current.find(QStringLiteral("text/plain;charset=utf-8")))
                    edit->insert(QString::fromUtf8(e->data));
        return r;
    }
};

struct WizardFixture {
    Platform platform;
    FakeCapture *capture;
    FakeClipboard *clipboard;
    TypingInjector *injector;
    std::unique_ptr<SetupWizard> wizard;

    WizardFixture()
    {
        auto cap = std::make_unique<FakeCapture>();
        auto cb = std::make_unique<FakeClipboard>();
        auto inj = std::make_unique<TypingInjector>(cb.get());
        capture = cap.get();
        clipboard = cb.get();
        injector = inj.get();
        platform.capture = std::move(cap);
        platform.clipboard = std::move(cb);
        platform.injector = std::move(inj);
        platform.environment = QStringLiteral("Test");
        wizard = std::make_unique<SetupWizard>(&platform, QStringLiteral("su._525.MarkpasteTest"));
        wizard->setPasteWaitMs(0);
        wizard->show();
        wizard->activateWindow();
        QTest::qWait(30);
    }
};

} // namespace

TEST(SetupWizard, CaptureReportsScreens)
{
    WizardFixture f;
    QImage img(64, 32, QImage::Format_ARGB32);
    f.capture->result.frames = {ScreenFrame{QStringLiteral("A"), QRect(0, 0, 64, 32), 1.0, img}};
    f.wizard->checkCapture();
    QTest::qWait(20);
    EXPECT_EQ(f.capture->calls, 1);
    EXPECT_EQ(f.wizard->captureResult(), SetupWizard::Result::Ok);

    f.capture->result = {Status::failure(QStringLiteral("denied")), {}};
    f.wizard->checkCapture();
    QTest::qWait(20);
    EXPECT_EQ(f.wizard->captureResult(), SetupWizard::Result::Failed);
}

TEST(SetupWizard, PasteRoundTripRestoresClipboard)
{
    WizardFixture f;
    f.clipboard->current = ClipboardPayload::text(QStringLiteral("user data"));
    f.wizard->checkPaste();
    QTest::qWait(300);   // 80 мс до Ctrl+V и цепочка future
    EXPECT_EQ(f.wizard->pasteResult(), SetupWizard::Result::Ok);
    EXPECT_EQ(f.injector->begins, 1);
    EXPECT_EQ(f.injector->ends, 1);
    // Буфер пользователя возвращён
    EXPECT_EQ(f.clipboard->current.find(QStringLiteral("text/plain;charset=utf-8"))->data, QByteArray("user data"));
}

TEST(SetupWizard, PasteThatDoesNotArriveFails)
{
    WizardFixture f;
    f.injector->deliver = false;
    f.wizard->checkPaste();
    QTest::qWait(300);
    EXPECT_EQ(f.wizard->pasteResult(), SetupWizard::Result::Failed);
    EXPECT_EQ(f.injector->ends, 1);   // сессия закрыта и при неудаче
}

TEST(SetupWizard, DeniedSessionFails)
{
    WizardFixture f;
    f.injector->beginStatus = Status::failure(QStringLiteral("denied"));
    f.wizard->checkPaste();
    QTest::qWait(100);
    EXPECT_EQ(f.wizard->pasteResult(), SetupWizard::Result::Failed);
}

TEST(SetupWizard, NoBackgroundClipboardWarnsWithoutTest)
{
    WizardFixture f;
    f.clipboard->background = false;
    SetupWizard w(&f.platform, QStringLiteral("su._525.MarkpasteTest"));
    EXPECT_EQ(w.pasteResult(), SetupWizard::Result::Warning);
}

TEST(SetupWizard, NoHotkeysBackendFails)
{
    WizardFixture f;
    // Без IHotkeys — сразу ошибка с подсказкой про CLI
    EXPECT_EQ(f.wizard->hotkeyResult(), SetupWizard::Result::Failed);
}

TEST(SetupWizard, HotkeysTurnOkWhenPressed)
{
    WizardFixture f;
    f.platform.hotkeys = std::make_unique<FakeHotkeys>();
    SetupWizard w(&f.platform, QStringLiteral("su._525.MarkpasteTest"));
    w.setTriggers({{Action::Capture, QStringLiteral("Print")}, {Action::PasteSet, QString()}});
    EXPECT_EQ(w.hotkeyResult(), SetupWizard::Result::Warning);   // вставке клавиша не назначена
    w.setTriggers({{Action::Capture, QStringLiteral("Print")}, {Action::PasteSet, QStringLiteral("Ctrl+Alt+V")}});
    EXPECT_EQ(w.hotkeyResult(), SetupWizard::Result::Pending);
    w.hotkeyActivated(Action::Capture);
    EXPECT_EQ(w.hotkeyResult(), SetupWizard::Result::Pending);
    w.hotkeyActivated(Action::PasteSet);
    EXPECT_EQ(w.hotkeyResult(), SetupWizard::Result::Ok);
}
