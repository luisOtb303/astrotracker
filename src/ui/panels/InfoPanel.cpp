#include "ui/panels/InfoPanel.h"

#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QStringList>
#include <QVBoxLayout>
#include <cmath>

namespace {
QLabel* makeSelectable(QLabel* label)
{
    label->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                   Qt::TextSelectableByKeyboard);
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    return label;
}
} // namespace

InfoPanel::InfoPanel(QWidget* parent)
    : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto* content = new QWidget(scroll);
    auto* mainLayout = new QVBoxLayout(content);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(6);

    // --- Tracking Stats ---
    trackingSection_ = new QGroupBox(tr("Tracking Stats"), content);
    auto* tlay = new QVBoxLayout(trackingSection_);
    tlay->setContentsMargins(8, 16, 8, 8);
    tlay->setSpacing(4);

    validCount_ = makeSelectable(new QLabel("--", content));
    validCount_->setStyleSheet("color: #2e8b57;");
    tlay->addWidget(validCount_);

    predictedCount_ = makeSelectable(new QLabel("--", content));
    predictedCount_->setStyleSheet("color: #ffa500;");
    tlay->addWidget(predictedCount_);

    lostCount_ = makeSelectable(new QLabel("--", content));
    lostCount_->setStyleSheet("color: #c04040;");
    tlay->addWidget(lostCount_);

    trackingPercent_ = makeSelectable(new QLabel("--", content));
    tlay->addWidget(trackingPercent_);

    mainLayout->addWidget(trackingSection_);

    // --- Frame Info ---
    frameSection_ = new QGroupBox(tr("Frame Info"), content);
    auto* flay = new QVBoxLayout(frameSection_);
    flay->setContentsMargins(8, 16, 8, 8);
    flay->setSpacing(4);

    frameIndexLabel_ = makeSelectable(new QLabel("--", content));
    flay->addWidget(frameIndexLabel_);

    timeLabel_ = makeSelectable(new QLabel("--", content));
    flay->addWidget(timeLabel_);

    methodLabel_ = makeSelectable(new QLabel("--", content));
    flay->addWidget(methodLabel_);

    confidenceLabel_ = makeSelectable(new QLabel("--", content));
    flay->addWidget(confidenceLabel_);

    offsetLabel_ = makeSelectable(new QLabel("--", content));
    flay->addWidget(offsetLabel_);

    statusLabel_ = makeSelectable(new QLabel("--", content));
    flay->addWidget(statusLabel_);

    mainLayout->addWidget(frameSection_);

    // --- Archivo y metadatos (fotos: fichero + EXIF; vídeo: fichero + frame) ---
    fileSection_ = new QGroupBox(tr("Archivo y metadatos"), content);
    auto* elay = new QVBoxLayout(fileSection_);
    elay->setContentsMargins(8, 16, 8, 8);
    elay->setSpacing(4);

    fileValueName_ = makeSelectable(new QLabel("--", content));
    elay->addWidget(fileValueName_);

    fileValuePath_ = makeSelectable(new QLabel("--", content));
    elay->addWidget(fileValuePath_);

    fileValueSize_ = makeSelectable(new QLabel("--", content));
    elay->addWidget(fileValueSize_);

    fileValueType_ = makeSelectable(new QLabel("--", content));
    elay->addWidget(fileValueType_);

    fileValueDate_ = makeSelectable(new QLabel("--", content));
    elay->addWidget(fileValueDate_);

    fileValueDimension_ = makeSelectable(new QLabel("--", content));
    elay->addWidget(fileValueDimension_);

    fileValueExtra_ = makeSelectable(new QLabel("--", content));
    elay->addWidget(fileValueExtra_);

    mainLayout->addWidget(fileSection_);
    mainLayout->addStretch(1);

    scroll->setWidget(content);
}

void InfoPanel::setTrackingStats(int valid, int predicted, int lost)
{
    const int total = valid + predicted + lost;
    validCount_->setText(tr("Valid: %1").arg(valid));
    predictedCount_->setText(tr("Predicted: %1").arg(predicted));
    lostCount_->setText(tr("Lost: %1").arg(lost));

    if (total > 0) {
        const double pct = 100.0 * valid / total;
        trackingPercent_->setText(tr("Success: %1%").arg(pct, 0, 'f', 1));
    } else {
        trackingPercent_->setText("--");
    }
}

void InfoPanel::setFrameInfo(int frameIndex, int totalFrames,
                             const QString& time,
                             const QString& method,
                             float confidence,
                             float offsetX, float offsetY,
                             const QString& status)
{
    frameIndexLabel_->setText(tr("#%1 / %2").arg(frameIndex).arg(totalFrames));
    timeLabel_->setText(time);
    methodLabel_->setText(tr("Method: %1").arg(method));
    confidenceLabel_->setText(tr("Confidence: %1%").arg(std::lround(confidence * 100)));
    offsetLabel_->setText(tr("Offset: %1, %2")
                              .arg(offsetX, 0, 'f', 1)
                              .arg(offsetY, 0, 'f', 1));
    statusLabel_->setText(tr("Status: %1").arg(status));
}

