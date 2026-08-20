#include "ui/PhotoFilmstrip.h"

#include <QListWidgetItem>
#include <QMouseEvent>
#include <QRect>

void PhotoFilmstrip::mouseReleaseEvent(QMouseEvent* e)
{
    QListWidgetItem* item = itemAt(e->pos());
    if (item && (item->flags() & Qt::ItemIsUserCheckable)) {
        // Zona de la casilla: esquina superior izquierda del rect del item.
        const QRect r = visualItemRect(item);
        const QRect box(r.x(), r.y(), 20, 20);
        if (box.contains(e->pos())) {
            item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked
                                                                  : Qt::Checked);
            return;
        }
    }
    QListWidget::mouseReleaseEvent(e);
}