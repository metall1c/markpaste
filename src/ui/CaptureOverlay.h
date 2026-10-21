#pragma once

#include "platform/PlatformTypes.h"

#include <QImage>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QWidget>

namespace mp {

class OverlayWidget;

// Selection result: the cut-out piece of the frame in physical pixels.
struct CaptureSelection {
    QImage image;
    qreal devicePixelRatio = 1.0;
    QString screenName;
};

// Area selection session: one full-screen window per monitor over the
// "frozen" frame (spec section 4, step 2). The selection stays within one monitor.
class CaptureSession : public QObject
{
    Q_OBJECT
public:
    CaptureSession(const QList<ScreenFrame> &frames, bool quickMode, QObject *parent = nullptr);
    ~CaptureSession() override;

    void start();

signals:
    void selected(const mp::CaptureSelection &selection);
    void cancelled();

private:
    friend class OverlayWidget;
    void onSelectionStarted(OverlayWidget *source);
    void confirm(OverlayWidget *source, const QRect &physicalRect);
    void cancel();
    void closeAll();

    QList<ScreenFrame> m_frames;
    QList<QPointer<OverlayWidget>> m_widgets;
    bool m_quick;
    bool m_done = false;
};

class OverlayWidget : public QWidget
{
    Q_OBJECT
public:
    OverlayWidget(CaptureSession *session, const ScreenFrame &frame, bool quickMode);

    void clearSelection();
    const ScreenFrame &frame() const { return m_frame; }

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void showEvent(QShowEvent *) override;
    void closeEvent(QCloseEvent *) override;

private:
    enum class Mode { Idle, Selecting, Adjusting, Moving, Resizing };
    enum Handle { None = 0, Left = 1, Right = 2, Top = 4, Bottom = 8 };

    QRect toPhysical(const QRectF &logical) const;
    QRectF selectionRect() const;
    int handleAt(const QPointF &pos) const;
    void paintMagnifier(QPainter &p, const QPointF &pos);
    void confirmSelection();

    CaptureSession *m_session;
    ScreenFrame m_frame;
    QImage m_display;            // frame with devicePixelRatio = scale: drawn 1:1 in physical pixels
    bool m_quick;
    Mode m_mode = Mode::Idle;
    QPointF m_anchor;            // start of the selection or drag
    QRectF m_sel;                // logical window coordinates
    QRectF m_selAtPress;
    int m_handle = None;
    QPointF m_cursor;
    bool m_hasCursor = false;
};

} // namespace mp

Q_DECLARE_METATYPE(mp::CaptureSelection)
