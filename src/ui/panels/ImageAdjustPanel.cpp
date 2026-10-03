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

    // --- Balance de blancos (calidez relativa al neutro) ---
    auto* wbGroup = new QGroupBox(tr("Balance de blancos"), this);
    auto* wbLay = new QVBoxLayout(wbGroup);
    wbLay->setContentsMargins(8, 16, 8, 8);
    wbLay->setSpacing(4);

    auto* wbRow = new QHBoxLayout();
    wbSlider_ = makeSlider(ImageAdjustLimits::kMinWarmth,
                           ImageAdjustLimits::kMaxWarmth, 0, 10, wbGroup);
    wbSlider_->setToolTip(
        tr("El centro (0) deja la foto tal como sale de la cámara o del archivo.\n"
           "A la derecha más cálida, a la izquierda más fría.\n"
           "No hay valores en grados porque no se puede saber con qué luz se tomó."));
    wbRow->addWidget(wbSlider_, 1);
    wbValueLabel_ = makeValueLabel(tr("Neutro"), wbGroup);
    wbRow->addWidget(wbValueLabel_);
    wbLay->addLayout(wbRow);

    auto* wbScaleRow = new QHBoxLayout();
    auto* coldLbl = new QLabel(tr("Frío"), wbGroup);
    auto* warmLbl = new QLabel(tr("Cálido"), wbGroup);
    coldLbl->setAlignment(Qt::AlignLeft);
    warmLbl->setAlignment(Qt::AlignRight);
    auto* neutralLbl = new QLabel(tr("◄ Neutro ►"), wbGroup);
    neutralLbl->setAlignment(Qt::AlignCenter);
    wbScaleRow->addWidget(coldLbl);
    wbScaleRow->addWidget(neutralLbl, 1);
    wbScaleRow->addWidget(warmLbl);
    wbLay->addLayout(wbScaleRow);

    auto* wbBtnRow = new QHBoxLayout();
    centerWarmthBtn_ = new QPushButton(tr("Centrar"), wbGroup);
    centerWarmthBtn_->setToolTip(tr("Vuelve al punto neutro: la foto queda tal como "
                                    "salió, sin corregir ni calentar ni enfriar"));
    wbBtnRow->addStretch();
    wbBtnRow->addWidget(centerWarmthBtn_);
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
    connect(sodiumSlider_, &QSlider::valueChanged, this, &ImageAdjustPanel::onSodiumChanged);
    connect(mercurySlider_, &QSlider::valueChanged, this, &ImageAdjustPanel::onMercuryChanged);
    connect(denoiseSlider_, &QSlider::valueChanged, this, &ImageAdjustPanel::onDenoiseChanged);
    connect(exposureSlider_, &QSlider::valueChanged, this, &ImageAdjustPanel::onExposureChanged);
    connect(brightnessSlider_, &QSlider::valueChanged, this, &ImageAdjustPanel::onBrightnessChanged);
    connect(contrastSlider_, &QSlider::valueChanged, this, &ImageAdjustPanel::onContrastChanged);
    connect(resetBtn_, &QPushButton::clicked, this, &ImageAdjustPanel::onResetClicked);
    connect(centerWarmthBtn_, &QPushButton::clicked, this,
            &ImageAdjustPanel::onCenterWarmthClicked);
    connect(undoBtn_, &QPushButton::clicked, this, &ImageAdjustPanel::onUndoClicked);

    // Historial de deshacer por arrastre: un gesto = un paso (no uno por tick).
    for (QSlider* s : {wbSlider_, sodiumSlider_, mercurySlider_, denoiseSlider_,
                        exposureSlider_, brightnessSlider_, contrastSlider_})
        installSliderUndo(s);

    // Ocultar el alcance en modo vídeo
    scopeCombo_->setVisible(false);
}

ImageAdjust ImageAdjustPanel::currentAdjust() const
{
    ImageAdjust a;
    a.wbWarmth = wbSlider_->value();
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
    wbSlider_->setValue(adj.wbWarmth);
    sodiumSlider_->setValue(adj.lpSodium);
    mercurySlider_->setValue(adj.lpMercury);
    denoiseSlider_->setValue(adj.denoise);
    exposureSlider_->setValue(adj.exposureEv);
    brightnessSlider_->setValue(adj.brightness);
    contrastSlider_->setValue(adj.contrast);
    updateWarmthLabel();
    suppressUndo_ = false;
}

