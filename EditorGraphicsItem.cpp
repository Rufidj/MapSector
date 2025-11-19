// EditorGraphicsItem.cpp
#include "EditorGraphicsItem.h"
#include <QPen>
#include <QBrush>

EditorSectorItem::EditorSectorItem(int sectorIndex, const EditorSector &s, QGraphicsItem *parent)
    : QObject(), QGraphicsPolygonItem(parent), sector(s), sectorIdx(sectorIndex)
{
    QPolygonF polygon;
    for (const QPointF &vertex : sector.vertices) {
        polygon << vertex;
    }
    setPolygon(polygon);
    setPen(QPen(Qt::blue, 2));
    setBrush(QBrush(QColor(100, 100, 255, 50)));

    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
}

QVariant EditorSectorItem::itemChange(GraphicsItemChange change, const QVariant &value) {
    if (change == ItemPositionHasChanged) {
        // Actualizar vértices del sector basándose en la nueva posición del item
        QPointF newPos = value.toPointF();
        QPointF delta = newPos;

        // Recalcular vértices en coordenadas de escena
        QPolygonF polygon = this->polygon();
        sector.vertices.clear();
        for (const QPointF &point : polygon) {
            sector.vertices.append(mapToScene(point));
        }

        emit sectorMoved(sectorIdx, delta);
    }
    return QGraphicsPolygonItem::itemChange(change, value);
}

void EditorSectorItem::updateFromSector(const EditorSector &newSector) {
    sector = newSector;
    QPolygonF polygon;
    for (const QPointF &vertex : sector.vertices) {
        polygon << vertex;
    }
    setPolygon(polygon);
}
