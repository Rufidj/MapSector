#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include "MapStructures.h"
#include "textureselectordialog.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_addSectorButton_clicked();
    void on_addWallButton_clicked();
    void on_addTextureButton_clicked();
    void on_exportWLDButton_clicked();
    void on_importWLDButton_clicked();
    void on_newMapButton_clicked();
    void on_sectorList_currentRowChanged(int index);
    void on_floorHeightSpin_valueChanged(double value);
    void on_ceilingHeightSpin_valueChanged(double value);
    void onVertexAdded(QPointF pos);
    void onPolygonFinished();
    void on_editVerticesButton_clicked();
    void on_deleteSectorButton_clicked();
    void onSectorMoved(int sectorIndex, QPointF delta);
    void onVertexMoved(int sectorIndex, int vertexIndex, QPointF newPosition);
    void onWallPointAdded(QPointF pos);
    void onWallFinished();
    void on_wallTextureThumb_clicked();
    void on_ceilingTextureThumb_clicked();
    void on_floorTextureThumb_clicked();
    void onSectorClicked(int sectorIndex);  // <-- Añadir esta línea
    void redrawVerticesOnly();
private:
    Ui::MainWindow *ui;
    QGraphicsScene *scene;

    // Reemplazar sectores/paredes individuales con mapa moderno
    ModernMap currentMap;

    QVector<QPointF> currentPolygon;
    QVector<QPointF> currentWallPoints;
    int selectedSectorIndex = -1;

    // COORDENADAS DEL MAPA
    qreal scale = 1.0;
    qreal zoom_level = 0.0625;  // Valor inicial como en divmap3d
    qreal scroll_x = 0;
    qreal scroll_y = 0;

    // Funciones de conversión y sincronización
    void syncModernMapToUI();
    void syncUIToModernMap();
    void updateSectorList();
    void updateTextureList();
    void updateScene();
    void drawRegion(const ModernRegion &region);
    void drawWall(const ModernWall &wall);
    void drawWLDMap(bool adjustView = true);
    void assignRegionsAndPortals();
    void sortRegionsByDepth();                      // <-- Añadir esta línea
    bool wallsShareVertices(const ModernWall &w1, const ModernWall &w2);  // <-- Añadir esta línea
    bool wallsHaveOppositeOrientation(const ModernWall &w1, const ModernWall &w2);  // <-- Añadir esta línea
    bool isPointInRegion(qreal x, qreal y, const std::vector<QPointF> &polygon);
    void onMouseMoved(QPointF pos);  // <-- Añadir esta línea
    void updateSelectionColors();

    // Funciones de archivo
    bool loadFPGFile(const QString &filename);
    bool exportToWLD(const QString &filename);
    bool importFromWLD(const QString &filename);

    // Funciones auxiliares
    void updateMapCenter();
    int findOrCreatePoint(int32_t x, int32_t y);
    void updateTextureThumbnails();
    void forceSyncSectorList();
};



#endif // MAINWINDOW_H
