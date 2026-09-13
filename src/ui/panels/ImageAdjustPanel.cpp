#include "ui/panels/ImageAdjustPanel.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QShortcut>
#include <QKeySequence>
#include <algorithm>
#include <cmath>

namespace {

QSlider* makeSlider(int min, int max, int value, int tickInterval = 1,
                    QWidget* parent = nullptr)
{
    auto* s = new QSlider(Qt::Horizontal, parent);
    s->setRange(min, max);
    s->setValue(value);
    s->setSingleStep(1);
    s->setTickInterval(tickInterval);
    s->setTickPosition(QSlider::TicksBelow);
    return s;
}

QLabel* makeValueLabel(const QString& initial, QWidget* parent)
{
    auto* l = new QLabel(initial, parent);
    l->setMinimumWidth(40);
    l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return l;
}

} // namespace

ImageAdjustPanel::ImageAdjustPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    // --- Alcance (solo Fotos) ---
    scopeCombo_ = new QComboBox(this);
    scopeCombo_->addItem(tr("Todas las fotos"));
    scopeCombo_->addItem(tr("Solo esta foto"));
    mainLayout->addWidget(scopeCombo_);
    connect(scopeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ImageAdjustPanel::onScopeChanged);

    // --- Balance de blancos ---
    auto* wbGroup = new QGroupBox(tr("Balance de blancos"), this);
    auto* wbLay = new QVBoxLayout(wbGroup);
    wbLay->setContentsMargins(8, 16, 8, 8);
    wbLay->setSpacing(4);

    auto* wbDetRow = new QHBoxLayout();
    wbDetectedLabel_ = new QLabel(tr("EXIF: —"), wbGroup);
    wbDetRow->addWidget(wbDetectedLabel_);
    wbDetRow->addStretch();
    wbLay->addLayout(wbDetRow);

    auto* wbRow = new QHBoxLayout();
    wbSlider_ = makeSlider(ImageAdjustLimits::kMinKelvin,
                           ImageAdjustLimits::kMaxKelvin,
                           ImageAdjustLimits::kDefaultKelvin, 50, wbGroup);
    wbSlider_->setToolTip(tr("Temperatura de balance de blancos (K). "
                               "11250 = neutro; bajar = más frío, subir = más cálido"));
    wbRow->addWidget(wbSlider_);
    wbSpin_ = new QSpinBox(wbGroup);
    wbSpin_->setRange(ImageAdjustLimits::kMinKelvin, ImageAdjustLimits::kMaxKelvin);
    wbSpin_->setValue(ImageAdjustLimits::kDefaultKelvin);
    wbSpin_->setSuffix(tr(" K"));
    wbSpin_->setSingleStep(100);
    wbSpin_->setFixedWidth(80);
    wbRow->addWidget(wbSpin_);
    wbValueLabel_ = makeValueLabel(tr("%1 K").arg(ImageAdjustLimits::kDefaultKelvin),
                                   wbGroup);
    wbValueLabel_->setVisible(false);  // hidden; spinbox shows the value
    wbLay->addLayout(wbRow);

    auto* wbBtnRow = new QHBoxLayout();
    detectWbBtn_ = new QPushButton(tr("Detectar"), wbGroup);
    detectWbBtn_->setToolTip(tr("Ancla el deslizador a la temperatura EXIF de la foto "
                                 "(si existe); si no, vuelve al neutro (11250 K)"));
    wbBtnRow->addStretch();
    wbBtnRow->addWidget(detectWbBtn_);
    wbLay->addLayout(wbBtnRow);

    mainLayout->addWidget(wbGroup);

    // --- Contaminación lumínica ---
    auto* lpGroup = new QGroupBox(tr("Contaminación lumínica"), this);
    auto* lpLay = new QVBoxLayout(lpGroup);
    lpLay->setContentsMargins(8, 16, 8, 8);
    lpLay->setSpacing(4);

    {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(tr("Sodio (589 nm)"), lpGroup);
        lbl->setMinimumWidth(80);
        row->addWidget(lbl);
        sodiumSlider_ = makeSlider(0, 100, 0, 10, lpGroup);
        sodiumSlider_->setToolTip(tr("Atenuación del tinte naranja de farolas de sodio"));
        row->addWidget(sodiumSlider_);
        sodiumValueLabel_ = makeValueLabel("0%", lpGroup);
        row->addWidget(sodiumValueLabel_);
        lpLay->addLayout(row);
    }
    {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(tr("Mercurio (546 nm)"), lpGroup);
        lbl->setMinimumWidth(80);
        row->addWidget(lbl);
        mercurySlider_ = makeSlider(0, 100, 0, 10, lpGroup);
        mercurySlider_->setToolTip(tr("Atenuación del tinte verde de farolas de mercurio"));
        row->addWidget(mercurySlider_);
        mercuryValueLabel_ = makeValueLabel("0%", lpGroup);
        row->addWidget(mercuryValueLabel_);
        lpLay->addLayout(row);
    }

    mainLayout->addWidget(lpGroup);

    // --- Ruido ---
    auto* denoiseGroup = new QGroupBox(tr("Reducción de ruido"), this);
    auto* denLay = new QVBoxLayout(denoiseGroup);
    denLay->setContentsMargins(8, 16, 8, 8);

    {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(tr("Intensidad"), denoiseGroup);
        lbl->setMinimumWidth(80);
        row->addWidget(lbl);
        denoiseSlider_ = makeSlider(0, 100, 0, 10, denoiseGroup);
        denoiseSlider_->setToolTip(tr("Reducción de ruido (fastNlMeans). "
                                       "0 = desactivado"));
        row->addWidget(denoiseSlider_);
        denoiseValueLabel_ = makeValueLabel("0", denoiseGroup);
        row->addWidget(denoiseValueLabel_);
        denLay->addLayout(row);
    }

    mainLayout->addWidget(denoiseGroup);

    // --- Exposición / Brillo / Contraste ---
    auto* toneGroup = new QGroupBox(tr("Tono"), this);
    auto* toneLay = new QVBoxLayout(toneGroup);
    toneLay->setContentsMargins(8, 16, 8, 8);
    toneLay->setSpacing(4);

    {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(tr("Exposición (EV)"), toneGroup);
        lbl->setMinimumWidth(80);
        row->addWidget(lbl);
        exposureSlider_ = makeSlider(ImageAdjustLimits::kMinEv,
                                     ImageAdjustLimits::kMaxEv, 0, 10, toneGroup);
        exposureSlider_->setToolTip(tr("Ajuste de exposición en EV (×10)"));
        row->addWidget(exposureSlider_);
        exposureValueLabel_ = makeValueLabel("0.0", toneGroup);
        row->addWidget(exposureValueLabel_);
        toneLay->addLayout(row);
    }
    {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(tr("Brillo"), toneGroup);
        lbl->setMinimumWidth(80);
        row->addWidget(lbl);
        brightnessSlider_ = makeSlider(ImageAdjustLimits::kMinBrightness,
                                       ImageAdjustLimits::kMaxBrightness, 0, 10,
                                       toneGroup);
        brightnessSlider_->setToolTip(tr("Brillo (-100..+100)"));
        row->addWidget(brightnessSlider_);
        brightnessValueLabel_ = makeValueLabel("0", toneGroup);
        row->addWidget(brightnessValueLabel_);
        toneLay->addLayout(row);
    }
    {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(tr("Contraste"), toneGroup);
        lbl->setMinimumWidth(80);
        row->addWidget(lbl);
        contrastSlider_ = makeSlider(ImageAdjustLimits::kMinContrast,
                                     ImageAdjustLimits::kMaxContrast, 0, 10,
                                     toneGroup);
        contrastSlider_->setToolTip(tr("Contraste (-100..+100)"));
        row->addWidget(contrastSlider_);
        contrastValueLabel_ = makeValueLabel("0", toneGroup);
        row->addWidget(contrastValueLabel_);
        toneLay->addLayout(row);
    }

    mainLayout->addWidget(toneGroup);

    // --- Botones: Restablecer + Deshacer ---
    auto* btnRow = new QHBoxLayout();
    resetBtn_ = new QPushButton(tr("Restablecer todo"), this);
    resetBtn_->setToolTip(tr("Restaurar todos los valores a los predeterminados"));
    undoBtn_ = new QPushButton(tr("Deshacer"), this);
    undoBtn_->setToolTip(tr("Deshacer el último cambio"));
    undoBtn_->setEnabled(false);
    btnRow->addWidget(resetBtn_);
    btnRow->addWidget(undoBtn_);
    mainLayout->addLayout(btnRow);

    mainLayout->addStretch();

    // --- Atajo Ctrl+Z para deshacer ---
    auto* undoShortcut = new QShortcut(QKeySequence::Undo, this);
    connect(undoShortcut, &QShortcut::activated, this, &ImageAdjustPanel::onUndoClicked);

    // --- Conexiones ---
    connect(wbSlider_, &QSlider::valueChanged, this, &ImageAdjustPanel::onWbSliderChanged);
    connect(wbSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ImageAdjustPanel::onWbSpinChanged);
    connect(sodiumSlider_, &QSlider::valueChanged, this, &ImageAdjustPanel::onSodiumChanged);
    connect(mercurySlider_, &QSlider::valueChanged, this, &ImageAdjustPanel::onMercuryChanged);
    connect(denoiseSlider_, &QSlider::valueChanged, this, &ImageAdjustPanel::onDenoiseChanged);
    connect(exposureSlider_, &QSlider::valueChanged, this, &ImageAdjustPanel::onExposureChanged);
    connect(brightnessSlider_, &QSlider::valueChanged, this, &ImageAdjustPanel::onBrightnessChanged);
    connect(contrastSlider_, &QSlider::valueChanged, this, &ImageAdjustPanel::onContrastChanged);
    connect(resetBtn_, &QPushButton::clicked, this, &ImageAdjustPanel::onResetClicked);
    connect(detectWbBtn_, &QPushButton::clicked, this, &ImageAdjustPanel::onDetectWbClicked);
    connect(undoBtn_, &QPushButton::clicked, this, &ImageAdjustPanel::onUndoClicked);

    // Gestos del slider WB para undo (una presidency = un paso).
    connect(wbSlider_, &QSlider::sliderPressed, this, [this]() {
        lastState_ = currentAdjust();
    });
    connect(wbSlider_, &QSlider::sliderReleased, this, [this]() {
        const ImageAdjust now = currentAdjust();
        if (now != lastState_) {
            undoDeque_.push_back(lastState_);
            if (undoDeque_.size() > kMaxUndo)
                undoDeque_.pop_front();
            undoBtn_->setEnabled(true);
        }
    });

    // Ocultar el alcance en modo vídeo
    scopeCombo_->setVisible(false);
}

