// VertexItem.h
#ifndef VERTEXITEM_H
#define VERTEXITEM_H

#include <QGraphicsEllipseItem>
#include <QGraphicsSceneMouseEvent>
#include <QObject>

class VertexItem : public QObject, public QGraphicsEllipseItem
{
    Q_OBJECT

public:
    VertexItem(int sectorIndex, int vertexIndex, QGraphicsItem *parent = nullptr);

    int getSectorIndex() const { return sectorIdx; }
    int getVertexIndex() const { return vertexIdx; }

signals:
    void vertexMoved(int sectorIndex, int vertexIndex, QPointF newPosition);

protected:
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    int sectorIdx;
    int vertexIdx;
};

#endif

