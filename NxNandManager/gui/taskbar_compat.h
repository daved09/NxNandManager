#ifndef TASKBAR_COMPAT_H
#define TASKBAR_COMPAT_H

// Windows taskbar progress (QtWinExtras) is only available on Windows.
// Provide no-op replacements on other platforms.
#if defined(_WIN32)
#include <QtWinExtras>
#include <QWinTaskbarButton>
#include <QWinTaskbarProgress>
#else
#include <QObject>
#include <QWindow>

class QWinTaskbarProgress : public QObject
{
public:
    explicit QWinTaskbarProgress(QObject *parent = nullptr) : QObject(parent) {}
    void setValue(int) {}
    void setVisible(bool) {}
};

class QWinTaskbarButton : public QObject
{
public:
    explicit QWinTaskbarButton(QObject *parent = nullptr) : QObject(parent), m_progress(this) {}
    void setWindow(QWindow *) {}
    QWinTaskbarProgress *progress() { return &m_progress; }
private:
    QWinTaskbarProgress m_progress;
};
#endif

#endif // TASKBAR_COMPAT_H
