#include "TestUtil.h"
#include "core/MessageBuilder.h"

#include <gtest/gtest.h>

using namespace mp;
using namespace mp::test;

static Shot shotWith(std::initializer_list<const char *> comments)
{
    Shot s;
    s.sourceSize = QSize(10, 10);
    for (const char *c : comments)
        s.marks.append(Mark{QUuid::createUuid(), QRect(0, 0, 1, 1), QString::fromUtf8(c)});
    return s;
}

TEST(MessageBuilder, DefaultRussianExampleFromSpec)
{
    CaptureSet set;
    set.shots << shotWith({"Кнопку «Купить» ярче", "Цену на карточках крупнее"})
              << shotWith({"Убрать этот блок"});
    const auto tpl = MessageTemplate::defaultFor(QLocale(QLocale::Russian));
    EXPECT_EQ(buildMessage(set, tpl),
              QStringLiteral("Снимок A\nA1 Кнопку «Купить» ярче\nA2 Цену на карточках крупнее\n\n"
                             "Снимок B\nB1 Убрать этот блок"));
}

TEST(MessageBuilder, EnglishDefault)
{
    CaptureSet set;
    set.shots << shotWith({"x"});
    EXPECT_EQ(buildMessage(set, MessageTemplate::defaultFor(QLocale(QLocale::English))),
              QStringLiteral("Shot A\nA1 x"));
}

TEST(MessageBuilder, EmptyCommentGivesOnlyId)
{
    CaptureSet set;
    set.shots << shotWith({"", "  "});
    MessageTemplate tpl = MessageTemplate::defaultFor(QLocale(QLocale::English));
    EXPECT_EQ(buildMessage(set, tpl), QStringLiteral("Shot A\nA1\nA2"));

    tpl.markLine = QStringLiteral("{id}: {text}");
    EXPECT_EQ(buildMessage(set, tpl), QStringLiteral("Shot A\nA1\nA2"));

    tpl.markLine = QStringLiteral("**{id}** {text}");
    EXPECT_EQ(buildMessage(set, tpl), QStringLiteral("Shot A\n**A1**\n**A2**"));
}

TEST(MessageBuilder, ShotWithoutMarksGivesHeaderOnly)
{
    CaptureSet set;
    set.shots << shotWith({}) << shotWith({"b"});
    EXPECT_EQ(buildMessage(set, MessageTemplate::defaultFor(QLocale(QLocale::English))),
              QStringLiteral("Shot A\n\nShot B\nB1 b"));
}

TEST(MessageBuilder, MultilineComment)
{
    CaptureSet set;
    set.shots << shotWith({"line one\nline two"});
    EXPECT_EQ(buildMessage(set, MessageTemplate::defaultFor(QLocale(QLocale::English))),
              QStringLiteral("Shot A\nA1 line one\nline two"));
}

TEST(MessageBuilder, IntroAndOutro)
{
    CaptureSet set;
    set.shots << shotWith({"x"});
    MessageTemplate tpl = MessageTemplate::defaultFor(QLocale(QLocale::English));
    tpl.intro = QStringLiteral("Fix these:");
    tpl.outro = QStringLiteral("Thanks");
    EXPECT_EQ(buildMessage(set, tpl), QStringLiteral("Fix these:\n\nShot A\nA1 x\n\nThanks"));
}

TEST(MessageBuilder, TwentySixShots)
{
    CaptureSet set;
    for (int i = 0; i < kMaxShots; ++i)
        set.shots << shotWith({"c"});
    const QString msg = buildMessage(set, MessageTemplate::defaultFor(QLocale(QLocale::English)));
    EXPECT_TRUE(msg.endsWith(QStringLiteral("Shot Z\nZ1 c")));
}
