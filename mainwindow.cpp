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
#include <zlib.h>

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
    if (currentMap.regions.size() < 1) {
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
                                                    "Seleccionar archivo .fpg", "", "FPG Files (*.fpg)");

    if (!filename.isEmpty()) {
        if (loadFPGFile(filename)) {
            updateTextureList();
            forceSyncSectorList();
            qDebug() << "Texturas FPG cargadas:" << currentMap.textures.size();
        }
    }
}



void MainWindow::updateSectorList() {
    ui->sectorList->clear();
    for (const ModernRegion &region : currentMap.regions) {
        ui->sectorList->addItem(QString("Sector %1 (Piso: %2, Techo: %3)")
                                    .arg(region.active)
                                    .arg(region.floor_height)
                                    .arg(region.ceiling_height));
    }
}

void MainWindow::updateTextureList() {
    // Limpiar thumbnails existentes
    ui->wallTextureThumb->setIcon(QIcon());
    ui->ceilingTextureThumb->setIcon(QIcon());
    ui->floorTextureThumb->setIcon(QIcon());

    if (currentMap.textures.empty()) {
        ui->texFileLabel->setText("Ningún archivo .fpg cargado");
        return;
    }

    // Mostrar primeras 3 texturas como thumbnails
    int thumbCount = qMin(3, (int)currentMap.textures.size());

    for (int i = 0; i < thumbCount; i++) {
        if (!currentMap.textures[i].pixmap.isNull()) {
            QPixmap thumbnail = currentMap.textures[i].pixmap.scaled(64, 64,
                                                                     Qt::KeepAspectRatio, Qt::SmoothTransformation);

            switch (i) {
            case 0:
                ui->wallTextureThumb->setIcon(QIcon(thumbnail));
                break;
            case 1:
                ui->ceilingTextureThumb->setIcon(QIcon(thumbnail));
                break;
            case 2:
                ui->floorTextureThumb->setIcon(QIcon(thumbnail));
                break;
            }
        }
    }

    ui->texFileLabel->setText(QString("Archivo .fpg: %1 texturas").arg(currentMap.textures.size()));
}


