#include "SetStorage.h"

#include "render/ShotRenderer.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace mp {

namespace {

constexpr int kFormatVersion = 1;

const QFileDevice::Permissions kPrivateDir = QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner;
const QFileDevice::Permissions kPrivateFile = QFileDevice::ReadOwner | QFileDevice::WriteOwner;

void setError(QString *error, const QString &text)
{
    if (error)
        *error = text;
}

bool writeFile(const QString &path, const QByteArray &data, bool privateFile, QString *error)
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit()) {
        setError(error, QStringLiteral("%1: %2").arg(path, f.errorString()));
        return false;
    }
    if (privateFile)
        QFile::setPermissions(path, kPrivateFile);
    return true;
}

QJsonArray rectToJson(const QRect &r)
{
    return {r.x(), r.y(), r.width(), r.height()};
}

QRect rectFromJson(const QJsonValue &v)
{
    const QJsonArray a = v.toArray();
    if (a.size() != 4)
        return {};
    return QRect(a[0].toInt(), a[1].toInt(), a[2].toInt(), a[3].toInt());
}

QString shapeName(MarkShape s)
{
    switch (s) {
    case MarkShape::Arrow: return QStringLiteral("arrow");
    case MarkShape::Freehand: return QStringLiteral("freehand");
    case MarkShape::Point: return QStringLiteral("point");
    case MarkShape::Rect: break;
    }
    return QStringLiteral("rect");
}

QString lineName(LineStyle l)
{
    switch (l) {
    case LineStyle::Dash: return QStringLiteral("dash");
    case LineStyle::Dot: return QStringLiteral("dot");
    case LineStyle::Solid: break;
    }
    return QStringLiteral("solid");
}

} // namespace

