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
#include "EditorGraphicsItem.h"
#include "VertexItem.h"
#include "WallDialog.h"
#include "ZoomableGraphicsView.h"
#include <QFileInfo>
#include <QPixmap>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // IMPORTANTE: Reemplazar QGraphicsView con ZoomableGraphicsView
    QGraphicsView *oldView = ui->mapView;
    ZoomableGraphicsView *newView = new ZoomableGraphicsView(this);

    // Copiar geometría y propiedades
    newView->setGeometry(oldView->geometry());
    newView->setObjectName("mapView");

    // Reemplazar en el layout del splitter
    QSplitter *splitter = ui->splitter;
    int index = splitter->indexOf(oldView);
    splitter->insertWidget(index, newView);
    splitter->widget(index + 1)->deleteLater(); // Eliminar el viejo

    // Actualizar el puntero en ui
    ui->mapView = newView;

    // Crear EditorScene
    EditorScene *editorScene = new EditorScene(this);
    editorScene->setSceneRect(0, 0, 800, 800);
    scene = editorScene;

    ui->mapView->setScene(scene);
    ui->mapView->setRenderHint(QPainter::Antialiasing);

    // Dibujar grid
    for (int i = 0; i <= 800; i += 50) {
        scene->addLine(i, 0, i, 800, QPen(Qt::lightGray));
        scene->addLine(0, i, 800, i, QPen(Qt::lightGray));
    }

    // Conectar señales
    connect(editorScene, &EditorScene::vertexAdded,
            this, &MainWindow::onVertexAdded);
    connect(editorScene, &EditorScene::polygonFinished,
            this, &MainWindow::onPolygonFinished);
    connect(editorScene, &EditorScene::wallPointAdded,
            this, &MainWindow::onWallPointAdded);
    connect(editorScene, &EditorScene::wallFinished,
            this, &MainWindow::onWallFinished);
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

    EditorScene *editorScene = qobject_cast<EditorScene*>(scene);
    if (editorScene) {
        editorScene->setWallDrawingMode(true);
        currentWallPoints.clear();

        QMessageBox::information(this, "Modo Dibujo de Pared",
                                 "Haz clic en dos puntos para definir la pared.\n"
                                 "Se creará automáticamente después del segundo punto.");
    }
}

