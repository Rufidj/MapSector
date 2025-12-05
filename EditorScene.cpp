#include "EditorScene.h"

EditorScene::EditorScene(QObject *parent)
    : QGraphicsScene(parent), drawingMode(false), wallDrawingMode(false), wallPointCount(0)
{
}

void EditorScene::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    if (drawingMode && event->button() == Qt::LeftButton) {
        emit vertexAdded(event->scenePos());
    } else if (event->button() == Qt::RightButton && drawingMode) {
        emit polygonFinished();
        drawingMode = false;
    } else if (wallDrawingMode && event->button() == Qt::LeftButton) {
        emit wallPointAdded(event->scenePos());
        wallPointCount++;
        if (wallPointCount >= 2) {
            emit wallFinished();
            wallDrawingMode = false;
            wallPointCount = 0;
        }
    } else if (event->button() == Qt::LeftButton && !drawingMode && !wallDrawingMode) {
        // Detección de clic en sector
        QGraphicsItem *item = itemAt(event->scenePos(), QTransform());
        if (item && item->type() == QGraphicsPolygonItem::Type) {
            QVariant sectorData = item->data(0);
            if (sectorData.isValid()) {
                emit sectorClicked(sectorData.toInt());
                return; // Importante: no procesar más eventos
            }
        }
    }

    QGraphicsScene::mousePressEvent(event);
}

void EditorScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event) {
    emit mouseMoved(event->scenePos());
    QGraphicsScene::mouseMoveEvent(event);
}
