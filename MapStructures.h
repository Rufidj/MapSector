#ifndef MAPSTRUCTURES_H
#define MAPSTRUCTURES_H

#include <QVector>
#include <QPointF>
#include <QString>
#include <cstdint>
#include <QPixmap>
#include <memory>
#include <vector>
#include <cstdio>
#include "divmap3d.hpp"

// Estructuras para formato FPG de BennuGD2
typedef struct {
    int code;          // Código del mapa
    int regsize;       // Tamaño del registro (normalmente 0)
    char name[32];     // Nombre del map
    char filename[12]; // Nombre del archivo
    int width;         // Ancho
    int height;        // Alto
    int flags;         // Flags (incluye cantidad de control points)
} FPG_CHUNK;

typedef struct {
    uint16_t x;
    uint16_t y;
} FPG_CONTROL_POINT;

// TextureEntry debe estar definido ANTES de ModernMap
struct TextureEntry {
    QString filename;
    uint32_t id;
    QPixmap pixmap;

    TextureEntry() : id(0) {}
    TextureEntry(const QString &fname, uint32_t tid) : filename(fname), id(tid) {}
};

// Envoltorio moderno de tpoint con manejo automático de memoria
class ModernPoint {
public:
    int32_t active = 0;
    int32_t x = 0, y = 0;
    int32_t links = 0;

    ModernPoint() = default;
    ModernPoint(int32_t px, int32_t py) : x(px), y(py), active(1), links(0) {}

    // Serialización compatible con divmap3d
    void writeToWLD(FILE* f) const {
        struct {
            int32_t active, x, y, links;
        } legacy = {active, x, y, links};
        fwrite(&legacy, sizeof(legacy), 1, f);
    }

    void readFromWLD(FILE* f) {
        struct {
            int32_t active, x, y, links;
        } legacy;
        fread(&legacy, sizeof(legacy), 1, f);
        active = legacy.active;
        x = legacy.x;
        y = legacy.y;
        links = legacy.links;
    }
};

// Región moderna con todos los campos necesarios
class ModernRegion {
public:
    int32_t active = 0;
    int32_t type = 0;
    int32_t floor_height = 0;
    int32_t ceiling_height = 0;
    int32_t floor_tex = 0;
    int32_t ceil_tex = 0;
    int32_t wall_tex = 0;      // Para texturas de paredes
    int32_t fade = 0;          // Valor de luz de 0 a 16

    std::vector<ModernPoint> points;

    ModernRegion() = default;

    // Serialización compatible con divmap3d
    void writeToWLD(FILE* f) const {
        struct {
            int32_t active, type, floor_height, ceiling_height;
            int32_t floor_tex, ceil_tex, fade;
        } legacy = {active, type, floor_height, ceiling_height, floor_tex, ceil_tex, fade};
        fwrite(&legacy, sizeof(legacy), 1, f);
    }

    void readFromWLD(FILE* f) {
        struct {
            int32_t active, type, floor_height, ceiling_height;
            int32_t floor_tex, ceil_tex, fade;
        } legacy;
        fread(&legacy, sizeof(legacy), 1, f);
        active = legacy.active;
        type = legacy.type;
        floor_height = legacy.floor_height;
        ceiling_height = legacy.ceiling_height;
        floor_tex = legacy.floor_tex;
        ceil_tex = legacy.ceil_tex;
        fade = legacy.fade;
    }
};

// Pared moderna con campos correctos
class ModernWall {
public:
    int32_t active = 0;
    int32_t type = 0;
    int32_t p1 = 0, p2 = 0;
    int32_t front_region = -1;  // Agregar este campo
    int32_t back_region = -1;   // Agregar este campo
    int32_t texture = 0;
    int32_t texture_top = 0;
    int32_t texture_bot = 0;
    int32_t fade = 0;

    ModernWall() = default;

    // Serialización compatible con divmap3d
    void writeToWLD(FILE* f) const {
        struct {
            int32_t active, type, p1, p2, front_region, back_region;
            int32_t texture, texture_top, texture_bot, fade;
        } legacy = {active, type, p1, p2, front_region, back_region,
                    texture, texture_top, texture_bot, fade};
        fwrite(&legacy, sizeof(legacy), 1, f);
    }

    void readFromWLD(FILE* f) {
        struct {
            int32_t active, type, p1, p2, front_region, back_region;
            int32_t texture, texture_top, texture_bot, fade;
        } legacy;
        fread(&legacy, sizeof(legacy), 1, f);
        active = legacy.active;
        type = legacy.type;
        p1 = legacy.p1;
        p2 = legacy.p2;
        front_region = legacy.front_region;
        back_region = legacy.back_region;
        texture = legacy.texture;
        texture_top = legacy.texture_top;
        texture_bot = legacy.texture_bot;
        fade = legacy.fade;
    }
};

// Mapa moderno con texturas y métodos WLD
class ModernMap {
public:
    std::vector<ModernPoint> points;
    std::vector<ModernRegion> regions;
    std::vector<ModernWall> walls;
    QVector<TextureEntry> textures;  // QVector para compatibilidad Qt

    void clear() {
        points.clear();
        regions.clear();
        walls.clear();
        textures.clear();
    }

    // Declaraciones de métodos WLD
    bool saveToWLD(const QString &filename);
    bool loadFromWLD(const QString &filename);
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
