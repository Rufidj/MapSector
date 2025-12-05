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

    // Añadir conexión para coordenadas del mouse
    connect(editorScene, &EditorScene::mouseMoved,
            this, &MainWindow::onMouseMoved);
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
    for (int i = 0; i < currentMap.regions.size(); i++) {
        const ModernRegion &region = currentMap.regions[i];
        ui->sectorList->addItem(QString("Sector %1 (Piso: %2, Techo: %3)")
                                    .arg(i)  // <-- Usar índice i en lugar de region.active
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

    // Crear región moderna
    ModernRegion region;
    region.active = 1;
    region.type = 0;
    region.floor_height = ui->floorHeightSpin->value();
    region.ceiling_height = ui->ceilingHeightSpin->value();
    region.floor_tex = 0;
    region.ceil_tex = 0;
    region.fade = 0;

    // Convertir vértices y añadir a currentMap.points
    std::vector<int> pointIndices;
    for (const QPointF &vertex : currentPolygon) {
        int pointIndex = findOrCreatePoint(vertex);
        pointIndices.push_back(pointIndex);
    }

    // Crear paredes para el sector
    int sectorIndex = currentMap.regions.size();
    for (size_t i = 0; i < pointIndices.size(); i++) {
        ModernWall wall;
        wall.active = 1;
        wall.type = 2; // Pared normal
        wall.p1 = pointIndices[i];
        wall.p2 = pointIndices[(i + 1) % pointIndices.size()];
        wall.front_region = sectorIndex;
        wall.back_region = -1;
        wall.texture = 0;
        wall.texture_top = 0;
        wall.texture_bot = 0;
        wall.fade = 0;

        currentMap.walls.push_back(wall);
    }

    // Guardar vértices también en region.points para compatibilidad
    for (const QPointF &vertex : currentPolygon) {
        region.points.push_back(ModernPoint(static_cast<int32_t>(vertex.x()),
                                            static_cast<int32_t>(vertex.y())));
    }

    currentMap.regions.push_back(region);

    // Limpiar elementos temporales y dibujar
    QList<QGraphicsItem*> items = scene->items();
    for (QGraphicsItem* item : items) {
        if (item->data(0).toString() == "temporary") {
            scene->removeItem(item);
            delete item;
        }
    }

    drawRegion(region);
    updateSectorList();
    currentPolygon.clear();

    QMessageBox::information(this, "Éxito",
                             QString("Sector creado con %1 vértices")
                                 .arg(region.points.size()));
}

void MainWindow::onMouseMoved(QPointF pos) {
    if (currentMap.points.empty() || currentMap.walls.empty()) {
        ui->coordXValue->setText("????");
        ui->coordYValue->setText("????");
        return;
    }

    // Usar la fórmula exacta de divmap3d (líneas 1910-1911)
    qreal mapX = pos.x() / zoom_level + scroll_x;
    qreal mapY = pos.y() / zoom_level + scroll_y;

    // Aplicar límites de FIN_GRID como en divmap3d
    const int FIN_GRID = 32768 - 2560; // 30208

    if (mapX < 0) mapX = 0;
    if (mapY < 0) mapY = 0;
    if (mapX > FIN_GRID) mapX = FIN_GRID;
    if (mapY > FIN_GRID) mapY = FIN_GRID;

    ui->coordXValue->setText(QString("%1").arg((int)mapX, 4, 10, QChar('0')));
    ui->coordYValue->setText(QString("%1").arg((int)mapY, 4, 10, QChar('0')));
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
        drawWLDMap(true);  // Ajustar vista al cargar nuevo mapa
        updateSectorList();

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

        // Usar el sistema de dibujo correcto según el tipo de mapa
        if (currentMap.walls.empty()) {
            // Mapa nuevo creado en editor - usar updateScene()
            updateScene();
        } else {
            // Mapa WLD cargado - usar drawWLDMap()
            drawWLDMap(false);
        }
    }
}

