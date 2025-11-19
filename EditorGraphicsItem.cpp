// EditorGraphicsItem.cpp
#include "EditorGraphicsItem.h"
#include <QPen>
#include <QBrush>

EditorSectorItem::EditorSectorItem(const EditorSector &s, QGraphicsItem *parent)
    : QGraphicsPolygonItem(parent), sector(s), dragging(false)
{
    QPolygonF polygon;
    for (const QPointF &vertex : sector.vertices) {
        polygon << vertex;
    }
    setPolygon(polygon);
    setPen(QPen(Qt::blue, 2));
    setBrush(QBrush(QColor(100, 100, 255, 50)));

    // Hacer el item seleccionable y movible
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemIsMovable);
}

void EditorSectorItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        dragging = true;
        dragStartPos = event->scenePos();
    }
    QGraphicsPolygonItem::mousePressEvent(event);
}

void EditorSectorItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    if (dragging) {
        QPointF delta = event->scenePos() - dragStartPos;

        // Actualizar vértices del sector
        for (QPointF &vertex : sector.vertices) {
            vertex += delta;
        }

        // Actualizar polígono visual
        QPolygonF polygon;
        for (const QPointF &vertex : sector.vertices) {
            polygon << vertex;
        }
        setPolygon(polygon);

        dragStartPos = event->scenePos();
    }
    QGraphicsPolygonItem::mouseMoveEvent(event);
}

void EditorSectorItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        dragging = false;
    }
    QGraphicsPolygonItem::mouseReleaseEvent(event);
}