void MainWindow::on_addTextureButton_clicked() {
    QString filename = QFileDialog::getOpenFileName(this,
                                                    "Seleccionar archivo .tex", "", "TEX Files (*.tex)");

    if (!filename.isEmpty()) {
        loadTEXFile(filename);
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
    // Limpiar thumbnails existentes
    ui->wallTextureThumb->clear();
    ui->ceilingTextureThumb->clear();
    ui->floorTextureThumb->clear();

    if (textures.isEmpty()) {
        ui->texFileLabel->setText("Ningún archivo .tex cargado");
        updateTextureComboBoxes();
        return;
    }

    // Mostrar primeras 3 texturas como thumbnails
    int thumbCount = qMin(3, textures.size());

    for (int i = 0; i < thumbCount; i++) {
        if (!textures[i].pixmap.isNull()) {
            QPixmap thumbnail = textures[i].pixmap.scaled(48, 48,
                                                          Qt::KeepAspectRatio, Qt::SmoothTransformation);

            switch (i) {
            case 0:
                ui->wallTextureThumb->setPixmap(thumbnail);
                break;
            case 1:
                ui->ceilingTextureThumb->setPixmap(thumbnail);
                break;
            case 2:
                ui->floorTextureThumb->setPixmap(thumbnail);
                break;
            }
        }
    }

    ui->texFileLabel->setText(QString("Archivo .tex: %1 texturas").arg(textures.size()));
    updateTextureComboBoxes();
}

void MainWindow::drawSector(const EditorSector &sector) {
    // Encontrar el índice del sector
    int index = -1;
    for (int i = 0; i < sectors.size(); i++) {
        if (sectors[i].id == sector.id) {
            index = i;
            break;
        }
    }

    if (index == -1) return;

    EditorSectorItem *item = new EditorSectorItem(index, sector);
    scene->addItem(item);

    // Conectar señal para sincronizar datos
    connect(item, &EditorSectorItem::sectorMoved,
            this, &MainWindow::onSectorMoved);
}

void MainWindow::drawWall(const EditorWall &wall) {
    scene->addLine(wall.p1.x(), wall.p1.y(), wall.p2.x(), wall.p2.y(),
                   QPen(Qt::red, 3));
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
        QMessageBox::critical(this, "Error",
                              "No se pudo crear el archivo: " + filename);
        return false;
    }

    QDataStream out(&file);
    out.setByteOrder(QDataStream::LittleEndian);

    // Header
    out.writeRawData("DMAP", 4);
    out << (uint32_t)1;  // version
    out << (uint32_t)sectors.size();
    out << (uint32_t)walls.size();
    out << (uint32_t)textures.size();

    // CORRECCIÓN: Exportar texturas en el orden que espera el shader
    // Orden del shader: 0=paredes, 1=piso, 2=techo
    // Necesitamos reorganizar las texturas según este orden

    // Crear un mapa temporal para reorganizar texturas
    QVector<TextureEntry> reorderedTextures;

    // Para cada sector, necesitamos asegurarnos de que sus texturas
    // estén en el orden correcto en el archivo DMAP
    for (const EditorSector &sector : sectors) {
        // Añadir textura de paredes (índice 0)
        if (sector.wall_texture_id < textures.size()) {
            reorderedTextures.append(textures[sector.wall_texture_id]);
        }

        // Añadir textura de piso (índice 1)
        if (sector.floor_texture_id < textures.size()) {
            reorderedTextures.append(textures[sector.floor_texture_id]);
        }

        // Añadir textura de techo (índice 2)
        if (sector.ceiling_texture_id < textures.size()) {
            reorderedTextures.append(textures[sector.ceiling_texture_id]);
        }
    }

    // Exportar texturas reorganizadas
    for (const TextureEntry &tex : reorderedTextures) {
        char buffer[256] = {0};
        // Usar solo el nombre del archivo, no la ruta completa
        QString relativePath = QFileInfo(tex.filename).fileName();
        strncpy(buffer, relativePath.toUtf8().constData(), 255);
        out.writeRawData(buffer, 256);
    }

    // Sectores
    for (const EditorSector &sector : sectors) {
        out << sector.id;
        out << sector.floor_height;
        out << sector.ceiling_height;

        // IMPORTANTE: Los IDs de textura ahora son 0, 1, 2 en orden
        out << (uint32_t)0;  // wall_texture_id (índice 0 en DMAP)
        out << (uint32_t)1;  // floor_texture_id (índice 1 en DMAP)
        out << (uint32_t)2;  // ceiling_texture_id (índice 2 en DMAP)

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
                                 .arg(sectors.size()).arg(walls.size()).arg(reorderedTextures.size()));

    return true;
}

