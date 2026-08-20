#pragma once

#include <QListWidget>

class QMouseEvent;

// Filmstrip del modo "Fotos" con una casilla de selección por miniatura:
// el clic sobre el icono navega a la foto, pero el clic sobre la pequeña
// casilla solo marca/desmarca la foto (para la exportación), sin navegar.
class PhotoFilmstrip : public QListWidget
{
    Q_OBJECT

public:
    explicit PhotoFilmstrip(QWidget* parent = nullptr)
        : QListWidget(parent)
    {
    }

protected:
    void mouseReleaseEvent(QMouseEvent* e) override;
};