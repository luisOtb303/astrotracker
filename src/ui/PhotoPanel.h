#pragma once

#include "stills/PhotoSequenceReader.h"

#include <QWidget>
#include <memory>

class QAction;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QSlider;
class VideoView;

// Pestaña "Fotos": abre una secuencia de fotos (JPG/PNG/TIFF/BMP) y muestra
// el filmstrip completo, con dos visores (Original | Centrado). En M3/M4 se
// añadirán el seguimiento por círculo y la exportación.
class PhotoPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PhotoPanel(QWidget* parent = nullptr);

    bool isOpen() const { return reader_.isOpen(); }
    int64_t currentIndex() const { return current_; }

    void openImagesDialog();
    void openFolderDialog();

private slots:
    void onItemActivated(QListWidgetItem* item);
    void onSliderChanged(int value);
    void showPrev();
    void showNext();
    void showCurrent();

private:
    void openPaths(const QStringList& paths);
    void reloadSequence();
    void buildFilmstrip();
    void updateNavUi();
    void clearSession();
    static QPixmap toPixmap(const cv::Mat& bgr);

    PhotoSequenceReader reader_;
    QListWidget* filmstrip_ = nullptr;
    VideoView* view_ = nullptr;
    VideoView* resultView_ = nullptr;
    QSlider* slider_ = nullptr;
    QLabel* indexLabel_ = nullptr;
    QLabel* infoLabel_ = nullptr;
    QAction* prevAction_ = nullptr;
    QAction* nextAction_ = nullptr;
    QAction* analyzeAction_ = nullptr;
    QAction* exportAction_ = nullptr;
    int64_t current_ = 0;
    int displayMaxDim_ = 1600;
    int thumbMaxDim_ = 240;
};