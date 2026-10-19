#pragma once

#include <QList>
#include <QString>

// Самопроверка режима 1: окно-приёмник, последовательная вставка в него и отчёт
// о том, что реально дошло. Проверяет связку data-control + RemoteDesktop без чужих программ.
//
// По умолчанию приёмник — отдельный процесс (как настоящий чат). С inProcess = true
// приёмник живёт в нашем процессе: так проверяется, что чтение собственного буфера
// не блокируется (docs/decisions/0002-*.md).
int runSelfTest(const QList<QByteArray> &images, const QString &text, int settleMs, int delayMs, bool inProcess);

// Режим дочернего процесса: окно-приёмник, каждое событие вставки — строка в stdout.
int runReceiver();