void ImageAdjustPanel::setMode(Mode mode)
{
    mode_ = mode;
    scopeCombo_->setVisible(mode == Mode::Photos);
}

void ImageAdjustPanel::clearHistory()
{
    undoDeque_.clear();
    undoBtn_->setEnabled(false);
}

void ImageAdjustPanel::onWbSliderChanged(int value)
{
    Q_UNUSED(value)
    updateWarmthLabel();
    noteEdit();
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onSodiumChanged(int value)
{
    sodiumValueLabel_->setText(tr("%1%").arg(value));
    noteEdit();
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onMercuryChanged(int value)
{
    mercuryValueLabel_->setText(tr("%1%").arg(value));
    noteEdit();
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onDenoiseChanged(int value)
{
    denoiseValueLabel_->setText(QString::number(value));
    noteEdit();
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onExposureChanged(int value)
{
    const float ev = static_cast<float>(value) / 10.f;
    exposureValueLabel_->setText(tr("%1").arg(ev, 0, 'f', 1));
    noteEdit();
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onBrightnessChanged(int value)
{
    brightnessValueLabel_->setText(QString::number(value));
    noteEdit();
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onContrastChanged(int value)
{
    contrastValueLabel_->setText(QString::number(value));
    noteEdit();
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onResetClicked()
{
    // Restablecer: limpiar undo y emitir defaults.
    undoDeque_.clear();
    undoBtn_->setEnabled(false);
    suppressUndo_ = true;
    wbSlider_->setValue(0);
    sodiumSlider_->setValue(0);
    mercurySlider_->setValue(0);
    denoiseSlider_->setValue(0);
    exposureSlider_->setValue(0);
    brightnessSlider_->setValue(0);
    contrastSlider_->setValue(0);
    updateWarmthLabel();
    suppressUndo_ = false;
    lastState_ = currentAdjust();
    emit resetRequested();
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onCenterWarmthClicked()
{
    if (wbSlider_->value() == 0)
        return;
    pushUndo();
    suppressUndo_ = true;
    wbSlider_->setValue(0);
    suppressUndo_ = false;
    updateWarmthLabel();
    lastState_ = currentAdjust();
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
    wbSlider_->setValue(prev.wbWarmth);
    sodiumSlider_->setValue(prev.lpSodium);
    mercurySlider_->setValue(prev.lpMercury);
    denoiseSlider_->setValue(prev.denoise);
    exposureSlider_->setValue(prev.exposureEv);
    brightnessSlider_->setValue(prev.brightness);
    contrastSlider_->setValue(prev.contrast);
    updateWarmthLabel();
    suppressUndo_ = false;
    lastState_ = prev;
    emit adjustEdited(currentAdjust());
}

void ImageAdjustPanel::onScopeChanged(int index)
{
    emit scopeChanged(index == 1);
}

void ImageAdjustPanel::pushUndo()
{
    if (suppressUndo_)
        return;
    undoDeque_.push_back(lastState_);
    while (undoDeque_.size() > kMaxUndo)
        undoDeque_.pop_front();
    undoBtn_->setEnabled(true);
}

void ImageAdjustPanel::noteEdit()
{
    if (suppressUndo_ || dragging_)
        return;
    const ImageAdjust now = currentAdjust();
    if (now == lastState_)
        return;
    pushUndo();
    lastState_ = now;
}

void ImageAdjustPanel::installSliderUndo(QSlider* slider)
{
    connect(slider, &QSlider::sliderPressed, this, [this] {
        dragging_ = true;
        lastState_ = currentAdjust();
    });
    connect(slider, &QSlider::sliderReleased, this, [this] {
        dragging_ = false;
        if (currentAdjust() != lastState_) {
            pushUndo();
            lastState_ = currentAdjust();
        }
    });
}

void ImageAdjustPanel::updateWarmthLabel()
{
    const int v = wbSlider_->value();
    if (v == 0)
        wbValueLabel_->setText(tr("Neutro"));
    else if (v > 0)
        wbValueLabel_->setText(tr("Cálido +%1").arg(v));
    else
        wbValueLabel_->setText(tr("Frío %1").arg(v));
}
