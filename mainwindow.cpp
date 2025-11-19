#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "MapStructures.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QGraphicsPolygonItem>
#include <QGraphicsLineItem>
#include <QDataStream>
#include <QFile>
#include "EditorScene.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Crear EditorScene en lugar de QGraphicsScene
    EditorScene *editorScene = new EditorScene(this);
    editorScene->setSceneRect(0, 0, 800, 800);
    scene = editorScene;  // Guardar referencia en la variable miembro

    ui->mapView->setScene(scene);
    ui->mapView->setRenderHint(QPainter::Antialiasing);

    // Dibujar grid
    for (int i = 0; i <= 800; i += 50) {
        scene->addLine(i, 0, i, 800, QPen(Qt::lightGray));
        scene->addLine(0, i, 800, i, QPen(Qt::lightGray));
    }

    // IMPORTANTE: Conectar señales de EditorScene
    connect(editorScene, &EditorScene::vertexAdded,
            this, &MainWindow::onVertexAdded);
    connect(editorScene, &EditorScene::polygonFinished,
            this, &MainWindow::onPolygonFinished);
}
void MainWindow::on_addSectorButton_clicked() {
    EditorScene *editorScene = qobject_cast<EditorScene*>(scene);
    if (editorScene) {
        editorScene->setDrawingMode(true);
        currentPolygon.clear();

        QMessageBox::information(this, "Modo Dibujo",
                                 "Haz clic en el mapa para colocar vértices.\n"
                                 "Haz clic derecho para finalizar el sector.");
    }
}

void MainWindow::on_addWallButton_clicked() {
    if (sectors.size() < 1) {
        QMessageBox::warning(this, "Advertencia", "Necesitas al menos un sector primero");
        return;
    }

    EditorWall wall;
    wall.sector1_id = 0;
    wall.sector2_id = 0;  // Pared sólida
    wall.texture_id = 0;
    wall.p1 = QPointF(100, 100);
    wall.p2 = QPointF(200, 100);

    walls.append(wall);
    drawWall(wall);
}

void MainWindow::on_addTextureButton_clicked() {
    QString filename = QFileDialog::getOpenFileName(this,
                                                    "Seleccionar Textura", "", "Images (*.png *.jpg *.bmp)");

    if (!filename.isEmpty()) {
        TextureEntry tex(filename, textures.size());
        textures.append(tex);
        updateTextureList();
    }
}

void MainWindow::on_exportButton_clicked() {
    QString filename = QFileDialog::getSaveFileName(this,
                                                    "Guardar Mapa DMAP", "", "DMAP Files (*.dmap)");

    if (!filename.isEmpty()) {
        exportToDMAP(filename);
    }
}

void MainWindow::updateSectorList() {
    ui->sectorList->clear();
    for (const EditorSector &sector : sectors) {
        ui->sectorList->addItem(QString("Sector %1 (Piso: %2, Techo: %3)")
                                    .arg(sector.id)
                                    .arg(sector.floor_height)
                                    .arg(sector.ceiling_height));
    }
}

void MainWindow::updateTextureList() {
    ui->textureList->clear();
    for (const TextureEntry &tex : textures) {
        ui->textureList->addItem(QString("%1: %2").arg(tex.id).arg(tex.filename));
    }
}

void MainWindow::drawSector(const EditorSector &sector) {
    QPolygonF polygon;
    for (const QPointF &vertex : sector.vertices) {
        polygon << vertex;
    }

    QGraphicsPolygonItem *item = scene->addPolygon(polygon,
                                                   QPen(Qt::blue, 2), QBrush(QColor(100, 100, 255, 50)));

    // Hacer el item seleccionable
    item->setFlag(QGraphicsItem::ItemIsSelectable, true);
    item->setFlag(QGraphicsItem::ItemIsMovable, true);

    // Guardar referencia al sector en el item
    item->setData(0, sector.id);
}

void MainWindow::drawWall(const EditorWall &wall) {
    scene->addLine(wall.p1.x(), wall.p1.y(), wall.p2.x(), wall.p2.y(),
                   QPen(Qt::red, 3));
}

void MainWindow::on_sectorList_currentRowChanged(int index) {
    if (index >= 0 && index < sectors.size()) {
        ui->floorHeightSpin->setValue(sectors[index].floor_height);
        ui->ceilingHeightSpin->setValue(sectors[index].ceiling_height);
    }
}

void MainWindow::on_floorHeightSpin_valueChanged(double value) {
    int index = ui->sectorList->currentRow();
    if (index >= 0 && index < sectors.size()) {
        sectors[index].floor_height = value;
        updateSectorList();
    }
}

void MainWindow::on_ceilingHeightSpin_valueChanged(double value) {
    int index = ui->sectorList->currentRow();
    if (index >= 0 && index < sectors.size()) {
        sectors[index].ceiling_height = value;
        updateSectorList();
    }
}

