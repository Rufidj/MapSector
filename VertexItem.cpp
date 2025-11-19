// VertexItem.cpp
#include "VertexItem.h"

VertexItem::VertexItem(int sectorIndex, int vertexIndex, QGraphicsItem *parent)
    : QGraphicsEllipseItem(parent), sectorIdx(sectorIndex), vertexIdx(vertexIndex)
{
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
}

void VertexItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    QGraphicsEllipseItem::mouseReleaseEvent(event);

    // Emitir señal para actualizar el sector
    // (necesitarás conectar esto con MainWindow)
}
