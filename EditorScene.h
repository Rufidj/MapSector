#ifndef EDITORSCENE_H
#define EDITORSCENE_H

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QObject>

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

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private:
    bool drawingMode;
    bool wallDrawingMode;
    int wallPointCount;
};

#endif // EDITORSCENE_H