void MainWindow::onVertexAdded(QPointF pos) {
    currentPolygon.append(pos);

    // Dibujar punto temporal y marcarlo como "temporary"
    QGraphicsEllipseItem* ellipse = scene->addEllipse(pos.x() - 3, pos.y() - 3, 6, 6,
                                                      QPen(Qt::red), QBrush(Qt::red));
    ellipse->setData(0, "temporary"); // Marcar como temporal

    // Si hay más de un vértice, dibujar línea al anterior
    if (currentPolygon.size() > 1) {
        QPointF prev = currentPolygon[currentPolygon.size() - 2];
        QGraphicsLineItem* line = scene->addLine(prev.x(), prev.y(), pos.x(), pos.y(),
                                                 QPen(Qt::red, 2));
        line->setData(0, "temporary"); // Marcar como temporal
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

    // Eliminar SOLO elementos marcados como temporales
    QList<QGraphicsItem*> items = scene->items();
    for (QGraphicsItem* item : items) {
        if (item->data(0).toString() == "temporary") {
            scene->removeItem(item);
            delete item;
        }
    }

    // Dibujar el sector final (azul)
    drawSector(sector);

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

    // Limpiar escena y redibujar solo el sector seleccionado
    scene->clear();

    EditorSector &sector = sectors[index];

    // Dibujar polígono del sector
    QPolygonF polygon;
    for (const QPointF &vertex : sector.vertices) {
        polygon << vertex;
    }
    scene->addPolygon(polygon, QPen(Qt::blue, 2), QBrush(QColor(100, 100, 255, 50)));

    // Dibujar vértices como VertexItem editables
    for (int i = 0; i < sector.vertices.size(); i++) {
        VertexItem *vertexItem = new VertexItem(index, i);
        vertexItem->setRect(sector.vertices[i].x() - 5, sector.vertices[i].y() - 5, 10, 10);
        vertexItem->setPen(QPen(Qt::red, 2));
        vertexItem->setBrush(QBrush(Qt::red));

        scene->addItem(vertexItem);

        // Conectar señal para actualizar datos
        connect(vertexItem, &VertexItem::vertexMoved,
                this, &MainWindow::onVertexMoved);
    }
}

void MainWindow::on_deleteSectorButton_clicked() {
    int index = ui->sectorList->currentRow();
    if (index < 0 || index >= sectors.size()) {
        QMessageBox::warning(this, "Error", "Selecciona un sector primero");
        return;
    }

    // Confirmar eliminación
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirmar",
        QString("¿Eliminar sector %1?").arg(sectors[index].id),
        QMessageBox::Yes | QMessageBox::No
        );

    if (reply == QMessageBox::Yes) {
        sectors.removeAt(index);

        // Redibujar escena
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
}

void MainWindow::updateTextureComboBoxes() {
    // Limpiar comboboxes
    ui->floorTextureCombo->clear();
    ui->ceilingTextureCombo->clear();
    ui->wallTextureCombo->clear();

    // Configurar tamaño de iconos
    ui->floorTextureCombo->setIconSize(QSize(32, 32));
    ui->ceilingTextureCombo->setIconSize(QSize(32, 32));
    ui->wallTextureCombo->setIconSize(QSize(32, 32));

    // Añadir opción "Sin textura"
    ui->floorTextureCombo->addItem("Sin textura", 0);
    ui->ceilingTextureCombo->addItem("Sin textura", 0);
    ui->wallTextureCombo->addItem("Sin textura", 0);

    // Añadir texturas disponibles con thumbnails
    for (const TextureEntry &tex : textures) {
        QPixmap pixmap(tex.filename);
        QIcon icon;

        if (!pixmap.isNull()) {
            // Crear thumbnail pequeño para combobox
            QPixmap thumbnail = pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            icon = QIcon(thumbnail);
        }

        QString displayName = QString("%1: %2").arg(tex.id).arg(QFileInfo(tex.filename).fileName());
        ui->floorTextureCombo->addItem(icon, displayName, tex.id);
        ui->ceilingTextureCombo->addItem(icon, displayName, tex.id);
        ui->wallTextureCombo->addItem(icon, displayName, tex.id);
    }
}

void MainWindow::on_sectorList_currentRowChanged(int index) {
    selectedSectorIndex = index;

    if (index >= 0 && index < sectors.size()) {
        const EditorSector &sector = sectors[index];

        // Actualizar spinboxes de altura
        ui->floorHeightSpin->setValue(sector.floor_height);
        ui->ceilingHeightSpin->setValue(sector.ceiling_height);

        // Actualizar comboboxes de texturas
        // Buscar el índice en el combobox que corresponde al texture_id
        for (int i = 0; i < ui->floorTextureCombo->count(); i++) {
            if (ui->floorTextureCombo->itemData(i).toInt() == (int)sector.floor_texture_id) {
                ui->floorTextureCombo->setCurrentIndex(i);
                break;
            }
        }

        for (int i = 0; i < ui->ceilingTextureCombo->count(); i++) {
            if (ui->ceilingTextureCombo->itemData(i).toInt() == (int)sector.ceiling_texture_id) {
                ui->ceilingTextureCombo->setCurrentIndex(i);
                break;
            }
        }

        for (int i = 0; i < ui->wallTextureCombo->count(); i++) {
            if (ui->wallTextureCombo->itemData(i).toInt() == (int)sector.wall_texture_id) {
                ui->wallTextureCombo->setCurrentIndex(i);
                break;
            }
        }
    }
}

void MainWindow::on_floorTextureCombo_currentIndexChanged(int index) {
    if (selectedSectorIndex >= 0 && selectedSectorIndex < sectors.size()) {
        int textureId = ui->floorTextureCombo->currentData().toInt();
        if (textureId < 0) textureId = 0;
        sectors[selectedSectorIndex].floor_texture_id = textureId;
    }
}

void MainWindow::on_ceilingTextureCombo_currentIndexChanged(int index) {
    if (selectedSectorIndex >= 0 && selectedSectorIndex < sectors.size()) {
        int textureId = ui->ceilingTextureCombo->currentData().toInt();
        if (textureId < 0) textureId = 0;
        sectors[selectedSectorIndex].ceiling_texture_id = textureId;
    }
}

void MainWindow::on_wallTextureCombo_currentIndexChanged(int index) {
    if (selectedSectorIndex >= 0 && selectedSectorIndex < sectors.size()) {
        int textureId = ui->wallTextureCombo->currentData().toInt();
        if (textureId < 0) textureId = 0;
        sectors[selectedSectorIndex].wall_texture_id = textureId;
    }
}

void MainWindow::onSectorMoved(int sectorIndex, QPointF delta) {
    if (sectorIndex >= 0 && sectorIndex < sectors.size()) {
        // Actualizar todos los vértices del sector en la colección principal
        for (QPointF &vertex : sectors[sectorIndex].vertices) {
            vertex += delta;
        }

        // Actualizar la lista visual
        updateSectorList();
    }
}

void MainWindow::onVertexMoved(int sectorIndex, int vertexIndex, QPointF newPosition) {
    if (sectorIndex >= 0 && sectorIndex < sectors.size()) {
        if (vertexIndex >= 0 && vertexIndex < sectors[sectorIndex].vertices.size()) {
            // Actualizar el vértice en la colección principal
            sectors[sectorIndex].vertices[vertexIndex] = newPosition;

            // Redibujar el sector con la nueva geometría
            scene->clear();

            // Redibujar polígono actualizado
            QPolygonF polygon;
            for (const QPointF &vertex : sectors[sectorIndex].vertices) {
                polygon << vertex;
            }
            scene->addPolygon(polygon, QPen(Qt::blue, 2), QBrush(QColor(100, 100, 255, 50)));

            // Redibujar todos los vértices
            for (int i = 0; i < sectors[sectorIndex].vertices.size(); i++) {
                VertexItem *vertexItem = new VertexItem(sectorIndex, i);
                vertexItem->setRect(sectors[sectorIndex].vertices[i].x() - 5,
                                    sectors[sectorIndex].vertices[i].y() - 5, 10, 10);
                vertexItem->setPen(QPen(Qt::red, 2));
                vertexItem->setBrush(QBrush(Qt::red));

                scene->addItem(vertexItem);
                connect(vertexItem, &VertexItem::vertexMoved,
                        this, &MainWindow::onVertexMoved);
            }

            updateSectorList();
        }
    }
}

void MainWindow::onWallPointAdded(QPointF pos) {
    currentWallPoints.append(pos);

    // Dibujar punto temporal
    QGraphicsEllipseItem* ellipse = scene->addEllipse(pos.x() - 3, pos.y() - 3, 6, 6,
                                                      QPen(Qt::green), QBrush(Qt::green));
    ellipse->setData(0, "temporary");

    // Si hay 2 puntos, dibujar línea temporal
    if (currentWallPoints.size() == 2) {
        QPointF p1 = currentWallPoints[0];
        QPointF p2 = currentWallPoints[1];
        QGraphicsLineItem* line = scene->addLine(p1.x(), p1.y(), p2.x(), p2.y(),
                                                 QPen(Qt::green, 2));
        line->setData(0, "temporary");
    }
}

void MainWindow::onWallFinished() {
    if (currentWallPoints.size() != 2) {
        QMessageBox::warning(this, "Error", "Se necesitan exactamente 2 puntos para crear una pared");
        currentWallPoints.clear();
        return;
    }

    // Crear lista de nombres de sectores para el diálogo
    QStringList sectorNames;
    for (const EditorSector &sector : sectors) {
        sectorNames.append(QString("Sector %1").arg(sector.id));
    }

    // Mostrar diálogo para configurar la pared
    WallDialog dialog(sectorNames, this);
    if (dialog.exec() == QDialog::Accepted) {
        // Crear pared con los datos del diálogo
        EditorWall wall;
        wall.sector1_id = dialog.getSector1Id();
        wall.sector2_id = dialog.getSector2Id();
        wall.texture_id = dialog.getTextureId();
        wall.p1 = currentWallPoints[0];
        wall.p2 = currentWallPoints[1];

        walls.append(wall);

        // Eliminar elementos temporales
        QList<QGraphicsItem*> items = scene->items();
        for (QGraphicsItem* item : items) {
            if (item->data(0).toString() == "temporary") {
                scene->removeItem(item);
                delete item;
            }
        }

        // Dibujar la pared final
        drawWall(wall);

        QMessageBox::information(this, "Éxito",
                                 QString("Pared creada conectando sectores %1 y %2")
                                     .arg(wall.sector1_id).arg(wall.sector2_id));
    } else {
        // Usuario canceló - eliminar elementos temporales
        QList<QGraphicsItem*> items = scene->items();
        for (QGraphicsItem* item : items) {
            if (item->data(0).toString() == "temporary") {
                scene->removeItem(item);
                delete item;
            }
        }
    }

    currentWallPoints.clear();
}

bool MainWindow::importFromDMAP(const QString &filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Error", "No se pudo abrir el archivo");
        return false;
    }

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    // Leer header
    char magic[5] = {0};
    in.readRawData(magic, 4);
    if (QString(magic) != "DMAP") {
        QMessageBox::critical(this, "Error", "Formato de archivo inválido");
        return false;
    }

    uint32_t version, sectorCount, wallCount, textureCount;
    in >> version >> sectorCount >> wallCount >> textureCount;

    // Limpiar datos existentes
    sectors.clear();
    walls.clear();
    textures.clear();

    // Leer texturas
    for (uint32_t i = 0; i < textureCount; i++) {
        char buffer[256];
        in.readRawData(buffer, 256);
        TextureEntry tex(QString::fromUtf8(buffer), i);
        textures.append(tex);
    }

    // Leer sectores
    for (uint32_t i = 0; i < sectorCount; i++) {
        EditorSector sector;
        uint32_t vertexCount;
        in >> sector.id >> sector.floor_height >> sector.ceiling_height
            >> sector.floor_texture_id >> sector.ceiling_texture_id
            >> sector.wall_texture_id >> vertexCount;

        for (uint32_t j = 0; j < vertexCount; j++) {
            float x, y;
            in >> x >> y;
            sector.vertices.append(QPointF(x, y));
        }
        sectors.append(sector);
    }

    // Leer paredes
    for (uint32_t i = 0; i < wallCount; i++) {
        EditorWall wall;
        float x1, y1, x2, y2;
        in >> wall.sector1_id >> wall.sector2_id >> wall.texture_id
            >> x1 >> y1 >> x2 >> y2;
        wall.p1 = QPointF(x1, y1);
        wall.p2 = QPointF(x2, y2);
        walls.append(wall);
    }

    file.close();

    // Redibujar todo
    scene->clear();

    // Redibujar grid
    for (int i = 0; i <= 800; i += 50) {
        scene->addLine(i, 0, i, 800, QPen(Qt::lightGray));
        scene->addLine(0, i, 800, i, QPen(Qt::lightGray));
    }

    // Redibujar sectores
    for (const EditorSector &s : sectors) {
        drawSector(s);
    }

    // Redibujar paredes
    for (const EditorWall &w : walls) {
        drawWall(w);
    }

    updateSectorList();
    updateTextureList();

    QMessageBox::information(this, "Éxito",
                             QString("Mapa importado: %1 sectores, %2 paredes, %3 texturas")
                                 .arg(sectors.size()).arg(walls.size()).arg(textures.size()));

    return true;
}