bool MainWindow::loadFPGFile(const QString &filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Error", "No se pudo abrir el archivo .fpg");
        return false;
    }

    // Leer header principal (8 bytes)
    QByteArray headerBytes = file.read(8);
    qDebug() << "Header leído (hex):" << headerBytes.toHex();

    QByteArray uncompressedData;

    // Detectar gzip
    if (headerBytes.startsWith(QByteArray::fromHex("1f8b"))) {
        qDebug() << "Archivo FPG comprimido con gzip detectado";

        // Volver al inicio y leer todo para descompresión
        file.seek(0);
        QByteArray compressedData = file.readAll();

        // Descompresión robusta con buffer dinámico
        z_stream strm;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.avail_in = compressedData.size();
        strm.next_in = (Bytef*)compressedData.data();

        // Inicializar inflate con modo gzip (+16)
        int ret = inflateInit2(&strm, 15 + 16);
        if (ret != Z_OK) {
            qDebug() << "inflateInit2 failed:" << ret;
            QMessageBox::critical(this, "Error", "Fallo al inicializar descompresión gzip");
            return false;
        }

        // Buffer dinámico que crece según necesidad
        QByteArray buffer;
        buffer.resize(4096); // Tamaño inicial

        do {
            strm.avail_out = buffer.size();
            strm.next_out = (Bytef*)buffer.data();

            ret = inflate(&strm, Z_NO_FLUSH);

            if (ret == Z_STREAM_ERROR) {
                qDebug() << "inflate error:" << ret;
                inflateEnd(&strm);
                QMessageBox::critical(this, "Error", "Error durante descompresión gzip");
                return false;
            }

            // Calcular bytes descomprimidos en esta iteración
            size_t have = buffer.size() - strm.avail_out;
            uncompressedData.append(buffer.data(), have);

            // Si necesitamos más espacio, agrandar buffer
            if (strm.avail_out == 0) {
                buffer.resize(buffer.size() * 2);
            }

        } while (ret != Z_STREAM_END);

        inflateEnd(&strm);
        qDebug() << "Descompresión exitosa - Bytes descomprimidos:" << uncompressedData.size();

    } else {
        // Archivo sin comprimir
        file.seek(0);
        uncompressedData = file.readAll();
        qDebug() << "Archivo FPG sin comprimir detectado";
    }

    file.close();

    // Validar magic number del formato FPG (case-insensitive)
    if (uncompressedData.size() < 8) {
        QMessageBox::critical(this, "Error", "Archivo FPG demasiado pequeño");
        return false;
    }

    QString magicOriginal = QString::fromLatin1(uncompressedData.left(7));
    QString magicBase = magicOriginal.left(3).toUpper();

    if (magicBase != "F32") {
        QMessageBox::critical(this, "Error",
                              QString("Formato .fpg inválido - se esperaba 'F32*', se encontró '%1'")
                                  .arg(magicOriginal));
        return false;
    }

    qDebug() << "Magic number original:" << magicOriginal;
    qDebug() << "Magic number validado correctamente";

    // Configurar QDataStream con endianness correcto
    QDataStream in(uncompressedData);
    in.setByteOrder(QDataStream::LittleEndian);  // ¡CRUCIAL! BennuGD2 usa little-endian
    in.setFloatingPointPrecision(QDataStream::SinglePrecision);

    // Saltar header (8 bytes)
    in.skipRawData(8);

    // Procesar chunks del archivo FPG
    int chunkCount = 0;
    currentMap.textures.clear();

    qDebug() << "Iniciando lectura de chunks...";

    while (!in.atEnd()) {
        // Leer estructura FPG_info usando operadores >> que respetan endianness
        FPG_CHUNK chunk;
        in >> chunk.code >> chunk.regsize;
        in.readRawData(chunk.name, 32);
        in.readRawData(chunk.filename, 12);
        in >> chunk.width >> chunk.height >> chunk.flags;

        if (in.status() != QDataStream::Ok) {
            qDebug() << "Error leyendo chunk header";
            break;
        }

        qDebug() << QString("Chunk %1 : código=%2, tamaño=%3x%4, flags=%5")
                        .arg(chunkCount + 1)
                        .arg(chunk.code)
                        .arg(chunk.width)
                        .arg(chunk.height)
                        .arg(chunk.flags);

        // Validar dimensiones del chunk
        if (chunk.width <= 0 || chunk.height <= 0 ||
            chunk.width > 4096 || chunk.height > 4096) {
            qDebug() << "Chunk con dimensiones inválidas, saltando";
            continue;
        }

        // Leer control points si flags > 0
        if (chunk.flags > 0) {
            int numPoints = chunk.flags;
            for (int i = 0; i < numPoints; i++) {
                int16_t x, y;
                in >> x >> y;
            }
        }

        // Calcular tamaño de datos de píxeles según formato
        int pixelDataSize = chunk.width * chunk.height * 4; // 32 bits RGBA

        // Leer datos de píxeles
        char* pixelBuffer = new char[pixelDataSize];
        int bytesRead = in.readRawData(pixelBuffer, pixelDataSize);

        if (bytesRead != pixelDataSize) {
            qDebug() << "Error: no se pudieron leer todos los bytes del chunk";
            delete[] pixelBuffer;
            break;
        }

        // Convertir de BGRA a RGBA manualmente para corregir colores
        for (int i = 0; i < pixelDataSize; i += 4) {
            char temp = pixelBuffer[i];
            pixelBuffer[i] = pixelBuffer[i + 2];   // R = B
            pixelBuffer[i + 2] = temp;             // B = R
            // G y A permanecen igual
        }

        // Crear QImage desde el buffer corregido
        QImage image(reinterpret_cast<const uchar*>(pixelBuffer),
                     chunk.width, chunk.height, QImage::Format_RGBA8888);

        if (!image.isNull()) {
            // Crear QPixmap y añadir a currentMap.textures
            QPixmap pixmap = QPixmap::fromImage(image);
            TextureEntry tex(filename, chunk.code);
            tex.pixmap = pixmap;
            currentMap.textures.append(tex);
            qDebug() << "Textura" << chunk.code << "cargada exitosamente";
        } else {
            qWarning() << "Error al crear QImage para chunk" << chunk.code;
        }

        delete[] pixelBuffer;
        chunkCount++;

        // Límite de seguridad para evitar bucles infinitos
        if (chunkCount > 1000) {
            qDebug() << "Alcanzado límite máximo de chunks";
            break;
        }
    }

    qDebug() << "Procesados" << chunkCount << "chunks";
    qDebug() << "Texturas FPG cargadas:" << currentMap.textures.size();

    updateTextureList();
    updateTextureThumbnails();

    QMessageBox::information(this, "Éxito",
                             QString("Se cargaron %1 texturas desde el archivo FPG")
                                 .arg(currentMap.textures.size()));

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

    // Crear región moderna con los vértices dibujados
    ModernRegion region;
    region.active = 1;
    region.type = 0;
    region.floor_height = ui->floorHeightSpin->value();
    region.ceiling_height = ui->ceilingHeightSpin->value();
    region.floor_tex = 0;
    region.ceil_tex = 0;
    region.fade = 0;

    // Convertir QPointF a ModernPoint
    for (const QPointF &vertex : currentPolygon) {
        region.points.push_back(ModernPoint(static_cast<int32_t>(vertex.x()),
                                            static_cast<int32_t>(vertex.y())));
    }

    currentMap.regions.push_back(region);

    // Eliminar SOLO elementos marcados como temporales
    QList<QGraphicsItem*> items = scene->items();
    for (QGraphicsItem* item : items) {
        if (item->data(0).toString() == "temporary") {
            scene->removeItem(item);
            delete item;
        }
    }

    // Dibujar la región final
    drawRegion(region);

    updateSectorList();
    currentPolygon.clear();

    QMessageBox::information(this, "Éxito",
                             QString("Sector creado con %1 vértices")
                                 .arg(region.points.size()));
}

