// VertexItem.h
#ifndef VERTEXITEM_H
#define VERTEXITEM_H

#include <QGraphicsEllipseItem>
#include <QGraphicsSceneMouseEvent>

class VertexItem : public QGraphicsEllipseItem
{
public:
    VertexItem(int sectorIndex, int vertexIndex, QGraphicsItem *parent = nullptr);

    int getSectorIndex() const { return sectorIdx; }
    int getVertexIndex() const { return vertexIdx; }

protected:
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    int sectorIdx;
    int vertexIdx;
};

#endif
