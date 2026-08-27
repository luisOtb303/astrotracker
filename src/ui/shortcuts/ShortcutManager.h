#pragma once

#include <QObject>
#include <QKeySequence>
#include <map>
#include <string>

class QShortcut;

class ShortcutManager : public QObject
{
    Q_OBJECT

public:
    enum class Action {
        OpenFile,
        PlayPause,
        Stop,
        StepForward,
        StepBack,
        ZoomIn,
        ZoomOut,
        ZoomFit,
        Zoom100,
        ToggleInfo,
        ToggleFilmstrip,
        StartAnalyze,
        StartExport,
        TogglePreview,
        NextFrame,
        PrevFrame,
    };

    explicit ShortcutManager(QWidget* parent = nullptr);

    QKeySequence shortcut(Action action) const;
    void setShortcut(Action action, const QKeySequence& seq);

    // Register all shortcuts on a target widget.
    void install(QWidget* target);

signals:
    void triggered(Action action);

private:
    std::map<Action, QKeySequence> defaults_;
    std::map<Action, QShortcut*> shortcuts_;
};