void MainWindow::drawRegion(const ModernRegion &region) {
    // Dibujar polígono del sector
    QPolygonF polygon;
    for (const ModernPoint &point : region.points) {
        polygon << QPointF(point.x, point.y);
    }
    scene->addPolygon(polygon, QPen(Qt::blue, 2), QBrush(QColor(100, 100, 255, 50)));
}

void MainWindow::drawWall(const ModernWall &wall) {
    if (wall.p1 < currentMap.points.size() && wall.p2 < currentMap.points.size()) {
        const ModernPoint &p1 = currentMap.points[wall.p1];
        const ModernPoint &p2 = currentMap.points[wall.p2];
        scene->addLine(p1.x, p1.y, p2.x, p2.y, QPen(Qt::red, 3));
    }
}

void MainWindow::on_floorHeightSpin_valueChanged(double value) {
    int index = ui->sectorList->currentRow();
    if (index >= 0 && index < currentMap.regions.size()) {
        currentMap.regions[index].floor_height = value;
        updateSectorList();
    }
}

void MainWindow::on_ceilingHeightSpin_valueChanged(double value) {
    int index = ui->sectorList->currentRow();
    if (index >= 0 && index < currentMap.regions.size()) {
        currentMap.regions[index].ceiling_height = value;
        updateSectorList();
    }
}

void MainWindow::on_exportWLDButton_clicked() {
    QString filename = QFileDialog::getSaveFileName(this,
                                                    "Guardar Mapa WLD", "", "WLD Files (*.wld)");

    if (!filename.isEmpty()) {
        if (currentMap.saveToWLD(filename)) {
            QMessageBox::information(this, "Éxito", "Mapa guardado en formato WLD");
        } else {
            QMessageBox::critical(this, "Error", "No se pudo guardar el archivo WLD");
        }
    }
}

void MainWindow::on_importWLDButton_clicked() {
    QString filename = QFileDialog::getOpenFileName(this,
                                                    "Cargar Mapa WLD", "", "WLD Files (*.wld)");
    if (filename.isEmpty()) return;

    if (currentMap.loadFromWLD(filename)) {
        // DIBUJAR LOS DATOS CARGADOS - esto falta
        drawWLDMap();

        QMessageBox::information(this, "Éxito",
                                 QString("Mapa WLD cargado: %1 regiones, %2 paredes")
                                     .arg(currentMap.regions.size())
                                     .arg(currentMap.walls.size()));
    }
}

void MainWindow::on_newMapButton_clicked() {
    currentMap.clear();
    scene->clear();

    // Redibujar grid
    for (int i = 0; i <= 800; i += 50) {
        scene->addLine(i, 0, i, 800, QPen(Qt::lightGray));
        scene->addLine(0, i, 800, i, QPen(Qt::lightGray));
    }

    updateSectorList();
    QMessageBox::information(this, "Nuevo Mapa", "Mapa WLD creado exitosamente");
}

