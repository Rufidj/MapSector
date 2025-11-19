#include "EditorScene.h"

EditorScene::EditorScene(QObject *parent)
    : QGraphicsScene(parent), drawingMode(false)
{
}

void EditorScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (drawingMode && event->button() == Qt::LeftButton) {
        // Añadir vértice en modo dibujo
        emit vertexAdded(event->scenePos());
    } else if (event->button() == Qt::RightButton && drawingMode) {
        // Finalizar polígono con clic derecho
        emit polygonFinished();
        drawingMode = false;
    }

    // Llamar a la implementación base para mantener funcionalidad normal
    QGraphicsScene::mousePressEvent(event);
}
