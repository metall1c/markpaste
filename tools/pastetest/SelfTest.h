#pragma once

#include <QList>
#include <QString>

// Mode 1 self-test: a receiver window, a sequential paste into it and a report
// of what actually arrived. Checks data-control + RemoteDesktop together without third-party programs.
//
// By default the receiver is a separate process (like a real chat). With inProcess = true
// the receiver lives in our process: this checks that reading our own clipboard
// does not block (docs/decisions/0002-*.md).
int runSelfTest(const QList<QByteArray> &images, const QString &text, int settleMs, int delayMs, bool inProcess);

// Child process mode: a receiver window, every paste event is a line on stdout.
int runReceiver();