QString InfoPanel::formatSize(qint64 bytes)
{
    const double kb = 1024.0;
    if (bytes >= kb * kb * kb)
        return tr("%1 GB").arg(bytes / (kb * kb * kb), 0, 'f', 2);
    if (bytes >= kb * kb)
        return tr("%1 MB").arg(bytes / (kb * kb), 0, 'f', 2);
    if (bytes >= kb)
        return tr("%1 KB").arg(bytes / kb, 0, 'f', 1);
    return tr("%1 B").arg(bytes);
}

void InfoPanel::setFileAndExif(const PhotoFileInfo& file, const PhotoExifInfo& exif)
{
    fileValueName_->setText(tr("Nombre: %1")
                                .arg(QString::fromStdString(file.name)));
    fileValuePath_->setText(tr("Ruta: %1")
                                .arg(QString::fromStdString(file.path)));
    fileValueSize_->setText(tr("Tamaño: %1").arg(formatSize(file.sizeBytes)));
    fileValueType_->setText(tr("Tipo: %1")
                                .arg(QString::fromStdString(file.type)));
    fileValueDate_->setText(tr("Fecha fichero: %1")
                                .arg(QString::fromStdString(file.modifyDate)));
    if (file.width > 0 && file.height > 0)
        fileValueDimension_->setText(tr("Dimensión: %1 x %2")
                                         .arg(file.width)
                                         .arg(file.height));
    else
        fileValueDimension_->setText(tr("Dimensión: --"));

    QStringList lines;
    if (exif.hasCamera())
        lines << tr("Cámara: %1").arg(QString::fromStdString(exif.camera));
    if (exif.hasLens())
        lines << tr("Objetivo: %1").arg(QString::fromStdString(exif.lens));
    if (exif.hasFocal())
        lines << tr("Focal: %1").arg(QString::fromStdString(exif.focalString()));
    if (exif.hasAperture())
        lines << tr("Apertura: %1").arg(QString::fromStdString(exif.apertureString()));
    if (exif.hasShutter())
        lines << tr("Obturación: %1").arg(QString::fromStdString(exif.shutterString()));
    if (exif.hasIso())
        lines << tr("ISO: %1").arg(exif.iso);
    if (exif.hasDate())
        lines << tr("Fecha disparo: %1").arg(QString::fromStdString(exif.date));
    fileValueExtra_->setText(lines.isEmpty() ? tr("EXIF: sin datos")
                                             : lines.join('\n'));
}

void InfoPanel::setVideoFileInfo(const QString& name, const QString& path,
                                 qint64 sizeBytes, const QString& modifyDate,
                                 int frameIndex, int totalFrames)
{
    fileValueName_->setText(tr("Nombre: %1").arg(name));
    fileValuePath_->setText(tr("Ruta: %1").arg(path));
    fileValueSize_->setText(tr("Tamaño: %1").arg(formatSize(sizeBytes)));
    fileValueType_->setText(tr("Tipo: Vídeo"));
    fileValueDate_->setText(tr("Fecha fichero: %1").arg(modifyDate));
    fileValueDimension_->setText(tr("Frame: %1 / %2").arg(frameIndex + 1).arg(totalFrames));
    fileValueExtra_->setText(tr("Posición: %1 / %2").arg(frameIndex + 1).arg(totalFrames));
}

void InfoPanel::clearExifInfo()
{
    fileValueExtra_->setText("--");
}

void InfoPanel::clearFileInfo()
{
    clearExifInfo();
    fileValueName_->setText(tr("Nombre: --"));
    fileValuePath_->setText(tr("Ruta: --"));
    fileValueSize_->setText(tr("Tamaño: --"));
    fileValueType_->setText(tr("Tipo: --"));
    fileValueDate_->setText(tr("Fecha fichero: --"));
    fileValueDimension_->setText(tr("--"));
}

void InfoPanel::setMaterialInfo(const QString& info)
{
    frameIndexLabel_->setText(info);
}

void InfoPanel::setTrackingVisible(bool visible)
{
    trackingSection_->setVisible(visible);
}

void InfoPanel::setFrameInfoVisible(bool visible)
{
    frameSection_->setVisible(visible);
}

void InfoPanel::setFileInfoVisible(bool visible)
{
    fileSection_->setVisible(visible);
}

void InfoPanel::setExifVisible(bool visible)
{
    // Obsoleto: la sección única "Archivo y metadatos" engloba el EXIF. Se
    // mantiene la firma para no romper llamadas; la visibilidad la gestiona
    // setFileInfoVisible en el modo Fotos.
    Q_UNUSED(visible);
}