void MainWindow::on_editVerticesButton_clicked() {
    int index = ui->sectorList->currentRow();
    if (index < 0 || index >= currentMap.regions.size()) {
        QMessageBox::warning(this, "Error", "Selecciona un sector primero");
        return;
    }

    // Limpiar escena
    scene->clear();

    // Calcular escala y offset (igual que drawWLDMap)
    qreal minX = std::numeric_limits<qreal>::max();
    qreal minY = std::numeric_limits<qreal>::max();
    qreal maxX = std::numeric_limits<qreal>::min();
    qreal maxY = std::numeric_limits<qreal>::min();

    for (const ModernWall &wall : currentMap.walls) {
        if (wall.p1 < currentMap.points.size() && wall.p2 < currentMap.points.size()) {
            const ModernPoint &p1 = currentMap.points[wall.p1];
            const ModernPoint &p2 = currentMap.points[wall.p2];

            minX = std::min({minX, (qreal)p1.x, (qreal)p2.x});
            minY = std::min({minY, (qreal)p1.y, (qreal)p2.y});
            maxX = std::max({maxX, (qreal)p1.x, (qreal)p2.x});
            maxY = std::max({maxY, (qreal)p1.y, (qreal)p2.y});
        }
    }

    qreal mapWidth = maxX - minX;
    qreal mapHeight = maxY - minY;
    qreal targetSize = 250;
    qreal scale = targetSize / std::max(mapWidth, mapHeight);
    qreal offsetX = 400 - (minX * scale);
    qreal offsetY = 400 - (minY * scale);

    // Cuadrícula
    qreal gridSpacing = 50 * scale;
    if (gridSpacing < 20) gridSpacing = 20;
    if (gridSpacing > 100) gridSpacing = 100;

    for (qreal i = 0; i <= 900; i += gridSpacing) {
        scene->addLine(i, 0, i, 800, QPen(Qt::lightGray, 0.5));
        scene->addLine(0, i, 900, i, QPen(Qt::lightGray, 0.5));
    }

    // Dibujar todas las paredes con transparencia (guía visual)
    for (const ModernWall &wall : currentMap.walls) {
        if (wall.p1 < currentMap.points.size() && wall.p2 < currentMap.points.size()) {
            const ModernPoint &p1 = currentMap.points[wall.p1];
            const ModernPoint &p2 = currentMap.points[wall.p2];

            qreal x1 = (p1.x * scale) + offsetX;
            qreal y1 = (p1.y * scale) + offsetY;
            qreal x2 = (p2.x * scale) + offsetX;
            qreal y2 = (p2.y * scale) + offsetY;

            bool isSelectedSector = (wall.front_region == index || wall.back_region == index);

            if (isSelectedSector) {
                scene->addLine(x1, y1, x2, y2, QPen(Qt::green, 1));
            } else {
                scene->addLine(x1, y1, x2, y2, QPen(Qt::red, 1));
            }

            // Vértices pequeños como guía
            scene->addEllipse(x1-0.5, y1-0.5, 1, 1, QPen(QColor(139, 0, 0, 100), 1), QBrush(QColor(139, 0, 0, 100)));
            scene->addEllipse(x2-0.5, y2-0.5, 1, 1, QPen(QColor(139, 0, 0, 100), 1), QBrush(QColor(139, 0, 0, 100)));
        }
    }

    // Recolectar vértices del sector seleccionado
    std::set<int> sectorVertices;
    for (const ModernWall &wall : currentMap.walls) {
        if (wall.front_region == index || wall.back_region == index) {
            sectorVertices.insert(wall.p1);
            sectorVertices.insert(wall.p2);
        }
    }

    // Dibujar polígono del sector seleccionado
    QPolygonF polygon;
    for (int vertexIdx : sectorVertices) {
        if (vertexIdx < currentMap.points.size()) {
            qreal x = (currentMap.points[vertexIdx].x * scale) + offsetX;
            qreal y = (currentMap.points[vertexIdx].y * scale) + offsetY;
            polygon << QPointF(x, y);
        }
    }
    scene->addPolygon(polygon, QPen(Qt::blue, 2), QBrush(QColor(100, 100, 255, 100)));

    // Dibujar vértices editables del sector seleccionado
    for (int vertexIdx : sectorVertices) {
        if (vertexIdx < currentMap.points.size()) {
            qreal x = (currentMap.points[vertexIdx].x * scale) + offsetX;
            qreal y = (currentMap.points[vertexIdx].y * scale) + offsetY;

            VertexItem *vertexItem = new VertexItem(index, vertexIdx);
            vertexItem->setRect(x - 0.5, y - 0.5, 1, 1);
            vertexItem->setPen(QPen(Qt::red, 2));
            vertexItem->setBrush(QBrush(Qt::red));

            scene->addItem(vertexItem);
            connect(vertexItem, &VertexItem::vertexMoved,
                    this, &MainWindow::onVertexMoved);
        }
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
        // Calcular escala y offset (igual que en drawWLDMap)
        qreal minX = std::numeric_limits<qreal>::max();
        qreal minY = std::numeric_limits<qreal>::max();
        qreal maxX = std::numeric_limits<qreal>::min();
        qreal maxY = std::numeric_limits<qreal>::min();

        for (const ModernWall &wall : currentMap.walls) {
            if (wall.p1 < currentMap.points.size() && wall.p2 < currentMap.points.size()) {
                const ModernPoint &p1 = currentMap.points[wall.p1];
                const ModernPoint &p2 = currentMap.points[wall.p2];

                minX = std::min({minX, (qreal)p1.x, (qreal)p2.x});
                minY = std::min({minY, (qreal)p1.y, (qreal)p2.y});
                maxX = std::max({maxX, (qreal)p1.x, (qreal)p2.x});
                maxY = std::max({maxY, (qreal)p1.y, (qreal)p2.y});
            }
        }

        qreal mapWidth = maxX - minX;
        qreal mapHeight = maxY - minY;
        qreal targetSize = 250;
        qreal scale = targetSize / std::max(mapWidth, mapHeight);
        qreal offsetX = 400 - (minX * scale);
        qreal offsetY = 400 - (minY * scale);

        // Convertir coordenadas de escena a coordenadas del mapa
        qreal mapX = (newPosition.x() - offsetX) / scale;
        qreal mapY = (newPosition.y() - offsetY) / scale;

        // Actualizar en currentMap.points
        if (vertexIndex >= 0 && vertexIndex < currentMap.points.size()) {
            currentMap.points[vertexIndex].x = mapX;
            currentMap.points[vertexIndex].y = mapY;
        }

        // Redibujar todo el mapa para mantener consistencia
        drawWLDMap(false);
        updateSectorList();
    }
}
void MainWindow::redrawVerticesOnly() {
    // Obtener todos los VertexItem en la escena
    QList<QGraphicsItem*> items = scene->items();

    for (QGraphicsItem* item : items) {
        VertexItem* vertex = qgraphicsitem_cast<VertexItem*>(item);
        if (vertex) {
            int vertexIdx = vertex->getVertexIndex();
            if (vertexIdx < currentMap.points.size()) {
                // Usar la misma lógica de escala que on_editVerticesButton_clicked
                qreal minX = std::numeric_limits<qreal>::max();
                qreal minY = std::numeric_limits<qreal>::max();
                qreal maxX = std::numeric_limits<qreal>::min();
                qreal maxY = std::numeric_limits<qreal>::min();

                for (const ModernWall &wall : currentMap.walls) {
                    if (wall.p1 < currentMap.points.size() && wall.p2 < currentMap.points.size()) {
                        const ModernPoint &p1 = currentMap.points[wall.p1];
                        const ModernPoint &p2 = currentMap.points[wall.p2];

                        minX = std::min({minX, (qreal)p1.x, (qreal)p2.x});
                        minY = std::min({minY, (qreal)p1.y, (qreal)p2.y});
                        maxX = std::max({maxX, (qreal)p1.x, (qreal)p2.x});
                        maxY = std::max({maxY, (qreal)p1.y, (qreal)p2.y});
                    }
                }

                qreal mapWidth = maxX - minX;
                qreal mapHeight = maxY - minY;
                qreal targetSize = 250;
                qreal scale = targetSize / std::max(mapWidth, mapHeight);
                qreal offsetX = 400 - (minX * scale);
                qreal offsetY = 400 - (minY * scale);

                qreal x = (currentMap.points[vertexIdx].x * scale) + offsetX;
                qreal y = (currentMap.points[vertexIdx].y * scale) + offsetY;

                vertex->setPos(x, y);
                vertex->setRect(x - 0.5, y - 0.5, 1, 1);
            }
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
                                    .arg(i)  // <-- Usar índice i
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

void MainWindow::drawWLDMap(bool adjustView) {
    scene->clear();

    // Si no hay mapa, inicializar valores por defecto como divmap3d
    if (currentMap.walls.empty()) {
        zoom_level = 0.0625;  // Valor inicial como en divmap3d
        scroll_x = 16384;     // FIN_GRID/2 = 30208/2 ≈ 15104
        scroll_y = 16384;     // FIN_GRID/2 = 30208/2 ≈ 15104
        scale = 1.0;

        // Dibujar solo grid
        for (int i = 0; i <= 800; i += 50) {
            scene->addLine(i, 0, i, 800, QPen(Qt::lightGray));
            scene->addLine(0, i, 800, i, QPen(Qt::lightGray));
        }
        return;
    }

    // Calcular bounds del mapa real
    qreal minX = std::numeric_limits<qreal>::max();
    qreal minY = std::numeric_limits<qreal>::max();
    qreal maxX = std::numeric_limits<qreal>::min();
    qreal maxY = std::numeric_limits<qreal>::min();

    for (const ModernWall &wall : currentMap.walls) {
        if (wall.p1 < currentMap.points.size() && wall.p2 < currentMap.points.size()) {
            const ModernPoint &p1 = currentMap.points[wall.p1];
            const ModernPoint &p2 = currentMap.points[wall.p2];

            minX = std::min({minX, (qreal)p1.x, (qreal)p2.x});
            minY = std::min({minY, (qreal)p1.y, (qreal)p2.y});
            maxX = std::max({maxX, (qreal)p1.x, (qreal)p2.x});
            maxY = std::max({maxY, (qreal)p1.y, (qreal)p2.y});
        }
    }

    // Calcular escala y offset
    qreal mapWidth = maxX - minX;
    qreal mapHeight = maxY - minY;
    qreal targetSize = 250;
    scale = targetSize / std::max(mapWidth, mapHeight);

    // IMPORTANTE: Centrar coordenadas como divmap3d
    // Calcular centro del mapa y usarlo como scroll
    scroll_x = (minX + maxX) / 2.0;
    scroll_y = (minY + maxY) / 2.0;
    zoom_level = scale;

    // Aplicar límites FIN_GRID como en divmap3d
    const int FIN_GRID = 32768 - 2560; // 30208
    if (scroll_x < 0) scroll_x = 0;
    if (scroll_y < 0) scroll_y = 0;
    if (scroll_x > FIN_GRID) scroll_x = FIN_GRID;
    if (scroll_y > FIN_GRID) scroll_y = FIN_GRID;

    qreal centerX = 400;
    qreal centerY = 400;
    qreal offsetX = centerX - (minX * scale);
    qreal offsetY = centerY - (minY * scale);

    // Cuadrícula extendida
    qreal gridSpacing = 50 * scale;
    if (gridSpacing < 20) gridSpacing = 20;
    if (gridSpacing > 100) gridSpacing = 100;

    for (qreal i = 0; i <= 900; i += gridSpacing) {
        scene->addLine(i, 0, i, 800, QPen(Qt::lightGray, 0.5));
        scene->addLine(0, i, 900, i, QPen(Qt::lightGray, 0.5));
    }

    // Dibujar paredes con colores según sector seleccionado
    for (const ModernWall &wall : currentMap.walls) {
        if (wall.p1 < currentMap.points.size() && wall.p2 < currentMap.points.size()) {
            const ModernPoint &p1 = currentMap.points[wall.p1];
            const ModernPoint &p2 = currentMap.points[wall.p2];

            qreal x1 = (p1.x * scale) + offsetX;
            qreal y1 = (p1.y * scale) + offsetY;
            qreal x2 = (p2.x * scale) + offsetX;
            qreal y2 = (p2.y * scale) + offsetY;

            bool isSelectedSector = (wall.front_region == selectedSectorIndex ||
                                     wall.back_region == selectedSectorIndex);

            if (isSelectedSector) {
                scene->addLine(x1, y1, x2, y2, QPen(Qt::green, 1));
            } else {
                scene->addLine(x1, y1, x2, y2, QPen(Qt::red, 1));
            }

            scene->addEllipse(x1-0.5, y1-0.5, 1, 1, QPen(Qt::darkRed, 1), QBrush(Qt::darkRed));
            scene->addEllipse(x2-0.5, y2-0.5, 1, 1, QPen(Qt::darkRed, 1), QBrush(Qt::darkRed));
        }
    }

    // Crear polígonos seleccionables para cada sector
    for (size_t sectorIdx = 0; sectorIdx < currentMap.regions.size(); sectorIdx++) {
        std::vector<QPointF> sectorPoints;

        for (const ModernWall &wall : currentMap.walls) {
            if (wall.front_region == sectorIdx) {
                if (wall.p1 < currentMap.points.size()) {
                    qreal x1 = (currentMap.points[wall.p1].x * scale) + offsetX;
                    qreal y1 = (currentMap.points[wall.p1].y * scale) + offsetY;
                    sectorPoints.push_back(QPointF(x1, y1));
                }
                if (wall.p2 < currentMap.points.size()) {
                    qreal x2 = (currentMap.points[wall.p2].x * scale) + offsetX;
                    qreal y2 = (currentMap.points[wall.p2].y * scale) + offsetY;
                    sectorPoints.push_back(QPointF(x2, y2));
                }
            }
        }

        // Crear polígono si hay suficientes puntos
        if (sectorPoints.size() >= 3) {
            QPolygonF polygon;
            for (const QPointF &point : sectorPoints) {
                polygon << point;
            }

            // Determinar color según selección
            bool isSelected = (sectorIdx == selectedSectorIndex);
            QPen pen(isSelected ? Qt::green : Qt::blue, isSelected ? 3 : 2);
            QBrush brush(QColor(isSelected ? 100 : 50, isSelected ? 255 : 100, isSelected ? 100 : 255, isSelected ? 120 : 80));

            // Crear item de polígono seleccionable
            QGraphicsPolygonItem *polygonItem = scene->addPolygon(polygon, pen, brush);
            polygonItem->setData(0, QVariant((int)sectorIdx));
            polygonItem->setFlag(QGraphicsItem::ItemIsSelectable, true);
        }
    }

    // Solo ajustar vista si se solicita explícitamente
    if (adjustView) {
        ui->mapView->fitInView(0, 0, 900, 800, Qt::KeepAspectRatio);
    }
}

void MainWindow::onSectorClicked(int sectorIndex) {
    if (sectorIndex >= 0 && sectorIndex < currentMap.regions.size()) {
        ui->sectorList->setCurrentRow(sectorIndex);
        selectedSectorIndex = sectorIndex;

        // Actualizar UI con datos del sector
        const ModernRegion &region = currentMap.regions[sectorIndex];
        ui->floorHeightSpin->setValue(region.floor_height);
        ui->ceilingHeightSpin->setValue(region.ceiling_height);
        updateTextureThumbnails();

        // Redibujar para mostrar selección
        drawWLDMap(false);
    }
}

void MainWindow::assignRegionsAndPortals() {
    // Ordenar regiones por profundidad primero
    sortRegionsByDepth();

    // Analizar cada pared para determinar portales
    for (int i = 0; i < currentMap.walls.size(); i++) {
        ModernWall &wall = currentMap.walls[i];

        // Inicializar como pared normal
        wall.back_region = -1;
        wall.type = 2; // Pared normal

        // Buscar paredes compartidas (portales potenciales)
        for (int j = i + 1; j < currentMap.walls.size(); j++) {
            if (wallsShareVertices(wall, currentMap.walls[j])) {
                if (wallsHaveOppositeOrientation(wall, currentMap.walls[j])) {
                    // Es un portal - asignar back_region
                    wall.back_region = currentMap.walls[j].front_region;
                    wall.type = 1; // Portal

                    // Configurar texturas para portal
                    wall.texture_top = wall.texture;
                    wall.texture_bot = wall.texture;
                    wall.texture = 0; // Sin textura media en portales
                }
            }
        }
    }
}

bool MainWindow::wallsShareVertices(const ModernWall &w1, const ModernWall &w2) {
    return (w1.p1 == w2.p1 && w1.p2 == w2.p2) ||
           (w1.p1 == w2.p2 && w1.p2 == w2.p1);
}

bool MainWindow::wallsHaveOppositeOrientation(const ModernWall &w1, const ModernWall &w2) {
    return w1.front_region != w2.front_region;
}

void MainWindow::sortRegionsByDepth() {
    if (currentMap.regions.size() <= 1) {
        return;
    }

    // Inicializar todas las regiones con type = 1 (nivel más externo)
    for (int i = 0; i < currentMap.regions.size(); i++) {
        currentMap.regions[i].type = 1;
    }

    // Calcular el número máximo de vértices por región
    int maxVertex = 0;
    std::vector<int> regionVertexCount(currentMap.regions.size(), 0);

    for (const ModernWall &wall : currentMap.walls) {
        if (wall.front_region >= 0 && wall.front_region < currentMap.regions.size()) {
            regionVertexCount[wall.front_region]++;
        }
    }

    for (int count : regionVertexCount) {
        if (count + 1 > maxVertex) {
            maxVertex = count + 1;
        }
    }

    // Crear polígonos para cada región
    std::vector<std::vector<QPointF>> regionPolygons(currentMap.regions.size());

    for (int regionIdx = 0; regionIdx < currentMap.regions.size(); regionIdx++) {
        std::vector<QPointF> vertices;

        for (const ModernWall &wall : currentMap.walls) {
            if (wall.front_region == regionIdx) {
                if (wall.p1 < currentMap.points.size()) {
                    vertices.push_back(QPointF(currentMap.points[wall.p1].x,
                                               currentMap.points[wall.p1].y));
                }
            }
        }

        regionPolygons[regionIdx] = vertices;
    }

    // Determinar profundidad de cada región
    for (int wallIdx = 0; wallIdx < currentMap.walls.size(); wallIdx++) {
        const ModernWall &wall = currentMap.walls[wallIdx];

        // Calcular punto medio de la pared
        if (wall.p1 < currentMap.points.size() && wall.p2 < currentMap.points.size()) {
            qreal midX = (currentMap.points[wall.p1].x + currentMap.points[wall.p2].x) / 2.0;
            qreal midY = (currentMap.points[wall.p1].y + currentMap.points[wall.p2].y) / 2.0;

            // Determinar en qué regiones está este punto medio
            int maxDepth = 1;
            for (int regionIdx = 0; regionIdx < currentMap.regions.size(); regionIdx++) {
                if (regionIdx != wall.front_region && isPointInRegion(midX, midY, regionPolygons[regionIdx])) {
                    if (currentMap.regions[regionIdx].type > maxDepth) {
                        maxDepth = currentMap.regions[regionIdx].type;
                    }
                }
            }

            // Actualizar profundidad de la región frontal
            if (maxDepth > currentMap.regions[wall.front_region].type) {
                currentMap.regions[wall.front_region].type = maxDepth;
            }
        }
    }
}

bool MainWindow::isPointInRegion(qreal x, qreal y, const std::vector<QPointF> &polygon) {
    if (polygon.size() < 3) return false;

    bool inside = false;
    int j = polygon.size() - 1;

    for (int i = 0; i < polygon.size(); i++) {
        const QPointF &pi = polygon[i];
        const QPointF &pj = polygon[j];

        if (((pi.y() > y) != (pj.y() > y)) &&
            (x < (pj.x() - pi.x()) * (y - pi.y()) / (pj.y() - pi.y()) + pi.x())) {
            inside = !inside;
        }
        j = i;
    }

    return inside;
}

MainWindow::~MainWindow()
{
    delete ui;
}