void MainWindow::updateScene() {
    scene->clear();

    // Redibujar grid
    for (int i = 0; i <= 800; i += 50) {
        scene->addLine(i, 0, i, 800, QPen(Qt::lightGray));
        scene->addLine(0, i, 800, i, QPen(Qt::lightGray));
    }

    // Dibujar todas las regiones
    for (const ModernRegion &region : currentMap.regions) {
        drawRegion(region);
    }

    // Dibujar todas las paredes
    for (const ModernWall &wall : currentMap.walls) {
        drawWall(wall);
    }
}

void MainWindow::syncModernMapToUI() {
    ui->sectorList->clear();
    for (size_t i = 0; i < currentMap.regions.size(); i++) {
        const ModernRegion &region = currentMap.regions[i];
        ui->sectorList->addItem(QString("Región %1 (Piso: %2, Techo: %3)")
                                    .arg(i)
                                    .arg(region.floor_height)
                                    .arg(region.ceiling_height));
    }
}

void MainWindow::syncUIToModernMap() {
    // Esta función sincroniza los datos de la UI al mapa moderno
    // Implementación según sea necesario
}

int MainWindow::findOrCreatePoint(const QPointF &pos) {
    // Buscar punto existente
    for (size_t i = 0; i < currentMap.points.size(); i++) {
        if (currentMap.points[i].x == pos.x() && currentMap.points[i].y == pos.y()) {
            return i;
        }
    }

    // Crear nuevo punto
    currentMap.points.emplace_back(pos.x(), pos.y());
    return currentMap.points.size() - 1;
}

void MainWindow::on_sectorList_currentRowChanged(int index) {
    // Validar consistencia entre UI y datos
    if (index >= 0 && index >= currentMap.regions.size()) {
        qDebug() << "Error: Inconsistencia entre UI y datos - UI index:" << index << "regions.size():" << currentMap.regions.size();
        selectedSectorIndex = -1;
        return;
    }

    selectedSectorIndex = index;
    qDebug() << "Sector seleccionado, índice:" << index;

    if (index >= 0 && index < currentMap.regions.size()) {
        const ModernRegion &region = currentMap.regions[index];
        ui->floorHeightSpin->setValue(region.floor_height);
        ui->ceilingHeightSpin->setValue(region.ceiling_height);
        updateTextureThumbnails();
    }
}

void MainWindow::on_editVerticesButton_clicked() {
    int index = ui->sectorList->currentRow();
    if (index < 0 || index >= currentMap.regions.size()) {
        QMessageBox::warning(this, "Error", "Selecciona un sector primero");
        return;
    }

    // Limpiar escena y redibujar solo el sector seleccionado
    scene->clear();

    ModernRegion &region = currentMap.regions[index];

    // Dibujar polígono del sector
    QPolygonF polygon;
    for (const ModernPoint &point : region.points) {
        polygon << QPointF(point.x, point.y);
    }
    scene->addPolygon(polygon, QPen(Qt::blue, 2), QBrush(QColor(100, 100, 255, 50)));

    // Dibujar vértices como VertexItem editables
    for (int i = 0; i < region.points.size(); i++) {
        VertexItem *vertexItem = new VertexItem(index, i);
        vertexItem->setRect(region.points[i].x - 5, region.points[i].y - 5, 10, 10);
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
    if (index < 0 || index >= currentMap.regions.size()) {
        QMessageBox::warning(this, "Error", "Selecciona un sector primero");
        return;
    }

    // Confirmar eliminación
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirmar",
        QString("¿Eliminar sector %1?").arg(index),
        QMessageBox::Yes | QMessageBox::No
        );

    if (reply == QMessageBox::Yes) {
        currentMap.regions.erase(currentMap.regions.begin() + index);

        // Redibujar escena
        scene->clear();

        // Redibujar grid
        for (int i = 0; i <= 800; i += 50) {
            scene->addLine(i, 0, i, 800, QPen(Qt::lightGray));
            scene->addLine(0, i, 800, i, QPen(Qt::lightGray));
        }

        // Redibujar sectores restantes
        for (const ModernRegion &region : currentMap.regions) {
            drawRegion(region);
        }

        updateSectorList();
    }
}

