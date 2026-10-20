#pragma once

#include "paste/PasteOrchestrator.h"

#include <QDialog>

class QLabel;
class QPushButton;
class QVBoxLayout;

namespace mp {

// Предпросмотр того, что уйдёт в чат (PST-6): ровно те картинки и текст и в том порядке,
// что даст вставка, — через тот же PasteOrchestrator::bakeSequence (JPEG вместо PNG для
// больших снимков, склейка в режиме 4, текст до или после картинок).
// Режим «файлы + текст» показывается как последовательность: картинки те же.
class PreviewDialog : public QDialog
{
    Q_OBJECT
public:
    explicit PreviewDialog(QWidget *parent = nullptr);

    // Запекание идёт в пуле потоков; до готовности — «Готовлю…»
    void showSet(const CaptureSet &set, const PasteOptions &options);
    // Для тестов: дождаться запекания
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
