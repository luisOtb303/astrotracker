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
        StartExportPhotos,
        TogglePreview,
        NextFrame,
        PrevFrame,
    };

    explicit ShortcutManager(QWidget* parent = nullptr);

    QKeySequence shortcut(Action action) const;
    void setShortcut(Action action, const QKeySequence& seq);

    // Register all shortcuts on a target widget.
    void install(QWidget* target);

    // ¿Esta acción se registra como atajo global (QShortcut) o se atiende por
    // el foco del widget? Solo las combinaciones con modificador son globales.
    //
    // Motivo: un QShortcut con WidgetWithChildrenShortcut consume la tecla antes
    // de que la reciba el widget con el foco, así que los sliders del dock de
    // ajustes perdían las flechas y el vídeo avanzaba de frame en su lugar.
    static bool isGlobalShortcut(Action action);

// Las teclas sin modificador (flechas, espacio, S, J, L, P, Return) no se
    // emiten desde aquí: se gestionan por foco en el visor y en la ventana, para
    // no robarle las flechas a los sliders del dock de ajustes.
    signals:
    void triggered(Action action);

private:
    std::map<Action, QKeySequence> defaults_;
    std::map<Action, QShortcut*> shortcuts_;
};
