#include "EditorScene.h"

EditorScene::EditorScene(QObject *parent)
    : QGraphicsScene(parent), drawingMode(false), wallDrawingMode(false), wallPointCount(0)
{
}

void EditorScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (drawingMode && event->button() == Qt::LeftButton) {
        // Modo dibujo de sectores
        emit vertexAdded(event->scenePos());
    } else if (event->button() == Qt::RightButton && drawingMode) {
        // Finalizar polígono con clic derecho
        emit polygonFinished();
        drawingMode = false;
    } else if (wallDrawingMode && event->button() == Qt::LeftButton) {
        // Modo dibujo de paredes
        emit wallPointAdded(event->scenePos());
        wallPointCount++;

        // Después de 2 puntos, finalizar automáticamente
        if (wallPointCount >= 2) {
            emit wallFinished();
            wallDrawingMode = false;
            wallPointCount = 0;
        }
    }

    // Llamar a la implementación base para mantener funcionalidad normal
    QGraphicsScene::mousePressEvent(event);
}
