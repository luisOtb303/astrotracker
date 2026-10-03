#include "ui/shortcuts/ShortcutManager.h"

#include <QGuiApplication>
#include <QKeySequence>
#include <cstdio>

// Regresión del reparto de atajos entre atajo global y atención por foco.
//
// Un QShortcut con WidgetWithChildrenShortcut consume la tecla ANTES de que la
// reciba el widget con el foco. Con las flechas registradas como atajo global,
// los QSlider del dock de ajustes nunca las veían: en su lugar stepForward()/
// stepBackward() movían el vídeo. Además las letras S/J/L/P se colaban en
// cualquier campo de texto y Espacio/Return pisaban los botones con el foco.
//
// Aquí se fija la clasificación: con modificador = global; tecla pelada = foco.
//
// Nota: se usa QGuiApplication (no QCoreApplication) porque QKeySequence usa
// QtGui. El test corre con QT_QPA_PLATFORM=offscreen, sin ventana visible.

namespace {

int failures = 0;

void expect(bool cond, const char* msg)
{
    if (!cond) {
        std::printf("FAIL: %s\n", msg);
        std::fflush(stdout);
        ++failures;
    }
}

using A = ShortcutManager::Action;

// Teclas peladas: NO deben ser atajos globales, se atienden por el foco.
const A kFocusActions[] = {
    A::PlayPause,  A::Stop,       A::StepForward, A::StepBack,
    A::NextFrame,  A::PrevFrame,  A::StartAnalyze, A::TogglePreview,
};

// Combinaciones con Ctrl: sí son atajos globales.
const A kGlobalActions[] = {
    A::OpenFile,    A::ZoomIn,      A::ZoomOut,        A::ZoomFit,
    A::Zoom100,     A::ToggleInfo,  A::ToggleFilmstrip, A::StartExport,
    A::StartExportPhotos,
};

int modifiersOf(const QKeySequence& seq)
{
    // QKeySequence guarda los modificadores en los bits altos del código.
    return seq[0] & static_cast<int>(Qt::KeyboardModifierMask);
}

} // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);

    // 1. Ninguna tecla pelada se registra como atajo global.
    for (A a : kFocusActions)
        expect(!ShortcutManager::isGlobalShortcut(a),
               "una tecla sin modificador no debe ser atajo global");

    // 2. Las combinaciones con Ctrl sí son globales.
    for (A a : kGlobalActions)
        expect(ShortcutManager::isGlobalShortcut(a),
               "una combinación con Ctrl debe ser atajo global");

    // 3. Contraste explícito con las teclas del informe.
    expect(!ShortcutManager::isGlobalShortcut(A::StepForward),
           "flecha derecha no debe ser atajo global (pisaba los sliders)");
    expect(!ShortcutManager::isGlobalShortcut(A::StepBack),
           "flecha izquierda no debe ser atajo global (pisaba los sliders)");
    expect(!ShortcutManager::isGlobalShortcut(A::PlayPause),
           "espacio no debe ser atajo global (pisaba los botones)");
    expect(!ShortcutManager::isGlobalShortcut(A::StartAnalyze),
           "return no debe ser atajo global (pisaba los botones)");

    // 4. Coherencia con la tabla por defecto: lo marcado como global debe
    //    llevar modificador de verdad, y lo atendido por foco, no.
    ShortcutManager mgr;

    for (A a : kFocusActions) {
        const QKeySequence seq = mgr.shortcut(a);
        expect(!seq.isEmpty(), "la acción por foco debe tener tecla asignada");
        if (!seq.isEmpty())
            expect(modifiersOf(seq) == 0,
                   "una acción por foco no debe llevar modificador");
    }

    for (A a : kGlobalActions) {
        const QKeySequence seq = mgr.shortcut(a);
        expect(!seq.isEmpty(), "la acción global debe tener tecla asignada");
        if (!seq.isEmpty())
            expect(modifiersOf(seq) != 0,
                   "una acción global debe llevar modificador");
    }

    if (failures == 0)
        std::printf("test_shortcuts: OK\n");
    else
        std::printf("test_shortcuts: %d fallos\n", failures);
    std::fflush(stdout);
    return failures == 0 ? 0 : 1;
}