void MainWindow::onSectorMoved(int sectorIndex, QPointF delta) {
    if (sectorIndex >= 0 && sectorIndex < currentMap.regions.size()) {
        // Actualizar todos los vértices del sector en la colección principal
        for (ModernPoint &point : currentMap.regions[sectorIndex].points) {
            point.x += delta.x();
            point.y += delta.y();
        }

        // Actualizar la lista visual
        updateSectorList();
    }
}

void MainWindow::onVertexMoved(int sectorIndex, int vertexIndex, QPointF newPosition) {
    if (sectorIndex >= 0 && sectorIndex < currentMap.regions.size()) {
        if (vertexIndex >= 0 && vertexIndex < currentMap.regions[sectorIndex].points.size()) {
            // Actualizar el vértice en la colección principal
            currentMap.regions[sectorIndex].points[vertexIndex].x = newPosition.x();
            currentMap.regions[sectorIndex].points[vertexIndex].y = newPosition.y();

            // Redibujar el sector con la nueva geometría
            scene->clear();

            // Redibujar polígono actualizado
            QPolygonF polygon;
            for (const ModernPoint &point : currentMap.regions[sectorIndex].points) {
                polygon << QPointF(point.x, point.y);
            }
            scene->addPolygon(polygon, QPen(Qt::blue, 2), QBrush(QColor(100, 100, 255, 50)));

            // Redibujar todos los vértices
            for (int i = 0; i < currentMap.regions[sectorIndex].points.size(); i++) {
                VertexItem *vertexItem = new VertexItem(sectorIndex, i);
                vertexItem->setRect(currentMap.regions[sectorIndex].points[i].x - 5,
                                    currentMap.regions[sectorIndex].points[i].y - 5, 10, 10);
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

    // Crear lista de nombres de regiones para el diálogo
    QStringList regionNames;
    for (const ModernRegion &region : currentMap.regions) {
        regionNames.append(QString("Región %1").arg(region.active));
    }

    // Mostrar diálogo para configurar la pared
    WallDialog dialog(regionNames, this);
    if (dialog.exec() == QDialog::Accepted) {
        // Crear pared con los datos del diálogo
        ModernWall wall;
        wall.front_region = dialog.getSector1Id();
        wall.back_region = dialog.getSector2Id();
        wall.texture = dialog.getTextureId();

        // Encontrar o crear puntos para la pared
        wall.p1 = findOrCreatePoint(currentWallPoints[0]);
        wall.p2 = findOrCreatePoint(currentWallPoints[1]);

        currentMap.walls.push_back(wall);

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
                                 QString("Pared creada conectando regiones %1 y %2")
                                     .arg(wall.front_region).arg(wall.back_region));
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

void MainWindow::on_wallTextureThumb_clicked() {
    if (currentMap.textures.empty()) {
        QMessageBox::information(this, "Información", "No hay texturas cargadas. Por favor, carga un archivo .fpg primero");
        return;
    }

    if (selectedSectorIndex < 0 || selectedSectorIndex >= currentMap.regions.size()) {
        QMessageBox::information(this, "Información", "No hay sector seleccionado. Por favor, selecciona un sector de la lista");
        return;
    }

    TextureSelectorDialog dialog(this);
    dialog.setTextures(currentMap.textures);

    if (dialog.exec() == QDialog::Accepted) {
        int textureId = dialog.selectedTextureId();
        if (textureId >= 0 && textureId < currentMap.textures.size()) {
            currentMap.regions[selectedSectorIndex].wall_tex = textureId;
            updateTextureThumbnails();
            updateSectorList();
        }
    }
}

void MainWindow::on_ceilingTextureThumb_clicked() {
    if (currentMap.textures.empty() || selectedSectorIndex < 0 || selectedSectorIndex >= currentMap.regions.size()) {
        QMessageBox::information(this, "Información",
                                 "Por favor, carga un archivo .fpg y selecciona un sector primero");
        return;
    }

    TextureSelectorDialog dialog(this);
    dialog.setTextures(currentMap.textures);

    if (dialog.exec() == QDialog::Accepted) {
        int textureId = dialog.selectedTextureId();
        if (textureId >= 0 && textureId < currentMap.textures.size()) {
            currentMap.regions[selectedSectorIndex].ceil_tex = textureId;
            updateTextureThumbnails();
            updateSectorList();
        }
    }
}

void MainWindow::on_floorTextureThumb_clicked() {
    if (currentMap.textures.empty() || selectedSectorIndex < 0 || selectedSectorIndex >= currentMap.regions.size()) {
        QMessageBox::information(this, "Información",
                                 "Por favor, carga un archivo .fpg y selecciona un sector primero");
        return;
    }

    TextureSelectorDialog dialog(this);
    dialog.setTextures(currentMap.textures);

    if (dialog.exec() == QDialog::Accepted) {
        int textureId = dialog.selectedTextureId();
        if (textureId >= 0 && textureId < currentMap.textures.size()) {
            currentMap.regions[selectedSectorIndex].floor_tex = textureId;
            updateTextureThumbnails();
            updateSectorList();
        }
    }
}

void MainWindow::forceSyncSectorList() {
    qDebug() << "Sincronización forzada - regions.size():" << currentMap.regions.size();

    // Limpiar completamente la UI
    ui->sectorList->clear();
    selectedSectorIndex = -1;

    // Reconstruir desde el vector real
    for (int i = 0; i < currentMap.regions.size(); i++) {
        const ModernRegion &region = currentMap.regions[i];
        ui->sectorList->addItem(QString("Sector %1 (Piso: %2, Techo: %3)")
                                    .arg(region.active)
                                    .arg(region.floor_height)
                                    .arg(region.ceiling_height));
    }

    qDebug() << "UI reconstruida con" << ui->sectorList->count() << "elementos";
}


void MainWindow::updateTextureThumbnails() {
    if (selectedSectorIndex < 0 || selectedSectorIndex >= currentMap.regions.size()) {
        return;
    }

    const ModernRegion &region = currentMap.regions[selectedSectorIndex];

    // Función auxiliar para encontrar textura por ID
    auto findTextureById = [&](uint32_t id) -> const TextureEntry* {
        for (const TextureEntry &tex : currentMap.textures) {
            if (tex.id == id) {
                return &tex;
            }
        }
        return nullptr;
    };

    // Actualizar thumbnail de pared
    const TextureEntry *wallTex = findTextureById(region.wall_tex);     // wall_tex
    if (wallTex && !wallTex->pixmap.isNull()) {
        QPixmap thumb = wallTex->pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ui->wallTextureThumb->setIcon(QIcon(thumb));
    }

    // Actualizar thumbnail de techo
    const TextureEntry *ceilingTex = findTextureById(region.ceil_tex); // ceil_tex
    if (ceilingTex && !ceilingTex->pixmap.isNull()) {
        QPixmap thumb = ceilingTex->pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ui->ceilingTextureThumb->setIcon(QIcon(thumb));
    }

    // Actualizar thumbnail de suelo
    const TextureEntry *floorTex = findTextureById(region.floor_tex);  // floor_tex
    if (floorTex && !floorTex->pixmap.isNull()) {
        QPixmap thumb = floorTex->pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        ui->floorTextureThumb->setIcon(QIcon(thumb));
    }
}

void MainWindow::drawWLDMap() {
    // Limpiar escena
    scene->clear();

    // Redibujar grid
    for (int i = 0; i <= 800; i += 50) {
        scene->addLine(i, 0, i, 800, QPen(Qt::lightGray));
        scene->addLine(0, i, 800, i, QPen(Qt::lightGray));
    }

    // Dibujar regiones (sectores) WLD
    for (const ModernRegion &region : currentMap.regions) {
        QPolygonF polygon;
        for (const ModernPoint &point : region.points) {
            polygon << QPointF(point.x, point.y);
        }
        scene->addPolygon(polygon, QPen(Qt::blue, 2), QBrush(QColor(100, 100, 255, 50)));
    }

    // Dibujar paredes WLD
    for (const ModernWall &wall : currentMap.walls) {
        if (wall.p1 < currentMap.points.size() && wall.p2 < currentMap.points.size()) {
            const ModernPoint &p1 = currentMap.points[wall.p1];
            const ModernPoint &p2 = currentMap.points[wall.p2];
            scene->addLine(p1.x, p1.y, p2.x, p2.y, QPen(Qt::red, 3));
        }
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}
