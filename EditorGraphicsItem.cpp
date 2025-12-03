// EditorGraphicsItem.cpp
#include "EditorGraphicsItem.h"
#include <QPen>
#include <QBrush>

EditorSectorItem::EditorSectorItem(int sectorIndex, const ModernRegion &r, QGraphicsItem *parent)
    : QObject(), QGraphicsPolygonItem(parent), region(r), sectorIdx(sectorIndex)
{
    QPolygonF polygon;
    for (const ModernPoint &point : region.points) {
        polygon << QPointF(point.x, point.y);
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
        QPointF newPos = value.toPointF();
        QPointF delta = newPos;

        // Actualizar puntos en coordenadas de escena
        QPolygonF polygon = this->polygon();
        region.points.clear();
        for (int i = 0; i < polygon.size(); i++) {
            QPointF scenePoint = mapToScene(polygon[i]);
            ModernPoint point;
            point.x = (int32_t)scenePoint.x();
            point.y = (int32_t)scenePoint.y();
            point.active = 1;
            region.points.push_back(point);
        }

        emit sectorMoved(sectorIdx, delta);
    }
    return QGraphicsPolygonItem::itemChange(change, value);
}

void EditorSectorItem::updateFromRegion(const ModernRegion &newRegion) {
    region = newRegion;
    QPolygonF polygon;
    for (const ModernPoint &point : region.points) {
        polygon << QPointF(point.x, point.y);
    }
    setPolygon(polygon);
}
