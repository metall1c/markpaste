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

TEST(MessageBuilder, MarksWithoutCommentAreSkipped)
{
    CaptureSet set;
    set.shots << shotWith({"", "кнопка", "  "});
    const MessageTemplate tpl = MessageTemplate::defaultFor(QLocale(QLocale::Russian));
    // Номер — как на картинке: A2, хотя A1 в тексте нет
    EXPECT_EQ(buildMessage(set, tpl), QStringLiteral("Снимок A\nA2 кнопка"));
}

TEST(MessageBuilder, SingleShotWithoutCommentsGivesNoText)
{
    CaptureSet set;
    set.shots << shotWith({"", " "});   // стрелка, рамка — без слов
    MessageTemplate tpl = MessageTemplate::defaultFor(QLocale(QLocale::Russian));
    EXPECT_EQ(buildMessage(set, tpl), QString());

    set.shots = {shotWith({})};
    EXPECT_EQ(buildMessage(set, tpl), QString());

    // Вводная и заключительная фразы — от пользователя: остаются
    tpl.intro = QStringLiteral("Посмотри:");
    EXPECT_EQ(buildMessage(set, tpl), QStringLiteral("Посмотри:"));
}

TEST(MessageBuilder, SeveralShotsWithoutCommentsGiveNoText)
{
    // Три снимка с метками, но без единого комментария — никакого текста: ни «Снимок A»,
    // ни номеров меток
    CaptureSet set;
    set.shots << shotWith({"", " "}) << shotWith({}) << shotWith({""});
    EXPECT_EQ(buildMessage(set, MessageTemplate::defaultFor(QLocale(QLocale::Russian))), QString());
}

TEST(MessageBuilder, ShotWithoutCommentsAmongSeveralGivesHeaderOnly)
{
    CaptureSet set;
    set.shots << shotWith({""}) << shotWith({"b"});
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