void MainWindow::on_importButton_clicked() {
    QString filename = QFileDialog::getOpenFileName(
        this, "Abrir Mapa DMAP", "", "DMAP Files (*.dmap)"
        );

    if (!filename.isEmpty()) {
        importFromDMAP(filename);
    }
}

bool MainWindow::loadTEXFile(const QString &filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Error", "No se pudo abrir el archivo .tex");
        return false;
    }

    QDataStream in(&file);
    in.setByteOrder(QDataStream::LittleEndian);

    // Leer cabecera TEX_HEADER
    TEX_HEADER header;
    in.readRawData(reinterpret_cast<char*>(header.magic), 4);
    in >> header.version >> header.num_images;
    in.readRawData(reinterpret_cast<char*>(header.reserved), 6);

    // Validar magic number
    if (strncmp(header.magic, "TEX", 3) != 0) {
        QMessageBox::critical(this, "Error", "Formato .tex inválido");
        return false;
    }

    // Validar número de imágenes
    if (header.num_images == 0 || header.num_images > 1000) {
        QMessageBox::critical(this, "Error", "Número de imágenes inválido");
        return false;
    }

    // Limpiar texturas existentes
    textures.clear();

    // Leer cada entrada TEX_ENTRY y cargar datos RGB
    for (uint16_t i = 0; i < header.num_images; i++) {
        TEX_ENTRY entry;
        in >> entry.index >> entry.width >> entry.height >> entry.format;
        in.readRawData(reinterpret_cast<char*>(entry.reserved), 250);

        // Validar dimensiones
        if (entry.width == 0 || entry.height == 0 ||
            entry.width > 4096 || entry.height > 4096) {
            qWarning() << "Dimensiones inválidas para textura" << entry.index;
            continue;
        }

        // Calcular tamaño según formato
        int dataSize;
        QImage::Format format;

        switch (entry.format) {
        case 0: // RGB
            dataSize = entry.width * entry.height * 3;
            format = QImage::Format_RGB888;
            break;
        case 1: // RGBA
            dataSize = entry.width * entry.height * 4;
            format = QImage::Format_RGBA8888;
            break;
        default:
            qWarning() << "Formato no soportado:" << entry.format;
            continue;
        }

        // Validar tamaño de datos
        if (dataSize <= 0 || dataSize > 16*1024*1024) { // Max 16MB por textura
            qWarning() << "Tamaño de datos inválido:" << dataSize;
            continue;
        }

        // Verificar que quedan suficientes bytes en el archivo
        qint64 remaining = file.size() - file.pos();
        if (remaining < dataSize) {
            qWarning() << "Archivo truncado, faltan" << dataSize - remaining << "bytes";
            break;
        }

        // Leer datos de imagen usando buffer temporal
        char* rgbBuffer = new char[dataSize];
        qint64 bytesRead = in.readRawData(rgbBuffer, dataSize);

        if (bytesRead != dataSize) {
            delete[] rgbBuffer;
            qWarning() << "Error: no se pudieron leer todos los bytes de la imagen";
            continue;
        }

        // Crear QImage desde el buffer
        QImage image(reinterpret_cast<const uchar*>(rgbBuffer),
                     entry.width, entry.height, format);

        if (image.isNull()) {
            delete[] rgbBuffer;
            qWarning() << "Error al crear QImage para textura" << entry.index;
            continue;
        }

        // Crear QPixmap y añadir a la lista
        QPixmap pixmap = QPixmap::fromImage(image);
        TextureEntry tex(filename, entry.index);
        tex.pixmap = pixmap;
        textures.append(tex);

        delete[] rgbBuffer;  // Liberar memoria
    }

    file.close();

    // Actualizar UI
    updateTextureList();

    QMessageBox::information(this, "Éxito",
                             QString("Cargadas %1 texturas de %2").arg(textures.size()).arg(header.num_images));

    return true;
}

MainWindow::~MainWindow()
{
    delete ui;
}