bool MainWindow::exportToDMAP(const QString &filename) {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, "Error", "No se pudo crear el archivo");
        return false;
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    // Header DMAP
    out.writeRawData("DMAP", 4);
    out << (uint32_t)1;  // version
    out << (uint32_t)sectors.size();
    out << (uint32_t)walls.size();
    out << (uint32_t)textures.size();

    // Tabla de texturas (256 bytes por entrada)
    for (const TextureEntry &tex : textures) {
        char buffer[256] = {0};
        QByteArray bytes = tex.filename.toUtf8();
        strncpy(buffer, bytes.constData(), 255);
        out.writeRawData(buffer, 256);
    }

    // Sectores
    for (const EditorSector &sector : sectors) {
        out << sector.id;
        out << sector.floor_height;
        out << sector.ceiling_height;
        out << sector.floor_texture_id;
        out << sector.ceiling_texture_id;
        out << sector.wall_texture_id;
        out << (uint32_t)sector.vertices.size();

        for (const QPointF &vertex : sector.vertices) {
            out << (float)vertex.x();
            out << (float)vertex.y();
        }
    }

    // Paredes
    for (const EditorWall &wall : walls) {
        out << wall.sector1_id;
        out << wall.sector2_id;
        out << wall.texture_id;
        out << (float)wall.p1.x();
        out << (float)wall.p1.y();
        out << (float)wall.p2.x();
        out << (float)wall.p2.y();
    }

    file.close();

    QMessageBox::information(this, "Éxito",
                             QString("Mapa exportado: %1 sectores, %2 paredes, %3 texturas")
                                 .arg(sectors.size()).arg(walls.size()).arg(textures.size()));

    return true;
}

void MainWindow::onVertexAdded(QPointF pos) {
    currentPolygon.append(pos);

    // Dibujar punto temporal
    scene->addEllipse(pos.x() - 3, pos.y() - 3, 6, 6,
                      QPen(Qt::red), QBrush(Qt::red));

    // Si hay más de un vértice, dibujar línea al anterior
    if (currentPolygon.size() > 1) {
        QPointF prev = currentPolygon[currentPolygon.size() - 2];
        scene->addLine(prev.x(), prev.y(), pos.x(), pos.y(),
                       QPen(Qt::red, 2));
    }
}

void MainWindow::onPolygonFinished() {
    if (currentPolygon.size() < 3) {
        QMessageBox::warning(this, "Error",
                             "Necesitas al menos 3 vértices para crear un sector");
        currentPolygon.clear();
        return;
    }

    // Crear sector con los vértices dibujados
    EditorSector sector;
    sector.id = sectors.size();
    sector.floor_height = ui->floorHeightSpin->value();
    sector.ceiling_height = ui->ceilingHeightSpin->value();
    sector.vertices = currentPolygon;

    sectors.append(sector);

    // Limpiar visualización temporal y dibujar sector final
    scene->clear();

    // Redibujar grid
    for (int i = 0; i <= 800; i += 50) {
        scene->addLine(i, 0, i, 800, QPen(Qt::lightGray));
        scene->addLine(0, i, 800, i, QPen(Qt::lightGray));
    }

    // Redibujar todos los sectores
    for (const EditorSector &s : sectors) {
        drawSector(s);
    }

    updateSectorList();
    currentPolygon.clear();

    QMessageBox::information(this, "Éxito",
                             QString("Sector %1 creado con %2 vértices")
                                 .arg(sector.id).arg(sector.vertices.size()));
}

void MainWindow::on_editVerticesButton_clicked() {
    int index = ui->sectorList->currentRow();
    if (index < 0 || index >= sectors.size()) {
        QMessageBox::warning(this, "Error", "Selecciona un sector primero");
        return;
    }

    // Limpiar escena y redibujar solo el sector seleccionado con vértices editables
    scene->clear();

    EditorSector &sector = sectors[index];

    // Dibujar polígono del sector
    QPolygonF polygon;
    for (const QPointF &vertex : sector.vertices) {
        polygon << vertex;
    }
    scene->addPolygon(polygon, QPen(Qt::blue, 2), QBrush(QColor(100, 100, 255, 50)));

    // Dibujar vértices como círculos editables
    for (int i = 0; i < sector.vertices.size(); i++) {
        QGraphicsEllipseItem *vertexItem = scene->addEllipse(
            sector.vertices[i].x() - 5, sector.vertices[i].y() - 5, 10, 10,
            QPen(Qt::red, 2), QBrush(Qt::red));

        vertexItem->setFlag(QGraphicsItem::ItemIsMovable, true);
        vertexItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
        vertexItem->setData(0, i); // Guardar índice del vértice
    }
}

void MainWindow::on_deleteSectorButton_clicked() {
    int index = ui->sectorList->currentRow();
    if (index < 0 || index >= sectors.size()) {
        QMessageBox::warning(this, "Error", "Selecciona un sector primero");
        return;
    }

    // Eliminar sector
    sectors.removeAt(index);

    // Redibujar todo
    scene->clear();

    // Redibujar grid
    for (int i = 0; i <= 800; i += 50) {
        scene->addLine(i, 0, i, 800, QPen(Qt::lightGray));
        scene->addLine(0, i, 800, i, QPen(Qt::lightGray));
    }

    // Redibujar sectores restantes
    for (const EditorSector &s : sectors) {
        drawSector(s);
    }

    updateSectorList();
}

MainWindow::~MainWindow()
{
    delete ui;
}

