#include "ui/shortcuts/ShortcutManager.h"

#include <QWidget>
#include <QShortcut>

ShortcutManager::ShortcutManager(QWidget* parent)
    : QObject(parent)
{
    defaults_[Action::OpenFile] = QKeySequence::Open;
    defaults_[Action::PlayPause] = QKeySequence(Qt::Key_Space);
    defaults_[Action::Stop] = QKeySequence(Qt::Key_S);
    defaults_[Action::StepForward] = QKeySequence(Qt::Key_Right);
    defaults_[Action::StepBack] = QKeySequence(Qt::Key_Left);
    defaults_[Action::NextFrame] = QKeySequence(Qt::Key_L);
    defaults_[Action::PrevFrame] = QKeySequence(Qt::Key_J);
    defaults_[Action::ZoomIn] = QKeySequence::ZoomIn;
    defaults_[Action::ZoomOut] = QKeySequence::ZoomOut;
    defaults_[Action::ZoomFit] = QKeySequence(Qt::CTRL | Qt::Key_0);
    defaults_[Action::Zoom100] = QKeySequence(Qt::CTRL | Qt::Key_1);
    defaults_[Action::ToggleInfo] = QKeySequence(Qt::CTRL | Qt::Key_I);
    defaults_[Action::ToggleFilmstrip] = QKeySequence(Qt::CTRL | Qt::Key_F);
    defaults_[Action::StartAnalyze] = QKeySequence(Qt::Key_Return);
    defaults_[Action::StartExport] = QKeySequence(Qt::CTRL | Qt::Key_E);
    defaults_[Action::StartExportPhotos] = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E);
    defaults_[Action::TogglePreview] = QKeySequence(Qt::Key_P);

    // Las de arriba sin modificador (PlayPause, Stop, StepForward/Back,
    // NextFrame/PrevFrame, StartAnalyze, TogglePreview) NO se instalan como
    // QShortcut global: se gestionan con el foco del widget. ZoomIn/ZoomOut
    // usan Ctrl y siguen siendo globales.
}

QKeySequence ShortcutManager::shortcut(Action action) const
{
    const auto it = defaults_.find(action);
    return it != defaults_.end() ? it->second : QKeySequence();
}

void ShortcutManager::setShortcut(Action action, const QKeySequence& seq)
{
    defaults_[action] = seq;
}

bool ShortcutManager::isGlobalShortcut(Action action)
{
    switch (action) {
    // Sin modificador: se atienden por el foco del widget (visor o ventana),
    // nunca como QShortcut global.
    case Action::PlayPause:
    case Action::Stop:
    case Action::StepForward:
    case Action::StepBack:
    case Action::NextFrame:
    case Action::PrevFrame:
    case Action::StartAnalyze:
    case Action::TogglePreview:
        return false;
    // Con Ctrl: no colisionan con la escritura ni con las flechas de un slider.
    case Action::OpenFile:
    case Action::ZoomIn:
    case Action::ZoomOut:
    case Action::ZoomFit:
    case Action::Zoom100:
    case Action::ToggleInfo:
    case Action::ToggleFilmstrip:
    case Action::StartExport:
    case Action::StartExportPhotos:
        return true;
    }
    return false;
}

void ShortcutManager::install(QWidget* target)
{
    for (auto& [action, keySeq] : defaults_) {
        // Solo las combinaciones con modificador se registran como QShortcut
        // global. Las teclas peladas (flechas, espacio, S/J/L/P, Return) se
        // ignoran aquí a propósito: un QShortcut con WidgetWithChildrenShortcut
        // sobre la ventana consume la tecla ANTES de que la reciba el widget con
        // el foco, así que los sliders del dock "Imagen" nunca veían las flechas
        // y en su lugar el vídeo avanzaba de frame. Ahora esas teclas se atienden
        // en VideoView::keyPressEvent(), es decir, solo con el visor enfocado.
        if (keySeq.isEmpty() || !isGlobalShortcut(action))
            continue;
        auto* sc = new QShortcut(keySeq, target);
        sc->setContext(Qt::WidgetWithChildrenShortcut);
        connect(sc, &QShortcut::activated, this, [this, action]() {
            emit triggered(action);
        });
        shortcuts_[action] = sc;
    }
}
