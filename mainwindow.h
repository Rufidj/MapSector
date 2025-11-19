#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include "MapStructures.h"

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
    // ESTAS DECLARACIONES SON NECESARIAS
    void on_addSectorButton_clicked();
    void on_addWallButton_clicked();
    void on_addTextureButton_clicked();
    void on_exportButton_clicked();
    void on_sectorList_currentRowChanged(int index);
    void on_floorHeightSpin_valueChanged(double value);
    void on_ceilingHeightSpin_valueChanged(double value);
    void onVertexAdded(QPointF pos);
    void onPolygonFinished();
    void on_editVerticesButton_clicked();
    void on_deleteSectorButton_clicked();
private:
    Ui::MainWindow *ui;
    QGraphicsScene *scene;

    QVector<EditorSector> sectors;
    QVector<EditorWall> walls;
    QVector<TextureEntry> textures;

    bool drawingMode;
    QVector<QPointF> currentPolygon;
    QGraphicsPolygonItem *previewPolygon;

    EditorSector* selectedSector;
    int selectedSectorIndex;


    void updateSectorList();
    void updateTextureList();
    void drawSector(const EditorSector &sector);
    void drawWall(const EditorWall &wall);
    bool exportToDMAP(const QString &filename);
};

#endif // MAINWINDOW_H
