#ifndef EDITORSCENE_H
#define EDITORSCENE_H

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QObject>
#include "EditorScene.h"
#include <QGraphicsItem>
#include <QGraphicsPolygonItem>
#include <QVariant>

class EditorScene : public QGraphicsScene
{
    Q_OBJECT

public:
    EditorScene(QObject *parent = nullptr);

    void setDrawingMode(bool enabled) { drawingMode = enabled; }
    void setWallDrawingMode(bool enabled) { wallDrawingMode = enabled; }

signals:
    void vertexAdded(QPointF pos);
    void polygonFinished();
    void wallPointAdded(QPointF pos);
    void wallFinished();
    void sectorClicked(int sectorIndex);
    void mouseMoved(QPointF pos);  // <-- Añadir esta línea


protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;  // <-- Añadir esta línea

private:
    bool drawingMode;
    bool wallDrawingMode;
    int wallPointCount;
};

#endif // EDITORSCENE_H