ImageAdjust ImageAdjustPanel::currentAdjust() const
{
    ImageAdjust a;
    a.wbKelvin = wbSlider_->value();
    a.lpSodium = sodiumSlider_->value();
    a.lpMercury = mercurySlider_->value();
    a.denoise = denoiseSlider_->value();
    a.exposureEv = exposureSlider_->value();
    a.brightness = brightnessSlider_->value();
    a.contrast = contrastSlider_->value();
    return a;
}

void ImageAdjustPanel::setAdjust(const ImageAdjust& adj)
{
    // Llamada programática (cambio de pestaña, proyecto, etc.): limpiar undo.
    undoDeque_.clear();
    undoBtn_->setEnabled(false);
    lastState_ = adj;

    suppressUndo_ = true;
    wbSlider_->setValue(adj.wbKelvin);
    wbSpin_->setValue(adj.wbKelvin);
    sodiumSlider_->setValue(adj.lpSodium);
    mercurySlider_->setValue(adj.lpMercury);
    denoiseSlider_->setValue(adj.denoise);
    exposureSlider_->setValue(adj.exposureEv);
    brightnessSlider_->setValue(adj.brightness);
    contrastSlider_->setValue(adj.contrast);
    updateWbLabel();
    suppressUndo_ = false;
}

void ImageAdjustPanel::setMode(Mode mode)
{
    mode_ = mode;
    scopeCombo_->setVisible(mode == Mode::Photos);
}