QByteArray SetStorage::toJson(const CaptureSet &set)
{
    QJsonArray shots;
    for (const Shot &s : set.shots) {
        QJsonArray marks;
        for (const Mark &m : s.marks) {
            QJsonObject o{
                {QStringLiteral("id"), m.id.toString(QUuid::WithoutBraces)},
                {QStringLiteral("rect"), rectToJson(m.rect)},
                {QStringLiteral("comment"), m.comment},
                {QStringLiteral("shape"), shapeName(m.shape)},
            };
            if (!m.points.isEmpty()) {
                QJsonArray pts;
                for (const QPoint &p : m.points)
                    pts << p.x() << p.y();
                o.insert(QStringLiteral("points"), pts);
            }
            if (m.pen.color.isValid())
                o.insert(QStringLiteral("color"), m.pen.color.name(QColor::HexArgb));
            if (m.pen.width > 0)
                o.insert(QStringLiteral("width"), m.pen.width);
            if (m.pen.line != LineStyle::Solid)
                o.insert(QStringLiteral("line"), lineName(m.pen.line));
            // Frame shape only for frames and only when not rectangular: old sets are read as they are
            if (m.shape == MarkShape::Rect && m.pen.frame != FrameShape::Rect)
                o.insert(QStringLiteral("frame"), m.pen.frame == FrameShape::Rounded ? QStringLiteral("rounded")
                                                                                     : QStringLiteral("ellipse"));
            marks << o;
        }
        QJsonArray redactions;
        for (const Redaction &r : s.redactions)
            redactions << QJsonObject{{QStringLiteral("id"), r.id.toString(QUuid::WithoutBraces)},
                                      {QStringLiteral("rect"), rectToJson(r.rect)}};
        QJsonObject o{
            {QStringLiteral("id"), s.id.toString(QUuid::WithoutBraces)},
            {QStringLiteral("width"), s.sourceSize.width()},
            {QStringLiteral("height"), s.sourceSize.height()},
            {QStringLiteral("dpr"), s.devicePixelRatio},
            {QStringLiteral("marks"), marks},
            {QStringLiteral("redactions"), redactions},
        };
        if (!s.crop.isNull())
            o.insert(QStringLiteral("crop"), rectToJson(s.crop));
        shots << o;
    }
    const QJsonObject root{{QStringLiteral("version"), kFormatVersion}, {QStringLiteral("shots"), shots}};
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

std::optional<CaptureSet> SetStorage::fromJson(const QByteArray &json, QString *error)
{
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (doc.isNull() || !doc.isObject()) {
        setError(error, QStringLiteral("set.json: ") + pe.errorString());
        return std::nullopt;
    }
    const QJsonObject root = doc.object();
    // Newer than we understand: do not read it, otherwise the next save would lose what we do not know
    if (root.value(QStringLiteral("version")).toInt() != kFormatVersion) {
        setError(error, QStringLiteral("set.json: unsupported version %1").arg(root.value(QStringLiteral("version")).toInt()));
        return std::nullopt;
    }
    CaptureSet set;
    for (const QJsonValue &sv : root.value(QStringLiteral("shots")).toArray()) {
        const QJsonObject so = sv.toObject();
        Shot s;
        s.id = QUuid::fromString(so.value(QStringLiteral("id")).toString());
        s.sourceSize = QSize(so.value(QStringLiteral("width")).toInt(), so.value(QStringLiteral("height")).toInt());
        s.devicePixelRatio = so.value(QStringLiteral("dpr")).toDouble(1.0);
        if (s.devicePixelRatio <= 0)
            s.devicePixelRatio = 1.0;
        if (so.contains(QStringLiteral("crop")))
            s.crop = rectFromJson(so.value(QStringLiteral("crop")));
        if (s.id.isNull() || s.sourceSize.isEmpty()) {
            setError(error, QStringLiteral("set.json: bad shot"));
            return std::nullopt;
        }
        for (const QJsonValue &mv : so.value(QStringLiteral("marks")).toArray()) {
            const QJsonObject mo = mv.toObject();
            Mark m;
            m.id = QUuid::fromString(mo.value(QStringLiteral("id")).toString());
            if (m.id.isNull())
                m.id = QUuid::createUuid();
            m.rect = rectFromJson(mo.value(QStringLiteral("rect")));
            m.comment = mo.value(QStringLiteral("comment")).toString().left(kMaxCommentLength);
            const QString shape = mo.value(QStringLiteral("shape")).toString();
            m.shape = shape == u"arrow"      ? MarkShape::Arrow
                    : shape == u"freehand" ? MarkShape::Freehand
                    : shape == u"point"    ? MarkShape::Point
                                           : MarkShape::Rect;
            const QJsonArray pts = mo.value(QStringLiteral("points")).toArray();
            for (int i = 0; i + 1 < pts.size(); i += 2)
                m.points << QPoint(pts[i].toInt(), pts[i + 1].toInt());
            if (m.isLinear() && m.points.size() < (m.shape == MarkShape::Point ? 1 : 2))
                continue;   // a line without points cannot be drawn: skip it rather than crash
            m.pen.color = QColor(mo.value(QStringLiteral("color")).toString());
            m.pen.width = mo.value(QStringLiteral("width")).toDouble(0);
            const QString line = mo.value(QStringLiteral("line")).toString();
            m.pen.line = line == u"dash" ? LineStyle::Dash : line == u"dot" ? LineStyle::Dot : LineStyle::Solid;
            const QString frame = mo.value(QStringLiteral("frame")).toString();
            m.pen.frame = frame == u"rounded" ? FrameShape::Rounded : frame == u"ellipse" ? FrameShape::Ellipse : FrameShape::Rect;
            s.marks << m;
        }
        for (const QJsonValue &rv : so.value(QStringLiteral("redactions")).toArray()) {
            const QJsonObject ro = rv.toObject();
            Redaction r{QUuid::fromString(ro.value(QStringLiteral("id")).toString()),
                        rectFromJson(ro.value(QStringLiteral("rect")))};
            if (r.id.isNull())
                r.id = QUuid::createUuid();
            s.redactions << r;
        }
        set.shots << s;
        if (set.isFull())
            break;
    }
    return set;
}

bool SetStorage::saveSession(const CaptureSet &set, const QString &dir, QString *error)
{
    if (!QDir().mkpath(dir)) {
        setError(error, QStringLiteral("cannot create %1").arg(dir));
        return false;
    }
    QFile::setPermissions(dir, kPrivateDir);
    QDir d(dir);

    QStringList keep{QStringLiteral("set.json")};
    for (const Shot &s : set.shots) {
        const QString name = s.id.toString(QUuid::WithoutBraces) + QStringLiteral(".png");
        keep << name;
        if (!d.exists(name) && !writeFile(d.filePath(name), s.sourcePng, true, error))
            return false;
    }
    // set.json goes last: if something fails midway, the old set.json refers to files that still exist
    if (!writeFile(d.filePath(QStringLiteral("set.json")), toJson(set), true, error))
        return false;
    for (const QString &name : d.entryList({QStringLiteral("*.png")}, QDir::Files))
        if (!keep.contains(name))
            d.remove(name);
    return true;
}

std::optional<CaptureSet> SetStorage::loadSession(const QString &dir, QString *error)
{
    QDir d(dir);
    QFile f(d.filePath(QStringLiteral("set.json")));
    if (!f.exists())
        return CaptureSet{};
    if (!f.open(QIODevice::ReadOnly)) {
        setError(error, f.errorString());
        return std::nullopt;
    }
    std::optional<CaptureSet> set = fromJson(f.readAll(), error);
    if (!set)
        return std::nullopt;
    // A shot without a source or with a broken PNG is skipped: the rest of the set is more useful than nothing
    QList<Shot> shots;
    for (Shot &s : set->shots) {
        QFile png(d.filePath(s.id.toString(QUuid::WithoutBraces) + QStringLiteral(".png")));
        if (!png.open(QIODevice::ReadOnly))
            continue;
        s.sourcePng = png.readAll();
        const QImage probe = QImage::fromData(s.sourcePng, "PNG");
        if (probe.isNull())
            continue;
        s.sourceSize = probe.size();
        shots << s;
    }
    set->shots = shots;
    return set;
}

void SetStorage::removeSession(const QString &dir)
{
    QDir(dir).removeRecursively();
}

QString SetStorage::exportMarkdown(const CaptureSet &set, const MessageTemplate &tpl)
{
    QStringList images;
    for (int i = 0; i < set.shots.size(); ++i)
        images << QStringLiteral("![%1](%1.png)").arg(shotLetter(i));
    const QString text = buildMessage(set, tpl);
    QStringList blocks;
    if (tpl.textBeforeImages && !text.isEmpty())
        blocks << text;
    if (!images.isEmpty())
        blocks << images.join(u'\n');
    if (!tpl.textBeforeImages && !text.isEmpty())
        blocks << text;
    return blocks.join(QStringLiteral("\n\n")) + u'\n';
}

QStringList SetStorage::exportSet(const CaptureSet &set, const RenderStyle &style, const MessageTemplate &tpl,
                                  const QString &dir, QString *error)
{
    if (!QDir().mkpath(dir)) {
        setError(error, QStringLiteral("cannot create %1").arg(dir));
        return {};
    }
    QDir d(dir);
    QStringList written;
    for (int i = 0; i < set.shots.size(); ++i) {
        const QImage image = renderShot(set.shots[i], i, style);
        const QString path = d.filePath(QString(shotLetter(i)) + QStringLiteral(".png"));
        QSaveFile f(path);
        if (image.isNull() || !f.open(QIODevice::WriteOnly) || !image.save(&f, "PNG") || !f.commit()) {
            setError(error, QStringLiteral("%1: %2").arg(path, f.errorString()));
            return {};
        }
        written << path;
    }
    const QString md = d.filePath(QStringLiteral("message.md"));
    if (!writeFile(md, exportMarkdown(set, tpl).toUtf8(), false, error))
        return {};
    written << md;
    return written;
}

} // namespace mp
