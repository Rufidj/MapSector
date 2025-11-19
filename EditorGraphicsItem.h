// EditorGraphicsItem.h
#ifndef EDITORGRAPHICSITEM_H
#define EDITORGRAPHICSITEM_H

#include <QGraphicsPolygonItem>
#include <QGraphicsSceneMouseEvent>
#include <QObject>
#include "MapStructures.h"

class EditorSectorItem : public QObject, public QGraphicsPolygonItem
{
    Q_OBJECT

public:
    EditorSectorItem(int sectorIndex, const EditorSector &sector, QGraphicsItem *parent = nullptr);

    EditorSector getSector() const { return sector; }
    void updateFromSector(const EditorSector &newSector);
    int getSectorIndex() const { return sectorIdx; }

signals:
    void sectorMoved(int sectorIndex, QPointF delta);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    EditorSector sector;
    int sectorIdx;
};

#endif
