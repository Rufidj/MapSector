// VertexItem.cpp
#include "VertexItem.h"

VertexItem::VertexItem(int sectorIndex, int vertexIndex, QGraphicsItem *parent)
    : QObject(), QGraphicsEllipseItem(parent), sectorIdx(sectorIndex), vertexIdx(vertexIndex)
{
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
}

void VertexItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsEllipseItem::mouseReleaseEvent(event);

    // Emitir señal con la nueva posición del vértice
    QPointF center = rect().center() + pos();
    emit vertexMoved(sectorIdx, vertexIdx, center);
}


