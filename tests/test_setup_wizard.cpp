#include "FakePlatform.h"
#include "ui/SetupWizard.h"

#include <QApplication>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>

using namespace mp;
using namespace mp::test;

namespace {

// Ctrl+V "reaches" the focused field: it pastes the clipboard text, as a chat would
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
    QTest::qWait(300);   // 80 ms before Ctrl+V and the future chain
    EXPECT_EQ(f.wizard->pasteResult(), SetupWizard::Result::Ok);
    EXPECT_EQ(f.injector->begins, 1);
    EXPECT_EQ(f.injector->ends, 1);
    // The user's clipboard is restored
    EXPECT_EQ(f.clipboard->current.find(QStringLiteral("text/plain;charset=utf-8"))->data, QByteArray("user data"));
}

TEST(SetupWizard, PasteThatDoesNotArriveFails)
{
    WizardFixture f;
    f.injector->deliver = false;
    f.wizard->checkPaste();
    QTest::qWait(300);
    EXPECT_EQ(f.wizard->pasteResult(), SetupWizard::Result::Failed);
    EXPECT_EQ(f.injector->ends, 1);   // the session is closed on failure too
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
    // Without IHotkeys, an error right away with a hint about the CLI
    EXPECT_EQ(f.wizard->hotkeyResult(), SetupWizard::Result::Failed);
}

TEST(SetupWizard, HotkeysTurnOkWhenPressed)
{
    WizardFixture f;
    f.platform.hotkeys = std::make_unique<FakeHotkeys>();
    SetupWizard w(&f.platform, QStringLiteral("su._525.MarkpasteTest"));
    w.setTriggers({{Action::Capture, QStringLiteral("Print")}, {Action::PasteSet, QString()}});
    EXPECT_EQ(w.hotkeyResult(), SetupWizard::Result::Warning);   // no key is assigned to paste
    w.setTriggers({{Action::Capture, QStringLiteral("Print")}, {Action::PasteSet, QStringLiteral("Ctrl+Alt+V")}});
    EXPECT_EQ(w.hotkeyResult(), SetupWizard::Result::Pending);
    w.hotkeyActivated(Action::Capture);
    EXPECT_EQ(w.hotkeyResult(), SetupWizard::Result::Pending);
    w.hotkeyActivated(Action::PasteSet);
    EXPECT_EQ(w.hotkeyResult(), SetupWizard::Result::Ok);
}

TEST(SetupWizard, ShotToStripKeyIsOptional)
{
    WizardFixture f;
    f.platform.hotkeys = std::make_unique<FakeHotkeys>();
    SetupWizard w(&f.platform, QStringLiteral("su._525.MarkpasteTest"));
    // Capture without editor is assigned but not pressed: the check still passes on the two main keys
    w.setTriggers({{Action::Capture, QStringLiteral("Print")},
                   {Action::PasteSet, QStringLiteral("Ctrl+Alt+V")},
                   {Action::CaptureToStrip, QStringLiteral("Ctrl+Print")}});
    w.hotkeyActivated(Action::Capture);
    w.hotkeyActivated(Action::PasteSet);
    EXPECT_EQ(w.hotkeyResult(), SetupWizard::Result::Ok);
    // Not assigned is not a warning
    w.setTriggers({{Action::Capture, QStringLiteral("Print")}, {Action::PasteSet, QStringLiteral("Ctrl+Alt+V")}});
    EXPECT_EQ(w.hotkeyResult(), SetupWizard::Result::Ok);
}

TEST(SetupWizard, OffersToFreeKeyTakenByDesktop)
{
    Platform platform;
    auto hk = std::make_unique<FakeHotkeys>();
    FakeHotkeys *hotkeys = hk.get();
    hotkeys->desktopHolders.insert(QStringLiteral("Print"), QStringLiteral("the GNOME screenshot tool"));
    platform.hotkeys = std::move(hk);
    platform.environment = QStringLiteral("Test");
    SetupWizard wizard(&platform, QStringLiteral("su._525.MarkpasteTest"));
    wizard.setTriggers({{Action::Capture, QStringLiteral("Print")}, {Action::PasteSet, QStringLiteral("Ctrl+Alt+V")}});

    QPushButton *give = nullptr;
    for (QPushButton *b : wizard.findChildren<QPushButton *>())
        if (b->text() == u"Give Print to Markpaste")
            give = b;
    ASSERT_NE(give, nullptr);
    EXPECT_FALSE(give->isHidden());

    give->click();
    EXPECT_EQ(hotkeys->released, QStringList{QStringLiteral("Print")});
    EXPECT_TRUE(give->isHidden());   // the key is free now: no more offer

    // A key that has fired is ours: no offer
    hotkeys->desktopHolders.insert(QStringLiteral("Print"), QStringLiteral("the GNOME screenshot tool"));
    SetupWizard again(&platform, QStringLiteral("su._525.MarkpasteTest"));
    again.setTriggers({{Action::Capture, QStringLiteral("Print")}});
    again.hotkeyActivated(Action::Capture);
    for (QPushButton *b : again.findChildren<QPushButton *>())
        EXPECT_FALSE(b->text().startsWith(u"Give ") && !b->isHidden());
}
