#pragma once

#include "paste/PasteOrchestrator.h"

#include <QDialog>

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace mp {

// Preview of what goes to the chat (PST-6): exactly the images and text, in the same order,
// that the paste produces, via the same PasteOrchestrator::bakeSequence (JPEG instead of PNG for
// large shots, stitching in mode 4, text before or after the images).
// The "files + text" mode is shown as a sequence: the images are the same.
class PreviewDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PreviewDialog(QWidget *parent = nullptr);

    // Baking runs in the thread pool; until it is ready, "Preparing…"
    void showSet(const CaptureSet &set, const PasteOptions &options);
    // For tests: wait for baking
    bool isReady() const { return m_ready; }
    QString messageText() const { return m_text; }
    int imageCount() const { return m_imageCount; }

signals:
    void saveToFolderRequested();

private:
    void fill(const QList<PasteOrchestrator::Item> &items);

    QVBoxLayout *m_items;
    QLabel *m_summary;
    QPushButton *m_copyText;
    QString m_text;
    int m_imageCount = 0;
    bool m_ready = false;
    quint64 m_generation = 0;
};

} // namespace mp
