#ifndef MAPSTRUCTURES_H
#define MAPSTRUCTURES_H

#include <QVector>
#include <QPointF>
#include <QString>
#include <cstdint>
#include <QPixmap>

// Estructura para un sector en el editor
struct EditorSector {
    uint32_t id;
    float floor_height;
    float ceiling_height;
    uint32_t floor_texture_id;
    uint32_t ceiling_texture_id;
    uint32_t wall_texture_id;
    QVector<QPointF> vertices;  // Polígono del sector

    EditorSector() : id(0), floor_height(0.0f), ceiling_height(150.0f),
        floor_texture_id(0), ceiling_texture_id(0), wall_texture_id(0) {}
};

// Estructura para una pared
struct EditorWall {
    uint32_t sector1_id;
    uint32_t sector2_id;
    uint32_t texture_id;
    QPointF p1, p2;  // Coordenadas de inicio y fin

    EditorWall() : sector1_id(0), sector2_id(0), texture_id(0) {}
};

// Entrada de textura
struct TextureEntry {
    QString filename;
    uint32_t id;
    QPixmap pixmap;

    TextureEntry() : id(0) {}
    TextureEntry(const QString &fname, uint32_t tid) : filename(fname), id(tid) {}
};

// Estructuras para formato .tex
typedef struct {
    char magic[4];
    uint32_t version;
    uint16_t num_images;
    uint8_t reserved[6];
} TEX_HEADER;

typedef struct {
    uint16_t index;
    uint16_t width;
    uint16_t height;
    uint16_t format;
    uint8_t reserved[250];
} TEX_ENTRY;
#endif // MAPSTRUCTURES_H
