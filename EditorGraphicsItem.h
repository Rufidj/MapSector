// EditorGraphicsItem.h
#ifndef EDITORGRAPHICSITEM_H
#define EDITORGRAPHICSITEM_H

#include <QGraphicsPolygonItem>
#include <QGraphicsSceneMouseEvent>
#include "MapStructures.h"

class EditorSectorItem : public QGraphicsPolygonItem
{
public:
    EditorSectorItem(const EditorSector &sector, QGraphicsItem *parent = nullptr);

    EditorSector getSector() const { return sector; }
    void updateFromSector(const EditorSector &newSector);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    EditorSector sector;
    bool dragging;
    QPointF dragStartPos;
};

#endif
