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

signals:
    void vertexAdded(QPointF pos);
    void polygonFinished();

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private:
    bool drawingMode;
};

#endif // EDITORSCENE_H
