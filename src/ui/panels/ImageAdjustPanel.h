#pragma once

#include "common/ImageAdjust.h"

#include <QWidget>
#include <QCheckBox>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QPushButton>
#include <deque>

// Dock de gestión de imagen: balance de blancos relativo (calidez), contaminación
// lumínica (sodio/mercurio), reducción de ruido, exposición, brillo y contraste.
// Funciona tanto para la pestaña Fotos (global / solo esta foto) como para
// Vídeo (global).
class ImageAdjustPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ImageAdjustPanel(QWidget* parent = nullptr);

    // Lee/escrita el ajuste actual.
    ImageAdjust currentAdjust() const;
    void setAdjust(const ImageAdjust& adj);

    // Modo de uso: Fotos (muestra selector alcance) o Vídeo (solo global).
    enum class Mode { Photos, Video };
    void setMode(Mode mode);

    // Estado del análisis temporal: refleja el progreso y el resultado en el
    // panel. `ok` con hotCount = 0 significa "no hay defects en este vídeo".
    void setHotScanRunning(bool running, int sampled = 0, int planned = 0);
    void setHotScanResult(bool ok, int hotCount, int sampled, const QString& error = {},
                          bool cancelled = false);
    void setHotScanError(const QString& error);
    // Limpia el mensaje de estado (cambio de vídeo, cierre de la pestaña).
    void clearHotScanStatus();

    // Borra el historial de deshacer (cambio de contexto).
    void clearHistory();

signals:
    // Emitido cuando el usuario mueve cualquier slider/botón.
    void adjustEdited(const ImageAdjust& adj);
    // "Restablecer todo" pulsado.
    void resetRequested();
    // El usuario cambió el alcance (Todas/Solo esta foto).
    void scopeChanged(bool perPhoto);
    // El usuario pidió analizar el vídeo en busca de píxeles calientes. Solo en
    // modo Vídeo; con fotos no hay con qué comparar y el botón no se muestra.
    void analyzeHotPixelsRequested();

private slots:
    void onWbSliderChanged(int value);
    void onSodiumChanged(int value);
    void onMercuryChanged(int value);
    void onExposureChanged(int value);
    void onBrightnessChanged(int value);
    void onContrastChanged(int value);
    void onDenoiseChanged(int value);
    void onHotToggled(bool checked);
    void onHotSensitivityChanged(int value);
    void onAnalyzeHotClicked();
    void onResetClicked();
    void onCenterWarmthClicked();
    void onUndoClicked();
    void onScopeChanged(int index);

private:
    void updateWarmthLabel();
    // Empuja el estado previo como punto de restauración de "Deshacer".
    void pushUndo();
    // Confirma el cambio actual como punto de deshacer, salvo si viene de un
    // arrastre en curso (esos se confirman al soltar, uno por gesto).
    void noteEdit();
    // Conecta un slider al historial por arrastre.
    void installSliderUndo(QSlider* slider);

    Mode mode_ = Mode::Photos;

    QComboBox* scopeCombo_ = nullptr;
    QSlider* wbSlider_ = nullptr;
    QLabel* wbValueLabel_ = nullptr;
    QPushButton* centerWarmthBtn_ = nullptr;

    QSlider* sodiumSlider_ = nullptr;
    QLabel* sodiumValueLabel_ = nullptr;

    QSlider* mercurySlider_ = nullptr;
    QLabel* mercuryValueLabel_ = nullptr;

    QSlider* denoiseSlider_ = nullptr;
    QLabel* denoiseValueLabel_ = nullptr;

    QCheckBox* hotCheck_ = nullptr;
    QSlider* hotSensitivitySlider_ = nullptr;
    QLabel* hotSensitivityLabel_ = nullptr;
    QPushButton* hotAnalyzeBtn_ = nullptr;
    QLabel* hotStatusLabel_ = nullptr;

    QSlider* exposureSlider_ = nullptr;
    QLabel* exposureValueLabel_ = nullptr;

    QSlider* brightnessSlider_ = nullptr;
    QLabel* brightnessValueLabel_ = nullptr;

    QSlider* contrastSlider_ = nullptr;
    QLabel* contrastValueLabel_ = nullptr;

    QPushButton* resetBtn_ = nullptr;
    QPushButton* undoBtn_ = nullptr;

    // Undo (bounded deque, ~30 entries).
    static constexpr size_t kMaxUndo = 30;
    std::deque<ImageAdjust> undoDeque_;
    ImageAdjust lastState_;
    bool suppressUndo_ = false;
    bool dragging_ = false;
};