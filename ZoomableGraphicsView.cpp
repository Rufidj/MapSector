#include "ZoomableGraphicsView.h"

ZoomableGraphicsView::ZoomableGraphicsView(QWidget *parent)
    : QGraphicsView(parent), zoomFactor(1.15)
{
    setDragMode(QGraphicsView::ScrollHandDrag);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
}

void ZoomableGraphicsView::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
        // Zoom con Ctrl + rueda
        if (event->angleDelta().y() > 0) {
            scale(zoomFactor, zoomFactor);
        } else {
            scale(1.0 / zoomFactor, 1.0 / zoomFactor);
        }
        event->accept();
    } else {
        // Scroll normal sin Ctrl
        QGraphicsView::wheelEvent(event);
    }
}
