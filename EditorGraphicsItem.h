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
    EditorSectorItem(int sectorIndex, const ModernRegion &region, QGraphicsItem *parent = nullptr);

    ModernRegion getRegion() const { return region; }
    void updateFromRegion(const ModernRegion &newRegion);
    int getSectorIndex() const { return sectorIdx; }

signals:
    void sectorMoved(int sectorIndex, QPointF delta);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

private:
    ModernRegion region;
    int sectorIdx;
};

#endif