void ImageAdjustPanel::setDetectedKelvin(int kelvin)
{
    detectedKelvin_ = kelvin;
    if (kelvin > 0)
        wbDetectedLabel_->setText(tr("EXIF: %1 K").arg(kelvin));
    else
        wbDetectedLabel_->setText(tr("EXIF: —"));
}

void ImageAdjustPanel::clearHistory()
{
    undoDeque_.clear();
    undoBtn_->setEnabled(false);
}

void ImageAdjustPanel::onWbSliderChanged(int value)
{
    if (!suppressUndo_) {
        wbSpin_->blockSignals(true);
        wbSpin_->setValue(value);
        wbSpin_->blockSignals(false);
    }
    updateWbLabel();
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onWbSpinChanged(int value)
{
    if (!suppressUndo_) {
        // Capturar estado antes del cambio (igual que slider drag).
        lastState_ = currentAdjust();
        // QSpinBox puede tener el antiguo valor; lo ignoramos y usamos value.
        suppressUndo_ = true;
        wbSlider_->setValue(value);
        suppressUndo_ = false;
        // Empujar undo por el cambio anterior.
        undoDeque_.push_back(lastState_);
        if (undoDeque_.size() > kMaxUndo)
            undoDeque_.pop_front();
        undoBtn_->setEnabled(true);
    }
    updateWbLabel();
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onSodiumChanged(int value)
{
    sodiumValueLabel_->setText(tr("%1%").arg(value));
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onMercuryChanged(int value)
{
    mercuryValueLabel_->setText(tr("%1%").arg(value));
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onDenoiseChanged(int value)
{
    denoiseValueLabel_->setText(QString::number(value));
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onExposureChanged(int value)
{
    const float ev = static_cast<float>(value) / 10.f;
    exposureValueLabel_->setText(tr("%1").arg(ev, 0, 'f', 1));
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onBrightnessChanged(int value)
{
    brightnessValueLabel_->setText(QString::number(value));
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onContrastChanged(int value)
{
    contrastValueLabel_->setText(QString::number(value));
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onResetClicked()
{
    // Restablecer: limpiar undo y emitir defaults.
    undoDeque_.clear();
    undoBtn_->setEnabled(false);
    suppressUndo_ = true;
    wbSlider_->setValue(0);
    wbSpin_->setValue(ImageAdjustLimits::kDefaultKelvin);
    sodiumSlider_->setValue(0);
    mercurySlider_->setValue(0);
    denoiseSlider_->setValue(0);
    exposureSlider_->setValue(0);
    brightnessSlider_->setValue(0);
    contrastSlider_->setValue(0);
    updateWbLabel();
    suppressUndo_ = false;
    emit resetRequested();
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onDetectWbClicked()
{
    // Usar EXIF si existe; si no, 11250 K.
    const int target = (detectedKelvin_ >= ImageAdjustLimits::kMinKelvin &&
                        detectedKelvin_ <= ImageAdjustLimits::kMaxKelvin)
                           ? detectedKelvin_
                           : ImageAdjustLimits::kDefaultKelvin;
    // Empujar estado actual al undo antes del cambio.
    undoDeque_.push_back(currentAdjust());
    if (undoDeque_.size() > kMaxUndo)
        undoDeque_.pop_front();
    undoBtn_->setEnabled(true);

    suppressUndo_ = true;
    wbSlider_->setValue(target);
    wbSpin_->setValue(target);
    suppressUndo_ = false;
    updateWbLabel();
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onUndoClicked()
{
    if (undoDeque_.empty())
        return;
    const ImageAdjust prev = undoDeque_.back();
    undoDeque_.pop_back();
    undoBtn_->setEnabled(!undoDeque_.empty());

    suppressUndo_ = true;
    wbSlider_->setValue(prev.wbKelvin);
    wbSpin_->setValue(prev.wbKelvin);
    sodiumSlider_->setValue(prev.lpSodium);
    mercurySlider_->setValue(prev.lpMercury);
    denoiseSlider_->setValue(prev.denoise);
    exposureSlider_->setValue(prev.exposureEv);
    brightnessSlider_->setValue(prev.brightness);
    contrastSlider_->setValue(prev.contrast);
    updateWbLabel();
    suppressUndo_ = false;
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onScopeChanged(int index)
{
    emit scopeChanged(index == 1);
}

void ImageAdjustPanel::blockSignals_(bool block)
{
    const auto widgets = {static_cast<QWidget*>(wbSlider_),
                          static_cast<QWidget*>(wbSpin_),
                          static_cast<QWidget*>(sodiumSlider_),
                          static_cast<QWidget*>(mercurySlider_),
                          static_cast<QWidget*>(denoiseSlider_),
                          static_cast<QWidget*>(exposureSlider_),
                          static_cast<QWidget*>(brightnessSlider_),
                          static_cast<QWidget*>(contrastSlider_)};
    for (auto* w : widgets)
        w->blockSignals(block);
}

void ImageAdjustPanel::updateWbLabel()
{
    const int v = wbSlider_->value();
    wbValueLabel_->setText(tr("%1 K").arg(v));
}
